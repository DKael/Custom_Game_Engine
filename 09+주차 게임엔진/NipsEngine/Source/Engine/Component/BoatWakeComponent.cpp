#include "BoatWakeComponent.h"

#include "Component/BillboardComponent.h"
#include "Component/DecalComponent.h"
#include "Component/SceneComponent.h"
#include "Core/ResourceManager.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PrimitiveActors.h"
#include "GameFramework/World.h"
#include "Math/Matrix.h"
#include "Math/Quat.h"
#include "Math/Utils.h"
#include "Object/ObjectFactory.h"

#include <cmath>

namespace
{
    constexpr float WakeMinDistanceEpsilon = 1.0e-4f;
    constexpr float WakeMinAxisEpsilon = 1.0e-3f;
    constexpr float WakeFadeInDuration = 0.08f;
    constexpr float WakeMinSpawnSpacing = 0.05f;
    constexpr float WakeMaxDeltaTime = 0.25f;
    constexpr float WakeMinFrameMovementForActive = 0.05f;
    constexpr float WakeTeleportSpacingMultiplier = 8.0f;
    constexpr int32 WakeMaxDecalsPerFrame = 2;
    constexpr int32 DefaultMaxActiveWakeDecals = 64;

    float Clamp01(float Value)
    {
        return MathUtil::Clamp(Value, 0.0f, 1.0f);
    }

    float Lerp(float A, float B, float Alpha)
    {
        return A + (B - A) * Clamp01(Alpha);
    }

    FVector MakePlanarVector(const FVector& Value)
    {
        return FVector(Value.X, Value.Y, 0.0f);
    }

    int32 SanitizeWakeDecalBudget(int32 Budget)
    {
        return Budget > 0 ? Budget : DefaultMaxActiveWakeDecals;
    }

    float SanitizeSpawnSpacing(float InSpawnSpacing)
    {
        return std::isfinite(InSpawnSpacing) && InSpawnSpacing >= WakeMinSpawnSpacing
            ? InSpawnSpacing
            : WakeMinSpawnSpacing;
    }
}

DEFINE_CLASS(UBoatWakeComponent, UActorComponent)
REGISTER_FACTORY(UBoatWakeComponent)

void UBoatWakeComponent::Serialize(FArchive& Ar)
{
    UActorComponent::Serialize(Ar);

    Ar << "Main Decal Material" << MainDecalMaterial;
    Ar << "Min Spawn Speed" << MinSpawnSpeed;
    Ar << "Max Wake Speed" << MaxWakeSpeed;
    Ar << "Spawn Spacing" << SpawnSpacing;
    Ar << "Max Active Wake Decals" << MaxActiveWakeDecals;

    Ar << "Main Width" << MainWidth;
    Ar << "Main Length" << MainLength;
    Ar << "Main Depth" << MainDepth;
    Ar << "Main Backward Offset" << MainBackwardOffset;
    Ar << "Main Fade Start Delay" << MainFadeStartDelay;
    Ar << "Main Fade Duration" << MainFadeDuration;
    Ar << "Main Fade Out Size Multiplier" << MainFadeOutSizeMultiplier;

    Ar << "Water Height Offset" << WaterHeightOffset;

    if (Ar.IsLoading())
    {
        SpawnSpacing = SanitizeSpawnSpacing(SpawnSpacing);
        MaxActiveWakeDecals = SanitizeWakeDecalBudget(MaxActiveWakeDecals);
        MainDecalMaterialRef = nullptr;
        DistanceAccumulator = 0.0f;
        bHasHistory = false;
        bWasMovingAboveSpawnSpeed = false;
        LastOwnerLocation = FVector::ZeroVector;
        WakeDecalPool.clear();
        NextWakeDecalPoolIndex = 0;
    }
}

void UBoatWakeComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    UActorComponent::GetEditableProperties(OutProps);

    OutProps.push_back({ "Main Decal Material", EPropertyType::String, &MainDecalMaterial });
    OutProps.push_back({ "Min Spawn Speed", EPropertyType::Float, &MinSpawnSpeed, 0.0f, 20.0f, 0.05f });
    OutProps.push_back({ "Max Wake Speed", EPropertyType::Float, &MaxWakeSpeed, 0.1f, 40.0f, 0.05f });
    OutProps.push_back({ "Spawn Spacing", EPropertyType::Float, &SpawnSpacing, 0.2f, 20.0f, 0.05f });
    OutProps.push_back({ "Max Active Wake Decals", EPropertyType::Int, &MaxActiveWakeDecals });

    OutProps.push_back({ "Main Width", EPropertyType::Float, &MainWidth, 0.1f, 30.0f, 0.05f });
    OutProps.push_back({ "Main Length", EPropertyType::Float, &MainLength, 0.1f, 40.0f, 0.05f });
    OutProps.push_back({ "Main Depth", EPropertyType::Float, &MainDepth, 0.1f, 10.0f, 0.05f });
    OutProps.push_back({ "Main Backward Offset", EPropertyType::Float, &MainBackwardOffset, 0.0f, 20.0f, 0.05f });
    OutProps.push_back({ "Main Fade Start Delay", EPropertyType::Float, &MainFadeStartDelay, 0.0f, 5.0f, 0.01f });
    OutProps.push_back({ "Main Fade Duration", EPropertyType::Float, &MainFadeDuration, 0.05f, 8.0f, 0.01f });
    OutProps.push_back({ "Main Fade Out Size Multiplier", EPropertyType::Vec3, &MainFadeOutSizeMultiplier });
    OutProps.push_back({ "Water Height Offset", EPropertyType::Float, &WaterHeightOffset, -2.0f, 2.0f, 0.01f });
}

void UBoatWakeComponent::PostEditProperty(const char* PropertyName)
{
    UActorComponent::PostEditProperty(PropertyName);
    (void)PropertyName;
    SpawnSpacing = SanitizeSpawnSpacing(SpawnSpacing);
    MaxActiveWakeDecals = SanitizeWakeDecalBudget(MaxActiveWakeDecals);
    RefreshMaterialRefs();
}

void UBoatWakeComponent::BeginPlay()
{
    UActorComponent::BeginPlay();

    SpawnSpacing = SanitizeSpawnSpacing(SpawnSpacing);
    MaxActiveWakeDecals = SanitizeWakeDecalBudget(MaxActiveWakeDecals);
    RefreshMaterialRefs();
    WakeDecalPool.clear();
    NextWakeDecalPoolIndex = 0;

    if (AActor* Owner = GetOwner())
    {
        LastOwnerLocation = Owner->GetActorLocation();
        bHasHistory = true;
        bWasMovingAboveSpawnSpeed = false;
    }
}

void UBoatWakeComponent::EndPlay()
{
    ReleaseWakeDecalPool();
    UActorComponent::EndPlay();
}

