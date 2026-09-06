#pragma once
#include "Object/Object.h"
#include "Object/ObjectFactory.h"
#include "Component/SceneComponent.h"
#include "Core/ActorTags.h"
#include "Core/ActorTagRegistry.h"
#include "Engine/GameFramework/WorldContext.h"

#include <type_traits>

class UWorld;
class UPrimitiveComponent;

class AActor : public UObject {
public:
    DECLARE_CLASS(AActor, UObject)
    AActor() = default;
    ~AActor() override;

    virtual void PostDuplicate(UObject* Original) override;

    virtual void Serialize(FArchive& Ar) override;

    virtual void InitDefaultComponents() {}
    void EnsureDefaultComponentsInitialized();

    // 컴포넌트 생성 + Owner 설정을 수행합니다. 월드에 붙어 있으면 즉시 등록됩니다.
    template<typename T>
    T* AddComponent() {
        static_assert(std::is_base_of_v<UActorComponent, T>,
            "AddComponent<T>: T must derive from UActorComponent");

        T* Comp = UObjectManager::Get().CreateObject<T>();

        bPrimitiveCacheDirty = true;

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

    // Gameplay lifecycle (BeginPlay/EndPlay) is separate from world registration.
    virtual void BeginPlay();
    virtual void Tick(float DeltaTime);
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason);
    // Destruction lifecycle entrypoint used by UWorld::DestroyActor.
    void TeardownForDestroy(const EEndPlayReason::Type EndPlayReason);
    virtual void Destroy();
    bool IsPendingDestroy() const { return bPendingDestroy; }
    bool IsBeingDestroyed() const { return bBeingDestroyed; }

    bool IsActive() const { return bIsActive ; }
    void SetActive(bool bEnabled) { bIsActive = bEnabled; }
    float GetCustomTimeDilation() const { return CustomTimeDilation; }
    void SetCustomTimeDilation(float InTimeDilation);

    const FString& GetTag() const { return ActorTag; }
    void SetTag(const FString& InTag)
    {
        ActorTag = ActorTags::Normalize(InTag);
        ActorTagRegistry::RegisterUsedTag(ActorTag);
    }
    bool CompareTag(const FString& InTag) const { return ActorTag == InTag; }

    bool ShouldTickInEditor() const { return bTickInEditor; }
    void SetTickInEditor(bool bEnabled)  { bTickInEditor = bEnabled; }

    // FTypeInfo 기반 런타임 컴포넌트 생성
    UActorComponent* AddComponentByClass(const FTypeInfo* Class);
    void RemoveComponent(UActorComponent* Component);
    void RemoveComponentWithChildren(USceneComponent* Comp);
    void RegisterComponent(UActorComponent* Comp);

    void SetRootComponent(USceneComponent* Comp);
    USceneComponent* GetRootComponent() const { return RootComponent; }

    const TArray<UActorComponent*>& GetComponents() const { return OwnedComponents; }

    // Transform — Location
    FVector GetActorLocation() const;
    void SetActorLocation(const FVector& Location);
    void AddActorWorldOffset(const FVector& Delta)
    {
        if (RootComponent) RootComponent->AddWorldOffset(Delta);
    }

    // Transform — Rotation
    FVector GetActorRotation() const
    {
        return RootComponent ? RootComponent->GetRelativeRotation() : FVector(0, 0, 0);
    }
    void SetActorRotation(const FVector& NewRotation)
    {
        if (RootComponent) RootComponent->SetRelativeRotation(NewRotation);
    }

    // Transform — Scale
    FVector GetActorScale() const
    {
        return RootComponent ? RootComponent->GetRelativeScale() : FVector(1, 1, 1);
    }
    void SetActorScale(const FVector& NewScale)
    {
        if (RootComponent) RootComponent->SetRelativeScale(NewScale);
    }

    // Direction
    FVector GetActorForward() const
    {
        if (RootComponent)
            return RootComponent->GetForwardVector();
        return FVector(0, 0, 1);
    }

    void SetWorld(UWorld* World);
    UWorld* GetFocusedWorld() const { return OwningWorld; }

    bool IsVisible() const { return bVisible; }
    void SetVisible(bool Visible);

    // 프로퍼티 시스템 — UObject 에서 상속
    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
    void PostEditProperty(const char* PropertyName) override {}

    const TArray<UPrimitiveComponent*>& GetPrimitiveComponents() const;
    void MarkPendingDestroy() { bPendingDestroy = true; }
    void MarkBeingDestroyed() { bBeingDestroyed = true; }
    bool IsOverlappingActor(const AActor* Other) const;

protected:
    void BeginPlayOwnedComponents();
    void EndPlayOwnedComponents();
    void RegisterOwnedComponents();
    void UnregisterOwnedComponents();
    void MarkPrimitiveComponentsDirty();

    USceneComponent* RootComponent = nullptr;
    UWorld* OwningWorld = nullptr;

    FVector PendingActorLocation = FVector(0, 0, 0);

    bool bVisible = true;
    bool bIsActive = true;
    bool bTickInEditor = false;
    float CustomTimeDilation = 1.0f;
    FString ActorTag = ActorTags::Untagged;
    bool bPendingDestroy = false;
    bool bBeingDestroyed = false;
    bool bHasBegunPlay = false;
    bool bComponentsRegisteredToWorld = false;

    TArray<UActorComponent*> OwnedComponents;

    // 렌더링용 캐시
    mutable TArray<UPrimitiveComponent*> PrimitiveCache;
    mutable bool bPrimitiveCacheDirty = true;
};
