#include "SpawnRandomRotatingCopiesComponent.h"

#include "GameFramework/AActor.h"
#include "GameFramework/Level.h"
#include "GameFramework/World.h"
#include "Math/Utils.h"
#include "Object/ObjectFactory.h"
#include "RotationMovementComponent.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>

DEFINE_CLASS(USpawnRandomRotatingCopiesComponent, UActorComponent)
REGISTER_FACTORY(USpawnRandomRotatingCopiesComponent)

namespace
{
    float RandomFloat(std::mt19937& Rng, float Min, float Max)
    {
        std::uniform_real_distribution<float> Dist(Min, Max);
        return Dist(Rng);
    }

    bool RandomBool(std::mt19937& Rng)
    {
        std::uniform_int_distribution<int32> Dist(0, 1);
        return Dist(Rng) != 0;
    }

    int32 ClampSpawnCount(int32 Count)
    {
        return std::max<int32>(0, Count);
    }
}

void USpawnRandomRotatingCopiesComponent::BeginPlay()
{
    UActorComponent::BeginPlay();

    if (bSpawnOnBeginPlay)
    {
        ExecuteSpawn();
    }
}

void USpawnRandomRotatingCopiesComponent::EndPlay()
{
    RestoreOriginalActorVisibility();

    // PIE owns these actors and will destroy them with the duplicated world.
    // CleanupSpawnedActors() is exposed for explicit early cleanup while PIE is running.
    SpawnedActors.clear();
    bHasExecuted = false;
}

USpawnRandomRotatingCopiesComponent* USpawnRandomRotatingCopiesComponent::Duplicate()
{
    USpawnRandomRotatingCopiesComponent* NewComp =
        UObjectManager::Get().CreateObject<USpawnRandomRotatingCopiesComponent>();

    NewComp->SetActive(this->IsActive());
    NewComp->SetOwner(nullptr);

    NewComp->SpawnCount = SpawnCount;
    NewComp->SpawnRadius = SpawnRadius;
    NewComp->MinRotationSpeed = MinRotationSpeed;
    NewComp->MaxRotationSpeed = MaxRotationSpeed;
    NewComp->bAffectOriginalActor = bAffectOriginalActor;
    NewComp->bHideOriginalActor = bHideOriginalActor;
    NewComp->RandomSeed = RandomSeed;
    NewComp->bSpawnOnBeginPlay = bSpawnOnBeginPlay;
    NewComp->bScatterIn3D = bScatterIn3D;

    // Runtime state is intentionally reset on duplicated editor/PIE copies.
    NewComp->bHasExecuted = false;
    NewComp->bOriginalVisibilitySaved = false;
    NewComp->bOriginalVisibilityBeforeHide = true;
    NewComp->SpawnedActors.clear();

    return NewComp;
}

void USpawnRandomRotatingCopiesComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    UActorComponent::GetEditableProperties(OutProps);

    OutProps.push_back({"SpawnCount", EPropertyType::Int, &SpawnCount});
    OutProps.push_back({"SpawnRadius", EPropertyType::Float, &SpawnRadius, 0.0f, 1000.0f, 0.1f});
    OutProps.push_back({"MinRotationSpeed", EPropertyType::Float, &MinRotationSpeed, 0.0f, 2000.0f, 1.0f});
    OutProps.push_back({"MaxRotationSpeed", EPropertyType::Float, &MaxRotationSpeed, 0.0f, 2000.0f, 1.0f});
    OutProps.push_back({"AffectOriginalActor", EPropertyType::Bool, &bAffectOriginalActor});
    OutProps.push_back({"HideOriginalActor", EPropertyType::Bool, &bHideOriginalActor});
    OutProps.push_back({"RandomSeed", EPropertyType::Int, &RandomSeed});
    OutProps.push_back({"SpawnOnBeginPlay", EPropertyType::Bool, &bSpawnOnBeginPlay});
    OutProps.push_back({"ScatterIn3D", EPropertyType::Bool, &bScatterIn3D});
}