void UBoatWakeComponent::TickComponent(float DeltaTime)
{
    AActor* Owner = GetOwner();
    if (Owner == nullptr || Owner->IsPendingDestroy())
    {
        return;
    }

    const FVector CurrentLocation = Owner->GetActorLocation();
    if (!bHasHistory)
    {
        LastOwnerLocation = CurrentLocation;
        bHasHistory = true;
        return;
    }

    if (DeltaTime <= 0.0f || !std::isfinite(DeltaTime))
    {
        ResetWakeProgress(CurrentLocation);
        return;
    }

    const FVector MovementDelta = MakePlanarVector(CurrentLocation - LastOwnerLocation);
    const float MovedDistance = MovementDelta.Size();

    if (MovedDistance <= WakeMinDistanceEpsilon)
    {
        ResetWakeProgress(CurrentLocation);
        return;
    }

    const float TeleportDistance = SpawnSpacing * WakeTeleportSpacingMultiplier;
    if (DeltaTime > WakeMaxDeltaTime ||
        !std::isfinite(MovedDistance) ||
        (TeleportDistance > 0.0f && MovedDistance > TeleportDistance))
    {
        ResetWakeProgress(CurrentLocation);
        return;
    }

    if (MainDecalMaterialRef == nullptr)
    {
        RefreshMaterialRefs();
    }

    FVector BoatForward;
    FVector BoatRight;
    if (!ResolveBoatAxes(BoatForward, BoatRight))
    {
        ResetWakeProgress(CurrentLocation);
        return;
    }

    const FVector MoveDir = MovementDelta / MovedDistance;
    const float Speed = MovedDistance / DeltaTime;
    const float SpeedRange = (MaxWakeSpeed - MinSpawnSpeed) > 0.001f ? (MaxWakeSpeed - MinSpawnSpeed) : 0.001f;
    const float SpeedAlpha = Clamp01((Speed - MinSpawnSpeed) / SpeedRange);
    const bool bMovingAboveSpawnSpeed = Speed >= MinSpawnSpeed || MovedDistance >= WakeMinFrameMovementForActive;

    if (bMovingAboveSpawnSpeed)
    {
        int32 SpawnedThisFrame = 0;
        if (!bWasMovingAboveSpawnSpeed)
        {
            DistanceAccumulator = 0.0f;
            SpawnWakeSet(CurrentLocation, BoatForward, BoatRight, SpeedAlpha);
            ++SpawnedThisFrame;
        }

        DistanceAccumulator += MovedDistance;

        while (DistanceAccumulator >= SpawnSpacing && SpawnedThisFrame < WakeMaxDecalsPerFrame)
        {
            const float Overshoot = DistanceAccumulator - SpawnSpacing;
            const FVector SampleLocation = CurrentLocation - MoveDir * Overshoot;
            SpawnWakeSet(SampleLocation, BoatForward, BoatRight, SpeedAlpha);
            DistanceAccumulator -= SpawnSpacing;
            ++SpawnedThisFrame;
        }

        if (DistanceAccumulator >= SpawnSpacing)
        {
            DistanceAccumulator = 0.0f;
        }
    }
    else
    {
        ResetWakeProgress(CurrentLocation);
        return;
    }

    bWasMovingAboveSpawnSpeed = bMovingAboveSpawnSpeed;
    LastOwnerLocation = CurrentLocation;
}

void UBoatWakeComponent::RefreshMaterialRefs()
{
    MainDecalMaterialRef = MainDecalMaterial.empty()
        ? nullptr
        : FResourceManager::Get().GetMaterialInterface(MainDecalMaterial);
}

bool UBoatWakeComponent::ResolveBoatAxes(FVector& OutForward, FVector& OutRight) const
{
    AActor* Owner = GetOwner();
    if (Owner == nullptr)
    {
        return false;
    }

    if (APawn* Pawn = Cast<APawn>(Owner))
    {
        OutForward = MakePlanarVector(Pawn->GetForwardVector()).GetSafeNormal2D();
        OutRight = MakePlanarVector(Pawn->GetRightVector()).GetSafeNormal2D();
    }
    else
    {
        OutForward = MakePlanarVector(Owner->GetActorForward()).GetSafeNormal2D();
        if (USceneComponent* Root = Owner->GetRootComponent())
        {
            OutRight = MakePlanarVector(Root->GetRightVector()).GetSafeNormal2D();
        }
        else
        {
            OutRight = FVector::RightVector;
        }
    }

    if (OutForward.SizeSquared() <= WakeMinAxisEpsilon)
    {
        return false;
    }

    if (OutRight.SizeSquared() <= WakeMinAxisEpsilon)
    {
        OutRight = FVector::CrossProduct(FVector::UpVector, OutForward).GetSafeNormal2D();
    }

    return OutRight.SizeSquared() > WakeMinAxisEpsilon;
}

