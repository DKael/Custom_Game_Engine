#include "GameFramework/World.h"

#include "Collision/CollisionSystem.h"
#include "Component/Light/LightComponent.h"

#include "Engine/Core/SoundManager.h"

#include <algorithm>


DEFINE_CLASS(UWorld, UObject)
REGISTER_FACTORY(UWorld)

namespace
{
    class FScopedLevelActorIteration
    {
    public:
        explicit FScopedLevelActorIteration(bool& InFlag)
            : Flag(InFlag)
        {
            Flag = true;
        }

        ~FScopedLevelActorIteration()
        {
            Flag = false;
        }

    private:
        bool& Flag;
    };
}

// FName, UUID 발급, 메모리 추적 등을 위해 UObjectManager를 통해 생성, 삭제한다.
UWorld::UWorld()
{
    PersistentLevel = UObjectManager::Get().CreateObject<ULevel>();
    // Camera manager needs world access for target resolution and defaults.
    PlayerCameraManager.SetOwnerWorld(this);
    // Interface is the script/gameplay-facing façade; bind it to this world once.
    CameraInterface.SetOwnerWorld(this);
}

// 소멸 역시 UObjectManager를 통해 처리한다.
UWorld::~UWorld()
{
    SpatialIndex.Clear();
    UObjectManager::Get().DestroyObject(PersistentLevel);
}

/* @brief 비노출 필드를 복사하고, Level을 깊은 복사한 뒤, 복제된 액터들의 소속을 자기 자신으로 재설정합니다. */
void UWorld::PostDuplicate(UObject* Original)
{
    // UWorld 생성자가 기본 PersistentLevel을 생성하므로,
    // 원본의 레벨로 교체하기 전에 먼저 해제합니다.
    if (PersistentLevel)
    {
        UObjectManager::Get().DestroyObject(PersistentLevel);
        PersistentLevel = nullptr;
    }

    const UWorld* OrigWorld = Cast<UWorld>(Original);

    // 프로퍼티 시스템에 노출되지 않은 필드를 직접 복사합니다.
    WorldType      = OrigWorld->WorldType;
    ActiveCamera   = nullptr;
    bHasBegunPlay  = false; // 항상 미시작 상태로 시작
    TimeManager.Reset();
    PlayerCameraManager.SetOwnerWorld(this);
    PlayerCameraManager.Reset();
    // Rebind façade because duplicated world instance has different address/ownership.
    CameraInterface.SetOwnerWorld(this);
    CameraInterface.Reset();

    // PersistentLevel 을 깊은 복사한 뒤, 복제된 액터들의 소속을 새 월드로 재설정합니다.
    if (OrigWorld->PersistentLevel)
    {
        PersistentLevel = Cast<ULevel>(OrigWorld->PersistentLevel->Duplicate());
        for (AActor* DuplicatedActor : PersistentLevel->GetActors())
        {
            if (!DuplicatedActor) continue;
            DuplicatedActor->SetWorld(this);
        }
    }

    RebuildSpatialIndex();
}

void UWorld::BeginPlay()
{
    bHasBegunPlay = true;
    TimeManager.Reset();
    PlayerCameraManager.SetOwnerWorld(this);
    PlayerCameraManager.Reset();
    // Keep façade binding deterministic across play sessions.
    CameraInterface.SetOwnerWorld(this);
    CameraInterface.Reset();
    CollisionSystem.Reset();
    CollectionSystem.Reset();
    ExplosionSystem.Reset();
    {
        FScopedLevelActorIteration ScopedLevelActorIteration(bIsIteratingLevelActors);
        PersistentLevel->BeginPlay();
    }
    RebuildSpatialIndex();

}

void UWorld::PrepareFrame(float UnscaledDeltaTime)
{   
    TimeManager.PrepareFrame(UnscaledDeltaTime);
}

void UWorld::Tick(float DeltaTime)
{
    if (!PersistentLevel)
        return;

    {
        FScopedLevelActorIteration ScopedLevelActorIteration(bIsIteratingLevelActors);
        if (WorldType == EWorldType::Editor)
            PersistentLevel->TickEditor(DeltaTime);
        else
            PersistentLevel->TickGame(DeltaTime);
    }

    if (WorldType == EWorldType::PIE || WorldType == EWorldType::Game)
    {
        SyncSpatialIndex();
        {
            FScopedLevelActorIteration ScopedLevelActorIteration(bIsIteratingLevelActors);
            CollisionSystem.Tick(this, DeltaTime);
            ExplosionSystem.Tick(this, DeltaTime);
            CollectionSystem.Tick(this, DeltaTime);
        }
    }
    FlushPendingDestroyActors();
    SyncSpatialIndex();
}