void USpawnRandomRotatingCopiesComponent::ExecuteSpawn()
{
    if (!CanExecuteInCurrentWorld())
    {
        return;
    }

    AActor* OwnerActor = GetOwner();
    UWorld* World = OwnerActor->GetWorld();

    bHasExecuted = true;
    SpawnedActors.clear();

    const uint32 Seed = ResolveRandomSeed();
    std::mt19937 Rng(Seed);

    const int32 SafeSpawnCount = ClampSpawnCount(SpawnCount);
    const FVector Origin = OwnerActor->GetActorLocation();

    TArray<FVector> UsedOffsets;
    UsedOffsets.reserve(SafeSpawnCount);
    SpawnedActors.reserve(SafeSpawnCount);

    std::printf(
        "[SpawnRandomRotatingCopies] Begin Owner=%s Count=%d Radius=%.3f Seed=%u WorldType=PIE\n",
        OwnerActor->GetName().c_str(),
        SafeSpawnCount,
        SpawnRadius,
        Seed);

    for (int32 Index = 0; Index < SafeSpawnCount; ++Index)
    {
        const FVector RotationAxis = MakeRandomUnitAxis(Rng);
        const float RotationSpeed = MakeRandomRotationSpeed(Rng);
        const FVector Offset = MakeRandomSpawnOffset(Rng, UsedOffsets);
        const FVector SpawnLocation = Origin + Offset;

        AActor* SpawnedActor = SpawnDuplicateActor(OwnerActor, World, SpawnLocation, RotationAxis, RotationSpeed);
        if (SpawnedActor == nullptr)
        {
            std::printf("[SpawnRandomRotatingCopies] Failed to spawn copy %d\n", Index);
            continue;
        }

        SpawnedActors.push_back(SpawnedActor);
        UsedOffsets.push_back(Offset);

        std::printf(
            "[SpawnRandomRotatingCopies] Spawned[%d]=%s Location=(%.3f, %.3f, %.3f) Axis=(%.3f, %.3f, %.3f) Speed=%.3f\n",
            Index,
            SpawnedActor->GetName().c_str(),
            SpawnLocation.X,
            SpawnLocation.Y,
            SpawnLocation.Z,
            RotationAxis.X,
            RotationAxis.Y,
            RotationAxis.Z,
            RotationSpeed);
    }

    ApplyOriginalActorOptions(Rng);
    World->SyncSpatialIndex();

    std::printf(
        "[SpawnRandomRotatingCopies] Completed Owner=%s Spawned=%zu\n",
        OwnerActor->GetName().c_str(),
        SpawnedActors.size());
}

void USpawnRandomRotatingCopiesComponent::CleanupSpawnedActors()
{
    AActor* OwnerActor = GetOwner();
    UWorld* World = OwnerActor ? OwnerActor->GetWorld() : nullptr;
    if (World == nullptr)
    {
        SpawnedActors.clear();
        return;
    }

    TArray<AActor*> ActorsToDestroy = SpawnedActors;
    SpawnedActors.clear();

    for (AActor* SpawnedActor : ActorsToDestroy)
    {
        if (SpawnedActor != nullptr && SpawnedActor->GetWorld() == World)
        {
            World->DestroyActor(SpawnedActor);
        }
    }

    World->SyncSpatialIndex();
}

bool USpawnRandomRotatingCopiesComponent::CanExecuteInCurrentWorld() const
{
    if (bHasExecuted)
    {
        std::printf("[SpawnRandomRotatingCopies] Skipped: already executed this PIE session\n");
        return false;
    }

    AActor* OwnerActor = GetOwner();
    if (OwnerActor == nullptr)
    {
        std::printf("[SpawnRandomRotatingCopies] Skipped: missing owner actor\n");
        return false;
    }

    UWorld* World = OwnerActor->GetWorld();
    if (World == nullptr)
    {
        std::printf("[SpawnRandomRotatingCopies] Skipped: missing owner world\n");
        return false;
    }

    if (World->GetWorldType() != EWorldType::PIE)
    {
        std::printf("[SpawnRandomRotatingCopies] Skipped: world is not PIE\n");
        return false;
    }

    return true;
}

