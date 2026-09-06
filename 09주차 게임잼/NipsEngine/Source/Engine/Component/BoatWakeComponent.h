#pragma once

#include "Component/ActorComponent.h"

class UMaterialInterface;
class UDecalComponent;
class ADecalActor;
class UWorld;

class UBoatWakeComponent : public UActorComponent
{
public:
    DECLARE_CLASS(UBoatWakeComponent, UActorComponent)

    UBoatWakeComponent() = default;
    ~UBoatWakeComponent() override = default;

    void Serialize(FArchive& Ar) override;
    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
    void PostEditProperty(const char* PropertyName) override;
    void BeginPlay() override;
    void EndPlay() override;

protected:
    void TickComponent(float DeltaTime) override;

private:
    void RefreshMaterialRefs();
    bool ResolveBoatAxes(FVector& OutForward, FVector& OutRight) const;
    void ResetWakeProgress(const FVector& CurrentLocation);
    void SpawnWakeSet(
        const FVector& BoatLocation,
        const FVector& BoatForward,
        const FVector& BoatRight,
        float SpeedAlpha);
    void SpawnWakeDecal(
        const FVector& WorldLocation,
        const FVector& PlaneForward,
        const FVector& PlaneRight,
        const FVector& DecalSize,
        UMaterialInterface* Material,
        float FadeStartDelay,
        float FadeDuration);
    ADecalActor* CreateWakeDecalActor(UWorld* World) const;
    ADecalActor* AcquireWakeDecalActor(UWorld* World);
    UDecalComponent* ResolveWakeDecalComponent(ADecalActor* DecalActor) const;
    void DeactivateWakeDecal(ADecalActor* DecalActor) const;
    void ReleaseWakeDecalPool();

private:
    FString MainDecalMaterial = "BoatWakeDecalMain";

    // Wake decals are component-owned runtime effects; materials stay shared assets.
    float MinSpawnSpeed = 1.5f;
    float MaxWakeSpeed = 15.0f;
    float SpawnSpacing = 2.25f;
    int32 MaxActiveWakeDecals = 64;

    float MainWidth = 8.0f;
    float MainLength = 13.0f;
    float MainDepth = 1.0f;
    float MainBackwardOffset = 4.0f;
    float MainFadeStartDelay = 0.05f;
    float MainFadeDuration = 1.8f;
    FVector MainFadeOutSizeMultiplier = FVector(1.0f, 1.14f, 1.24f);

    float WaterHeightOffset = 0.05f;

    FVector LastOwnerLocation = FVector::ZeroVector;
    float DistanceAccumulator = 0.0f;
    bool bHasHistory = false;
    bool bWasMovingAboveSpawnSpeed = false;

    UMaterialInterface* MainDecalMaterialRef = nullptr;
    TArray<ADecalActor*> WakeDecalPool;
    int32 NextWakeDecalPoolIndex = 0;
};
