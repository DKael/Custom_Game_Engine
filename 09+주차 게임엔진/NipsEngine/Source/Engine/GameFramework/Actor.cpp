#include "GameFramework/Actor.h"
#include "Component/PrimitiveComponent.h"
#include "Component/ActorComponent.h"
#include "Component/Movement/MovementComponent.h"
#include "GameFramework/World.h"
#include "Math/Utils.h"

DEFINE_CLASS(AActor, UObject)
REGISTER_FACTORY(AActor)

AActor::~AActor()
{
    UnregisterOwnedComponents();

    if (OwningWorld != nullptr)
    {
        OwningWorld->GetSpatialIndex().UnregisterActor(this);
        OwningWorld = nullptr;
    }

    for (auto* Comp : OwnedComponents)
    {
        UObjectManager::Get().DestroyObject(Comp);
    }

    OwnedComponents.clear();
    RootComponent = nullptr;
}

/* 액터의 계층 구조를 복원하기 위해 자식들을 재귀적으로 순회합니다. */
static USceneComponent* DuplicateSubTree(
    USceneComponent* OriginalComp, AActor* NewActor, TArray<UActorComponent*>& OwnedComponents,
    TMap<USceneComponent*, USceneComponent*>& OutCompMap)
{
    if (!OriginalComp || OriginalComp->IsEditorOnly())
        return nullptr;

    // 현재 노드(부모) 복제
    USceneComponent* DuplicatedComp = Cast<USceneComponent>(OriginalComp->Duplicate());
    if (!DuplicatedComp)
        return nullptr; // 에디터 전용 등이면 nullptr 반환됨

    // 소유자 등록
    DuplicatedComp->SetOwner(NewActor);
    OwnedComponents.push_back(DuplicatedComp);

    // 원본 → 복제본 매핑 등록 (MovementComponent의 UpdatedComponent 연결에 사용)
    OutCompMap[OriginalComp] = DuplicatedComp;

    // 자식들을 재귀적으로 순회하며 방금 복제된 자신(DuplicatedComp)에게 바로 Attach
    for (USceneComponent* Child : OriginalComp->GetChildren())
    {
        USceneComponent* DuplicatedChild = DuplicateSubTree(Child, NewActor, OwnedComponents, OutCompMap);
        if (DuplicatedChild)
        {
            DuplicatedChild->AttachToComponent(DuplicatedComp);
        }
    }

    return DuplicatedComp;
}

void AActor::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    OutProps.push_back({ "Tag", EPropertyType::String, &ActorTag });
    OutProps.push_back({ "Visible", EPropertyType::Bool, &bVisible });
    OutProps.push_back({ "Active", EPropertyType::Bool, &bIsActive });
    OutProps.push_back({ "Tick In Editor", EPropertyType::Bool, &bTickInEditor });
    OutProps.push_back({ "Custom Time Dilation", EPropertyType::Float, &CustomTimeDilation, 0.0f, 8.0f, 0.01f });
    OutProps.push_back({ "Pending Location", EPropertyType::Vec3, &PendingActorLocation });
}

void AActor::PostDuplicate(UObject* Original)
{
    AActor* OrigActor = static_cast<AActor*>(Original);
    OwningWorld = nullptr;
    bHasBegunPlay = false;
    bComponentsRegisteredToWorld = false;
    CustomTimeDilation = OrigActor->CustomTimeDilation;
    OwnedComponents.clear();

    // MovementComponent 등 일반 컴포넌트들의 참조를 복원하기 위한 맵을 선언합니다.
    TMap<USceneComponent*, USceneComponent*> ComponentMap;

    // 1. RootComponent부터 씬 컴포넌트 트리 전체를 복제하여 조립합니다.
    if (OrigActor->RootComponent)
    {
        SetRootComponent(DuplicateSubTree(OrigActor->RootComponent, this, OwnedComponents, ComponentMap));
    }

    // 2. SceneComponent가 아닌 일반 컴포넌트들을 복제하기 위해 배열을 순회합니다.
    for (UActorComponent* OriginalComp : OrigActor->OwnedComponents)
    {
        if (!OriginalComp || OriginalComp->IsA<USceneComponent>() || OriginalComp->IsEditorOnly())
            continue;

        UActorComponent* DuplicatedComp = Cast<UActorComponent>(OriginalComp->Duplicate());
        if (!DuplicatedComp)
            continue;

        DuplicatedComp->SetOwner(this);
        OwnedComponents.push_back(DuplicatedComp);

        // MovementComponent도 기존에 제어하던 컴포넌트를 제어하도록 연결해줍니다.
        if (UMovementComponent* OriginalMoveComp = Cast<UMovementComponent>(OriginalComp))
        {
            UMovementComponent* DuplicatedMoveComp = Cast<UMovementComponent>(DuplicatedComp);
            USceneComponent* OldTarget = OriginalMoveComp->GetUpdatedComponent();

            if (OldTarget && ComponentMap.find(OldTarget) != ComponentMap.end())
            {
                DuplicatedMoveComp->SetUpdatedComponent(ComponentMap[OldTarget]);
            }
            else
            {
                DuplicatedMoveComp->SetUpdatedComponent(GetRootComponent());
            }
        }
    }

    // 월드 연결/등록은 호출자가 SetWorld()를 통해 명시적으로 수행합니다.

    bPrimitiveCacheDirty = true;
}