void UBoatWakeComponent::ResetWakeProgress(const FVector& CurrentLocation)
{
    DistanceAccumulator = 0.0f;
    bWasMovingAboveSpawnSpeed = false;
    LastOwnerLocation = CurrentLocation;
    bHasHistory = true;
}

void UBoatWakeComponent::SpawnWakeSet(
    const FVector& BoatLocation,
    const FVector& BoatForward,
    const FVector& BoatRight,
    float SpeedAlpha)
{
    if (MainDecalMaterialRef == nullptr)
    {
        return;
    }

    const float MainWidthScale = Lerp(0.85f, 1.15f, SpeedAlpha);
    const float MainLengthScale = Lerp(0.80f, 1.20f, SpeedAlpha);
    const FVector MainSize(
        MainDepth,
        MainWidth * MainWidthScale,
        MainLength * MainLengthScale);
    const FVector WaterOffset(0.0f, 0.0f, WaterHeightOffset);
    const FVector SternAnchor = BoatLocation + WaterOffset;
    const FVector MainLocation = SternAnchor - BoatForward * MainBackwardOffset;

    if (MainDecalMaterialRef != nullptr)
    {
        SpawnWakeDecal(
            MainLocation,
            BoatForward,
            BoatRight,
            MainSize,
            MainDecalMaterialRef,
            MainFadeStartDelay,
            MainFadeDuration);
    }
}

void UBoatWakeComponent::SpawnWakeDecal(
    const FVector& WorldLocation,
    const FVector& PlaneForward,
    const FVector& PlaneRight,
    const FVector& DecalSize,
    UMaterialInterface* Material,
    float FadeStartDelay,
    float FadeDuration)
{
    AActor* Owner = GetOwner();
    UWorld* World = Owner ? Owner->GetFocusedWorld() : nullptr;
    if (World == nullptr || Material == nullptr)
    {
        return;
    }

    const FVector ProjectionAxis = -FVector::UpVector;
    const FVector DecalForward = MakePlanarVector(PlaneForward).GetSafeNormal2D();
    const FVector DecalRight = MakePlanarVector(PlaneRight).GetSafeNormal2D();
    if (DecalForward.SizeSquared() <= WakeMinAxisEpsilon || DecalRight.SizeSquared() <= WakeMinAxisEpsilon)
    {
        return;
    }

    ADecalActor* DecalActor = AcquireWakeDecalActor(World);
    if (DecalActor == nullptr)
    {
        return;
    }

    UDecalComponent* DecalComponent = ResolveWakeDecalComponent(DecalActor);
    if (DecalComponent == nullptr)
    {
        DecalActor->Destroy();
        return;
    }

    DecalActor->SetVisible(true);
    DecalActor->SetActive(true);
    DecalActor->SetActorLocation(WorldLocation);
    DecalComponent->SetSize(DecalSize);
    DecalComponent->SetFadeOutSizeMultiplier(MainFadeOutSizeMultiplier);
    DecalComponent->SetAffectOnlyWaterReceivers(true);
    DecalComponent->SetMaterial(Material);
    DecalComponent->SetFadeIn(0.0f, WakeFadeInDuration);
    DecalComponent->SetFadeOut(FadeStartDelay, FadeDuration, false);
    DecalComponent->ResetFadeState();

    FMatrix RotationMatrix = FMatrix::Identity;
    RotationMatrix.SetAxes(ProjectionAxis, DecalRight, DecalForward);

    if (USceneComponent* RootComponent = DecalActor->GetRootComponent())
    {
        FQuat RotationQuat(RotationMatrix);
        RotationQuat.Normalize();
        RootComponent->SetRelativeRotationQuat(RotationQuat);
    }
}