uint32 USpawnRandomRotatingCopiesComponent::ResolveRandomSeed() const
{
    if (RandomSeed != 0)
    {
        return static_cast<uint32>(RandomSeed);
    }

    const uint64 ClockSeed = static_cast<uint64>(
        std::chrono::high_resolution_clock::now().time_since_epoch().count());
    const uint64 ThisBits = reinterpret_cast<uintptr_t>(this);

    std::random_device Device;
    const uint64 DeviceSeed = (static_cast<uint64>(Device()) << 32) ^ static_cast<uint64>(Device());
    const uint64 MixedSeed = ClockSeed ^ DeviceSeed ^ ThisBits;

    return static_cast<uint32>((MixedSeed >> 32) ^ MixedSeed);
}

FVector USpawnRandomRotatingCopiesComponent::MakeRandomUnitAxis(std::mt19937& Rng) const
{
    for (int32 Attempt = 0; Attempt < 16; ++Attempt)
    {
        FVector Axis(
            RandomFloat(Rng, -1.0f, 1.0f),
            RandomFloat(Rng, -1.0f, 1.0f),
            RandomFloat(Rng, -1.0f, 1.0f));

        Axis = Axis.GetSafeNormal();
        if (!Axis.IsNearlyZero())
        {
            return Axis;
        }
    }

    return FVector::UpVector;
}

float USpawnRandomRotatingCopiesComponent::MakeRandomRotationSpeed(std::mt19937& Rng) const
{
    const float SafeMin = std::max(0.0f, std::min(MinRotationSpeed, MaxRotationSpeed));
    const float SafeMax = std::max(0.0f, std::max(MinRotationSpeed, MaxRotationSpeed));

    float Speed = RandomFloat(Rng, SafeMin, SafeMax);
    if (RandomBool(Rng))
    {
        Speed = -Speed;
    }

    return Speed;
}

FVector USpawnRandomRotatingCopiesComponent::MakeRandomSpawnOffset(
    std::mt19937& Rng,
    const TArray<FVector>& ExistingOffsets) const
{
    const float SafeRadius = std::max(0.0f, SpawnRadius);
    if (SafeRadius <= MathUtil::SmallNumber)
    {
        return FVector::ZeroVector;
    }

    FVector BestOffset = FVector::ZeroVector;
    for (int32 Attempt = 0; Attempt < 12; ++Attempt)
    {
        FVector CandidateOffset = FVector::ZeroVector;

        if (bScatterIn3D)
        {
            const FVector Direction = MakeRandomUnitAxis(Rng);
            const float Radius = std::cbrt(RandomFloat(Rng, 0.0f, 1.0f)) * SafeRadius;
            CandidateOffset = Direction * Radius;
        }
        else
        {
            const float Angle = RandomFloat(Rng, 0.0f, MathUtil::TwoPi);
            const float Radius = std::sqrt(RandomFloat(Rng, 0.0f, 1.0f)) * SafeRadius;
            CandidateOffset = FVector(std::cos(Angle) * Radius, std::sin(Angle) * Radius, 0.0f);
        }

        BestOffset = CandidateOffset;
        if (!IsTooCloseToExistingOffset(CandidateOffset, ExistingOffsets))
        {
            return CandidateOffset;
        }
    }

    return BestOffset;
}

bool USpawnRandomRotatingCopiesComponent::IsTooCloseToExistingOffset(
    const FVector& CandidateOffset,
    const TArray<FVector>& ExistingOffsets) const
{
    if (ExistingOffsets.empty() || SpawnRadius <= MathUtil::SmallNumber)
    {
        return false;
    }

    const float CountScale = std::sqrt(static_cast<float>(std::max<int32>(1, SpawnCount)));
    const float MinSeparation = std::max(0.05f, (SpawnRadius / CountScale) * 0.35f);
    const float MinSeparationSq = MinSeparation * MinSeparation;

    for (const FVector& ExistingOffset : ExistingOffsets)
    {
        if (FVector::DistSquared(CandidateOffset, ExistingOffset) < MinSeparationSq)
        {
            return true;
        }
    }

    return false;
}