void AActor::Serialize(FArchive& Ar)
{
    Ar.BeginObject(std::to_string(GetUUID()));
    UObject::Serialize(Ar);
    Ar << "Tag" << ActorTag;
    Ar << "Visible" << bVisible;
    Ar << "Editor Only" << bTickInEditor;
    Ar << "Custom Time Dilation" << CustomTimeDilation;
    if (Ar.IsLoading())
    {
        ActorTag = ActorTags::Normalize(ActorTag);
        CustomTimeDilation = MathUtil::Clamp(CustomTimeDilation, 0.0f, 8.0f);
    }
    Ar.EndObject();

    for (UActorComponent* Comp : OwnedComponents)
    {
        if (Comp->IsTransient()) continue;
        Ar.BeginObject(std::to_string(Comp->GetUUID()));
        Comp->Serialize(Ar);
        Ar.EndObject();
    }
}

void AActor::EnsureDefaultComponentsInitialized()
{
    if (RootComponent != nullptr || !OwnedComponents.empty())
    {
        return;
    }

    InitDefaultComponents();
}

UActorComponent* AActor::AddComponentByClass(const FTypeInfo* Class)
{
    if (!Class)
        return nullptr;

    UObject* Obj = FObjectFactory::Get().Create(Class->name);
    if (!Obj)
        return nullptr;

    UActorComponent* Comp = Cast<UActorComponent>(Obj);
    if (!Comp)
    {
        UObjectManager::Get().DestroyObject(Obj);
        return nullptr;
    }

    Comp->SetOwner(this);
    OwnedComponents.push_back(Comp);
    bPrimitiveCacheDirty = true;
    if (OwningWorld != nullptr)
    {
        Comp->OnRegister();
        bComponentsRegisteredToWorld = true;
    }
    return Comp;
}

void AActor::RegisterComponent(UActorComponent* Comp)
{
    if (!Comp)
        return;

    auto it = std::find(OwnedComponents.begin(), OwnedComponents.end(), Comp);
    if (it == OwnedComponents.end())
    {
        Comp->SetOwner(this);
        OwnedComponents.push_back(Comp);
        bPrimitiveCacheDirty = true;
        if (OwningWorld != nullptr)
        {
            Comp->OnRegister();
            bComponentsRegisteredToWorld = true;
        }
    }
}

void AActor::RemoveComponent(UActorComponent* Component)
{
    if (!Component)
        return;

    if (bComponentsRegisteredToWorld)
    {
        Component->OnUnregister();
    }

    // 다른 컴포넌트들이 삭제될 컴포넌트를 참조하고 있다면 nullptr로 밀어줍니다.
    for (UActorComponent* Comp : OwnedComponents)
    {
        if (!Comp) continue;
        if (UMovementComponent* MoveComp = Cast<UMovementComponent>(Comp))
        {
            if (MoveComp->GetUpdatedComponent() == Component)
            {
                MoveComp->SetUpdatedComponent(nullptr);
            }
        }
    }

    auto it = std::find(OwnedComponents.begin(), OwnedComponents.end(), Component);
    if (it != OwnedComponents.end())
    {
        OwnedComponents.erase(it);
        bPrimitiveCacheDirty = true;
    }

    if (RootComponent == Component)
        RootComponent = nullptr;

    UObjectManager::Get().DestroyObject(Component);
}

// 자식 목록을 복사해 두고, 컴포넌트를 재귀적으로 삭제합니다.
void AActor::RemoveComponentWithChildren(USceneComponent* Comp)
{
    if (!Comp) return;
    TArray<USceneComponent*> Children = Comp->GetChildren();
    for (USceneComponent* Child : Children)
        RemoveComponentWithChildren(Child);
    RemoveComponent(Comp);
}

void AActor::SetVisible(bool Visible)
{
    if (bVisible == Visible)
    {
        return;
    }

    bVisible = Visible;
    MarkPrimitiveComponentsDirty();
}

void AActor::SetCustomTimeDilation(float InTimeDilation)
{
    CustomTimeDilation = MathUtil::Clamp(InTimeDilation, 0.0f, 8.0f);
}

