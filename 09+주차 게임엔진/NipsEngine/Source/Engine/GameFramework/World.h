#pragma once
#include "Object/Object.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Camera/PlayerCameraManager.h"
#include "GameFramework/Camera/WorldCameraInterface.h"
#include "DriftSalvage/CollectionSystem.h"
#include "DriftSalvage/ExplosionSystem.h"
#include "Level.h"
#include "Spatial/WorldSpatialIndex.h"
#include "Collision/CollisionSystem.h"
#include "Core/TimeManager.h"

class UCameraComponent;
class ULineBatchComponent;
class FViewportCamera;
class ULightComponentBase;

/**
 * 원래는 데이터 복사본을 넣고 Dirty 여부에 따라 업데이트 해줘야 하지만
 * SceneProxy 개념 도입 전이므로 아래와 같이 포인터를 통해 접근
 * Pointer 에 대한 안전 체크는 Slot 을 통해 처리
 */
struct FLightSlot
{
    ULightComponentBase* LightData = nullptr;
    uint32 Generation = 0;
    bool bAlive = false;
};

struct FLightHandle
{
    uint32 Index = 0xFFFFFFFF;
    // Invalidate 검증용
    uint32 Generation = 0;

    bool IsValid() const { return Index != 0xFFFFFFFF; }
};

class UWorld : public UObject {
public:
    DECLARE_CLASS(UWorld, UObject)
    UWorld();
    ~UWorld() override;

    virtual void PostDuplicate(UObject* Original) override;

    // 프로퍼티 시스템 — UObject 에서 상속
    // UWorld 는 현재 에디터에 노출할 스칼라 프로퍼티가 없습니다.
    // (PersistentLevel 은 PostDuplicate 에서 별도 처리)
    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override {}
    void PostEditProperty(const char* PropertyName) override {}


    // Actor lifecycle
    template<typename T>
    T* SpawnActor()
    {
        // create and register an actor
        T* Actor = UObjectManager::Get().CreateObject<T>();
        Actor->EnsureDefaultComponentsInitialized();
        Actor->SetWorld(this);
        if (bHasBegunPlay)
        {
            Actor->BeginPlay();
        }
        PersistentLevel->AddActor(Actor);
        SpatialIndex.FlushDirtyBounds();
        return Actor;
    }

    void DestroyActor(AActor* Actor);
    void RequestDestroyActor(AActor* Actor);
    void FlushPendingDestroyActors();

    TArray<AActor*> GetActors() const { return PersistentLevel->GetActors(); }

    ULevel* GetPersistentLevel() const { return PersistentLevel; }

    void BeginPlay();      // Triggers BeginPlay on all actors
    void Tick(float DeltaTime);  // Drives the game loop every frame
    void EndPlay(EEndPlayReason::Type EndPlayReason); // Cleanup before world is destroyed
    void PrepareFrame(float UnscaledDeltaTime);

    /** @brief Rebuild the world BVH and bounds snapshot from all current primitives. */
    void RebuildSpatialIndex();

    /** @brief Flush pending bounds and visibility dirties into the world BVH. */
    void SyncSpatialIndex();

    bool HasBegunPlay() const { return bHasBegunPlay; }

    // Active Camera — EditorViewportClient 또는 PlayerController가 세팅e
    void SetActiveCamera(FViewportCamera* InCamera) { ActiveCamera = InCamera; }
    FViewportCamera* GetActiveCamera() const { return ActiveCamera; }
    FPlayerCameraManager& GetPlayerCameraManager() { return PlayerCameraManager; }
    const FPlayerCameraManager& GetPlayerCameraManager() const { return PlayerCameraManager; }
    // High-level façade used by Lua/gameplay for camera + hit-feel commands.
    FWorldCameraInterface& GetCameraInterface() { return CameraInterface; }
    const FWorldCameraInterface& GetCameraInterface() const { return CameraInterface; }

    float GetUnscaledDeltaTime() const { return TimeManager.GetUnscaledDeltaTime(); }
    float GetScaledDeltaTime() const { return TimeManager.GetScaledDeltaTime(); }
    float GetGlobalTimeDilation() const { return TimeManager.GetGlobalTimeDilation(); }
    void SetBaseTimeDilation(float InTimeDilation);
    void StartHitStop(float Duration, float TimeScale = 0.05f);
    void StartSlomo(float TimeScale, float Duration);
    void StopSlomo();

    /** @brief Access the world-level primitive AABB/BVH manager. */
    FWorldSpatialIndex& GetSpatialIndex() { return SpatialIndex; }

    /** @brief Access the world-level primitive AABB/BVH manager. */
    const FWorldSpatialIndex& GetSpatialIndex() const { return SpatialIndex; }

    EWorldType GetWorldType() const { return WorldType; }
    void SetWorldType(EWorldType InWorldType) { WorldType = InWorldType; }
    
    FLightHandle RegisterLight(ULightComponentBase* Comp);
    void UnregisterLight(ULightComponentBase* Comp);
    const TArray<FLightSlot>& GetWorldLightSlots() const { return WorldLightSlots; }
    
    FCollisionSystem& GetCollisionSystem() { return CollisionSystem; }
    const FCollisionSystem& GetCollisionSystem() const { return CollisionSystem; }

    FCollectionSystem& GetCollectionSystem() { return CollectionSystem; }
    const FCollectionSystem& GetCollectionSystem() const { return CollectionSystem; }

    FExplosionSystem& GetExplosionSystem() { return ExplosionSystem; }
    const FExplosionSystem& GetExplosionSystem() const { return ExplosionSystem; }
    
private:
    EWorldType WorldType = EWorldType::Editor;
    ULevel* PersistentLevel = nullptr;
    FViewportCamera* ActiveCamera = nullptr;
    FWorldSpatialIndex SpatialIndex;
    bool bHasBegunPlay = false;

    TArray<FLightSlot> WorldLightSlots;
    TArray<uint32> FreeLightSlotList;  // 삭제된 Light 의 Index 만 Free 로 등록
    TArray<AActor*> PendingDestroyActors;
    FCollisionSystem CollisionSystem;
    FCollectionSystem CollectionSystem;
    FExplosionSystem ExplosionSystem;
    bool bIsIteratingLevelActors = false;
    FPlayerCameraManager PlayerCameraManager;
    // Kept as a separate façade so scripting code avoids depending on internal managers.
    FWorldCameraInterface CameraInterface;

    FTimeManager TimeManager;
};