ADecalActor* UBoatWakeComponent::CreateWakeDecalActor(UWorld* World) const
{
    if (World == nullptr)
    {
        return nullptr;
    }

    ADecalActor* NewDecalActor = World->SpawnActor<ADecalActor>();
    if (NewDecalActor == nullptr)
    {
        return nullptr;
    }

    for (UActorComponent* Component : NewDecalActor->GetComponents())
    {
        UBillboardComponent* BillboardComponent = Cast<UBillboardComponent>(Component);
        if (BillboardComponent == nullptr)
        {
            continue;
        }

        BillboardComponent->SetEditorOnly(true);
        BillboardComponent->SetVisibility(false);
        BillboardComponent->SetActive(false);
    }

    UDecalComponent* DecalComponent = ResolveWakeDecalComponent(NewDecalActor);
    if (DecalComponent == nullptr)
    {
        NewDecalActor->Destroy();
        return nullptr;
    }

    DeactivateWakeDecal(NewDecalActor);
    return NewDecalActor;
}

ADecalActor* UBoatWakeComponent::AcquireWakeDecalActor(UWorld* World)
{
    const int32 PoolBudget = SanitizeWakeDecalBudget(MaxActiveWakeDecals);
    if (World == nullptr)
    {
        return nullptr;
    }

    for (ADecalActor*& PooledDecalActor : WakeDecalPool)
    {
        if (PooledDecalActor == nullptr)
        {
            continue;
        }

        if (!UObject::IsValid(PooledDecalActor) ||
            PooledDecalActor->IsPendingDestroy() ||
            PooledDecalActor->IsBeingDestroyed() ||
            ResolveWakeDecalComponent(PooledDecalActor) == nullptr)
        {
            PooledDecalActor = nullptr;
        }
    }

    for (ADecalActor* PooledDecalActor : WakeDecalPool)
    {
        if (PooledDecalActor != nullptr && !PooledDecalActor->IsActive())
        {
            return PooledDecalActor;
        }
    }

    if (static_cast<int32>(WakeDecalPool.size()) < PoolBudget)
    {
        ADecalActor* NewDecalActor = CreateWakeDecalActor(World);
        if (NewDecalActor != nullptr)
        {
            WakeDecalPool.push_back(NewDecalActor);
        }
        return NewDecalActor;
    }

    const int32 PoolCount = static_cast<int32>(WakeDecalPool.size());
    if (PoolCount <= 0)
    {
        return nullptr;
    }

    if (NextWakeDecalPoolIndex < 0 || NextWakeDecalPoolIndex >= PoolCount)
    {
        NextWakeDecalPoolIndex = 0;
    }

    const int32 ReuseIndex = NextWakeDecalPoolIndex;
    NextWakeDecalPoolIndex = (NextWakeDecalPoolIndex + 1) % PoolCount;

    if (WakeDecalPool[ReuseIndex] == nullptr)
    {
        WakeDecalPool[ReuseIndex] = CreateWakeDecalActor(World);
    }

    return WakeDecalPool[ReuseIndex];
}

UDecalComponent* UBoatWakeComponent::ResolveWakeDecalComponent(ADecalActor* DecalActor) const
{
    if (DecalActor == nullptr || !UObject::IsValid(DecalActor))
    {
        return nullptr;
    }

    return Cast<UDecalComponent>(DecalActor->GetRootComponent());
}

void UBoatWakeComponent::DeactivateWakeDecal(ADecalActor* DecalActor) const
{
    if (DecalActor == nullptr || !UObject::IsValid(DecalActor))
    {
        return;
    }

    if (UDecalComponent* DecalComponent = ResolveWakeDecalComponent(DecalActor))
    {
        DecalComponent->SetVisibility(false);
        DecalComponent->SetActive(false);
    }

    DecalActor->SetVisible(false);
    DecalActor->SetActive(false);
}

void UBoatWakeComponent::ReleaseWakeDecalPool()
{
    for (ADecalActor* DecalActor : WakeDecalPool)
    {
        if (DecalActor == nullptr || !UObject::IsValid(DecalActor))
        {
            continue;
        }

        DeactivateWakeDecal(DecalActor);
        DecalActor->Destroy();
    }

    WakeDecalPool.clear();
    NextWakeDecalPoolIndex = 0;
}