void AActor::SetWorld(UWorld* World)
{
    if (OwningWorld == World)
    {
        return;
    }

    if (OwningWorld != nullptr)
    {
        UnregisterOwnedComponents();
        OwningWorld->GetSpatialIndex().UnregisterActor(this);
    }

    OwningWorld = World;

    if (OwningWorld != nullptr)
    {
        OwningWorld->GetSpatialIndex().RegisterActor(this);
        RegisterOwnedComponents();
    }
}

void AActor::SetRootComponent(USceneComponent* Comp)
{
    if (!Comp)
        return;
    RootComponent = Comp;
}

FVector AActor::GetActorLocation() const
{
    if (RootComponent)
    {
        return RootComponent->GetWorldLocation();
    }
    return FVector(0, 0, 0);
}

void AActor::SetActorLocation(const FVector& NewLocation)
{
    PendingActorLocation = NewLocation;

    if (RootComponent)
    {
        RootComponent->SetWorldLocation(NewLocation);
    }
}

void AActor::BeginPlay()
{
    if (bHasBegunPlay)
    {
        return;
    }

    bHasBegunPlay = true;
    BeginPlayOwnedComponents();
}

void AActor::Tick(float DeltaTime)
{
    if (bPendingDestroy || bBeingDestroyed)
    {
        return;
    }

    const float ActorDeltaTime = DeltaTime * CustomTimeDilation;
    for (UActorComponent* Component : OwnedComponents)
    {
        if (Component && Component->IsActive())
        {
            Component->ExecuteTick(ActorDeltaTime);
            if (bPendingDestroy || bBeingDestroyed)
            {
                break;
            }
        }
    }
}

void AActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (!bHasBegunPlay)
    {
        return;
    }

    bHasBegunPlay = false;
    EndPlayOwnedComponents();
}

void AActor::TeardownForDestroy(const EEndPlayReason::Type EndPlayReason)
{
    EndPlay(EndPlayReason);
    UnregisterOwnedComponents();
}

void AActor::BeginPlayOwnedComponents()
{
    for (UActorComponent* Component : OwnedComponents)
    {
        if (Component)
        {
            Component->BeginPlay();
        }
    }
}

void AActor::EndPlayOwnedComponents()
{
    const TArray<UActorComponent*>& Components = OwnedComponents;
    for (int32 i = static_cast<int32>(Components.size()) - 1; i >= 0; --i)
    {
        if (Components[i])
        {
            Components[i]->EndPlay();
        }
    }
}

void AActor::RegisterOwnedComponents()
{
    if (OwningWorld == nullptr || bComponentsRegisteredToWorld)
    {
        return;
    }

    for (UActorComponent* Component : OwnedComponents)
    {
        if (Component)
        {
            Component->OnRegister();
        }
    }

    bComponentsRegisteredToWorld = true;
}

void AActor::UnregisterOwnedComponents()
{
    if (!bComponentsRegisteredToWorld)
    {
        return;
    }

    const TArray<UActorComponent*>& Components = OwnedComponents;
    for (int32 i = static_cast<int32>(Components.size()) - 1; i >= 0; --i)
    {
        if (Components[i])
        {
            Components[i]->OnUnregister();
        }
    }

    bComponentsRegisteredToWorld = false;
}

void AActor::Destroy()
{
    if (bPendingDestroy || bBeingDestroyed)
    {
        return;
    }

    if (OwningWorld != nullptr)
    {
        OwningWorld->RequestDestroyActor(this);
    }
    else
    {
        bPendingDestroy = true;
    }
}


void AActor::MarkPrimitiveComponentsDirty()
{
    if (OwningWorld == nullptr)
    {
        return;
    }

    for (UPrimitiveComponent* Primitive : GetPrimitiveComponents())
    {
        OwningWorld->GetSpatialIndex().MarkPrimitiveDirty(Primitive);
    }
}

const TArray<UPrimitiveComponent*>& AActor::GetPrimitiveComponents() const
{
    if (bPrimitiveCacheDirty)
    {
        PrimitiveCache.clear();
        for (UActorComponent* Comp : OwnedComponents)
        {
            if (Comp && Comp->IsA<UPrimitiveComponent>())
            {
                PrimitiveCache.emplace_back(static_cast<UPrimitiveComponent*>(Comp));
            }
        }
        bPrimitiveCacheDirty = false;
    }
    return PrimitiveCache;
}

bool AActor::IsOverlappingActor(const AActor* Other) const
{
    if (!Other)
    {
        return false;
    }

    for (UActorComponent* OwnedComp : OwnedComponents)
    {
        UShapeComponent* ShapeComp = Cast<UShapeComponent>(OwnedComp);
        if (!ShapeComp)
        {
            continue;
        }

        if (!ShapeComp->GetOverlapInfos().empty() &&
            ShapeComp->IsOverlappingActor(Other))
        {
            return true;
        }
    }

    return false;
}
