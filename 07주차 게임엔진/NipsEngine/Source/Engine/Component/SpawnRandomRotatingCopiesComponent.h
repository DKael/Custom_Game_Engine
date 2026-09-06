#pragma once

#include "ActorComponent.h"
#include "Math/Vector.h"

#include <random>

class AActor;
class URotationMovementComponent;
class UWorld;

class USpawnRandomRotatingCopiesComponent : public UActorComponent
{
public:
    DECLARE_CLASS(USpawnRandomRotatingCopiesComponent, UActorComponent)

    void BeginPlay() override;
    void EndPlay() override;

    virtual USpawnRandomRotatingCopiesComponent* Duplicate() override;
    virtual void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;

    void ExecuteSpawn();
    void CleanupSpawnedActors();

    bool HasExecuted() const { return bHasExecuted; }
    const TArray<AActor*>& GetSpawnedActors() const { return SpawnedActors; }

private:
    bool CanExecuteInCurrentWorld() const;
    uint32 ResolveRandomSeed() const;

    FVector MakeRandomUnitAxis(std::mt19937& Rng) const;
    float MakeRandomRotationSpeed(std::mt19937& Rng) const;
    FVector MakeRandomSpawnOffset(std::mt19937& Rng, const TArray<FVector>& ExistingOffsets) const;
    bool IsTooCloseToExistingOffset(const FVector& CandidateOffset, const TArray<FVector>& ExistingOffsets) const;

    AActor* SpawnDuplicateActor(
        AActor* TemplateActor,
        UWorld* World,
        const FVector& SpawnLocation,
        const FVector& RotationAxis,
        float RotationSpeedDegreesPerSecond);

    void RemoveSpawnerComponentsFromActor(AActor* Actor) const;
    void ConfigureRotationComponent(
        URotationMovementComponent* RotationComponent,
        const FVector& RotationAxis,
        float RotationSpeedDegreesPerSecond) const;

    void ApplyOriginalActorOptions(std::mt19937& Rng);
    void RestoreOriginalActorVisibility();

private:
    int32 SpawnCount = 12;
    float SpawnRadius = 5.0f;
    float MinRotationSpeed = 25.0f;
    float MaxRotationSpeed = 120.0f;

    bool bAffectOriginalActor = false;
    bool bHideOriginalActor = false;
    int32 RandomSeed = 0;
    bool bSpawnOnBeginPlay = true;
    bool bScatterIn3D = false;

    bool bHasExecuted = false;
    bool bOriginalVisibilitySaved = false;
    bool bOriginalVisibilityBeforeHide = true;

    TArray<AActor*> SpawnedActors;
};