AActor* USpawnRandomRotatingCopiesComponent::SpawnDuplicateActor(
    AActor* TemplateActor,
    UWorld* World,
    const FVector& SpawnLocation,
    const FVector& RotationAxis,
    float RotationSpeedDegreesPerSecond)
{
    if (TemplateActor == nullptr || World == nullptr || World->GetPersistentLevel() == nullptr)
    {
        return nullptr;
    }

    AActor* NewActor = TemplateActor->Duplicate();
    if (NewActor == nullptr)
    {
        return nullptr;
    }

    NewActor->DuplicateSubObjects();

    // Critical recursion guard: spawned copies must not carry this spawner.
    RemoveSpawnerComponentsFromActor(NewActor);

    NewActor->SetActorLocation(SpawnLocation);

    URotationMovementComponent* RotationComponent = NewActor->AddComponent<URotationMovementComponent>();
    ConfigureRotationComponent(RotationComponent, RotationAxis, RotationSpeedDegreesPerSecond);

    NewActor->SetWorld(World);
    World->GetPersistentLevel()->AddActor(NewActor);

    if (World->HasBegunPlay())
    {
        NewActor->BeginPlay();
    }

    return NewActor;
}

void USpawnRandomRotatingCopiesComponent::RemoveSpawnerComponentsFromActor(AActor* Actor) const
{
    if (Actor == nullptr)
    {
        return;
    }

    TArray<UActorComponent*> ComponentsToRemove;
    for (UActorComponent* Component : Actor->GetComponents())
    {
        if (Cast<USpawnRandomRotatingCopiesComponent>(Component) != nullptr)
        {
            ComponentsToRemove.push_back(Component);
        }
    }

    for (UActorComponent* Component : ComponentsToRemove)
    {
        Actor->RemoveComponent(Component);
    }
}

void USpawnRandomRotatingCopiesComponent::ConfigureRotationComponent(
    URotationMovementComponent* RotationComponent,
    const FVector& RotationAxis,
    float RotationSpeedDegreesPerSecond) const
{
    if (RotationComponent == nullptr)
    {
        return;
    }

    RotationComponent->SetAxisAngleRotation(RotationAxis, RotationSpeedDegreesPerSecond);
    RotationComponent->SetRotationInLocalSpace(false);
}

void USpawnRandomRotatingCopiesComponent::ApplyOriginalActorOptions(std::mt19937& Rng)
{
    AActor* OwnerActor = GetOwner();
    if (OwnerActor == nullptr)
    {
        return;
    }

    if (bAffectOriginalActor)
    {
        const FVector RotationAxis = MakeRandomUnitAxis(Rng);
        const float RotationSpeed = MakeRandomRotationSpeed(Rng);

        URotationMovementComponent* RotationComponent = OwnerActor->AddComponent<URotationMovementComponent>();
        ConfigureRotationComponent(RotationComponent, RotationAxis, RotationSpeed);
        RotationComponent->BeginPlay();

        std::printf(
            "[SpawnRandomRotatingCopies] Original affected Axis=(%.3f, %.3f, %.3f) Speed=%.3f\n",
            RotationAxis.X,
            RotationAxis.Y,
            RotationAxis.Z,
            RotationSpeed);
    }

    if (bHideOriginalActor)
    {
        if (!bOriginalVisibilitySaved)
        {
            bOriginalVisibilityBeforeHide = OwnerActor->IsVisible();
            bOriginalVisibilitySaved = true;
        }

        OwnerActor->SetVisible(false);
        std::printf("[SpawnRandomRotatingCopies] Original hidden Owner=%s\n", OwnerActor->GetName().c_str());
    }
}

void USpawnRandomRotatingCopiesComponent::RestoreOriginalActorVisibility()
{
    if (!bOriginalVisibilitySaved)
    {
        return;
    }

    AActor* OwnerActor = GetOwner();
    if (OwnerActor != nullptr)
    {
        OwnerActor->SetVisible(bOriginalVisibilityBeforeHide);
    }

    bOriginalVisibilitySaved = false;
}

/*
Test scenario:
1. Add USpawnRandomRotatingCopiesComponent to an editor-world actor.
2. Set SpawnCount=8, SpawnRadius=6, MinRotationSpeed=20, MaxRotationSpeed=120.
3. Start PIE. Only the PIE world should receive 8 duplicated actors.
4. Verify every copy has no USpawnRandomRotatingCopiesComponent and has one URotationMovementComponent.
5. Restart PIE with RandomSeed=1234. Locations, axes, and speeds should repeat.
6. Stop PIE. The editor world should contain only the original actor.
*/