void UWorld::EndPlay(EEndPlayReason::Type EndPlayReason)
{
    if (bHasBegunPlay)
    {
        bHasBegunPlay = false;
        FScopedLevelActorIteration ScopedLevelActorIteration(bIsIteratingLevelActors);
        PersistentLevel->EndPlay(EndPlayReason);
    }
    //
	///* test
	FSoundManager::Get().StopBGM();
	//*/
    CollisionSystem.Reset();
    CollectionSystem.Reset();
    ExplosionSystem.Reset();
    PlayerCameraManager.Reset();
    // Symmetric lifecycle reset for façade entry point.
    CameraInterface.Reset();
    TimeManager.Reset();
}

void UWorld::RebuildSpatialIndex()
{
    SpatialIndex.Rebuild(this);
}

void UWorld::SyncSpatialIndex()
{
    SpatialIndex.FlushDirtyBounds();
}

void UWorld::DestroyActor(AActor* Actor) 
{
    if (!Actor || !UObject::IsValid(Actor) || Actor->IsBeingDestroyed()) return;

    if (bIsIteratingLevelActors)
    {
        RequestDestroyActor(Actor);
        return;
    }

    Actor->MarkBeingDestroyed();
    Actor->TeardownForDestroy(EEndPlayReason::Type::Destroyed);
    
    PersistentLevel->RemoveActor(Actor);
    Actor->SetWorld(nullptr);
    UObjectManager::Get().DestroyObject(Actor);
}

void UWorld::RequestDestroyActor(AActor* Actor)
{
    if (!Actor || !UObject::IsValid(Actor) || Actor->IsPendingDestroy() || Actor->IsBeingDestroyed())
    {
        return;
    }

    Actor->MarkPendingDestroy();
    PendingDestroyActors.push_back(Actor);
}

void UWorld::FlushPendingDestroyActors()
{
    if (PendingDestroyActors.empty())
    {
        return;
    }

    TArray<AActor*> ActorsToDestroy = PendingDestroyActors;
    PendingDestroyActors.clear();

    for (AActor* Actor : ActorsToDestroy)
    {
        if (!Actor || !UObject::IsValid(Actor))
        {
            continue;
        }

        DestroyActor(Actor);
    }

    if (PersistentLevel)
    {
        PersistentLevel->RemovePendingDestroyActors();
    }
}

FLightHandle UWorld::RegisterLight(ULightComponentBase* Comp)
{
    FLightHandle LightHandle;
    FLightSlot LightSlot;

    if (FreeLightSlotList.empty())
    {
        // 새로 생성
        uint32 Index = static_cast<uint32>(WorldLightSlots.size());
        LightSlot.LightData = Comp;
        LightSlot.Generation = 0;
        LightSlot.bAlive = true;

        WorldLightSlots.push_back(LightSlot);

        LightHandle.Index = Index;
        LightHandle.Generation = WorldLightSlots[Index].Generation;
    }
    else
    {
        // Free Slot 사용
        uint32 Index = FreeLightSlotList.back();
        FreeLightSlotList.pop_back();
        WorldLightSlots[Index].Generation += 1;
        WorldLightSlots[Index].LightData = Comp;
        WorldLightSlots[Index].bAlive = true;

        LightHandle.Index = Index;
        LightHandle.Generation = WorldLightSlots[Index].Generation;
    }

    Comp->SetLightHandle(LightHandle);

    return LightHandle;
}

void UWorld::UnregisterLight(ULightComponentBase* Comp)
{
    FLightHandle LightHandle = Comp->GetLightHandle();
    // LightHandle이 없거나, 해당 Slot에 다른 데이터가 들어가 있으면 등록 해제 취소
    if (!LightHandle.IsValid() || WorldLightSlots[LightHandle.Index].Generation != LightHandle.Generation)
    {
        return;
    }

    WorldLightSlots[LightHandle.Index].bAlive = false;
    WorldLightSlots[LightHandle.Index].LightData = nullptr;
    FreeLightSlotList.push_back(LightHandle.Index);
}

void UWorld::SetBaseTimeDilation(float InTimeDilation)
{
    TimeManager.SetBaseTimeDilation(InTimeDilation);
}

void UWorld::StartHitStop(float Duration, float TimeScale)
{
    TimeManager.StartHitStop(Duration, TimeScale);
}

void UWorld::StartSlomo(float TimeScale, float Duration)
{
    TimeManager.StartSlomo(TimeScale, Duration);
}

void UWorld::StopSlomo()
{
    TimeManager.StopSlomo();
}