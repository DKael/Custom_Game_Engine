#include "Component/SpringArmComponent.h"

#include "Collision/CollisionSystem.h"
#include "Component/CameraComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/World.h"
#include "Math/Rotator.h"
#include "Object/ObjectFactory.h"

DEFINE_CLASS(USpringArmComponent, USceneComponent)
REGISTER_FACTORY(USpringArmComponent)

namespace
{
    float FInterpTo(float Current, float Target, float DeltaTime, float InterpSpeed)
    {
        if (InterpSpeed <= 0.0f)
        {
            return Target;
        }

        const float Alpha = MathUtil::Clamp(DeltaTime * InterpSpeed, 0.0f, 1.0f);
        return Current + (Target - Current) * Alpha;
    }

    FVector VInterpTo(const FVector& Current, const FVector& Target, float DeltaTime, float InterpSpeed)
    {
        if (InterpSpeed <= 0.0f)
        {
            return Target;
        }

        const float Alpha = MathUtil::Clamp(DeltaTime * InterpSpeed, 0.0f, 1.0f);
        return Current + (Target - Current) * Alpha;
    }
}

void USpringArmComponent::TickComponent(float DeltaTime)
{
    USceneComponent::TickComponent(DeltaTime);

    // Step 1) Find camera to drive.
    UCameraComponent* TargetCamera = ResolveTargetCameraComponent();
    if (TargetCamera == nullptr)
    {
        return;
    }

    // Step 2~4) Evaluate spring-arm output (rotation, collision, lag).
    const FSpringArmEvaluationResult Result = EvaluateSpringArm(DeltaTime);

    // Step 5) Apply evaluated pose to camera component.
    ApplyCameraPose(TargetCamera, Result);
}

void USpringArmComponent::Serialize(FArchive& Ar)
{
    USceneComponent::Serialize(Ar);

    Ar << "TargetArmLength" << TargetArmLength;
    Ar << "SocketOffset" << SocketOffset;
    Ar << "InheritPitch" << bInheritPitch;
    Ar << "InheritYaw" << bInheritYaw;
    Ar << "InheritRoll" << bInheritRoll;
    Ar << "DoCollisionTest" << bDoCollisionTest;
    Ar << "CollisionProbeRadius" << CollisionProbeRadius;
    Ar << "CollisionPadding" << CollisionPadding;
    Ar << "ArmLengthBlendSpeed" << ArmLengthBlendSpeed;
    Ar << "EnableCameraLag" << bEnableCameraLag;
    Ar << "CameraLagSpeed" << CameraLagSpeed;
    Ar << "DrawDebug" << bDrawDebug;
}

void USpringArmComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    USceneComponent::GetEditableProperties(OutProps);

    OutProps.push_back({ "Target Arm Length", EPropertyType::Float, &TargetArmLength, 0.0f, 5000.0f, 1.0f });
    OutProps.push_back({ "Socket Offset", EPropertyType::Vec3, &SocketOffset, 0.0f, 0.0f, 0.1f });

    OutProps.push_back({ "Inherit Pitch", EPropertyType::Bool, &bInheritPitch });
    OutProps.push_back({ "Inherit Yaw", EPropertyType::Bool, &bInheritYaw });
    OutProps.push_back({ "Inherit Roll", EPropertyType::Bool, &bInheritRoll });

    OutProps.push_back({ "Do Collision Test", EPropertyType::Bool, &bDoCollisionTest });
    OutProps.push_back({ "Collision Probe Radius", EPropertyType::Float, &CollisionProbeRadius, 0.0f, 128.0f, 0.5f });
    OutProps.push_back({ "Collision Padding", EPropertyType::Float, &CollisionPadding, 0.0f, 128.0f, 0.5f });
    OutProps.push_back({ "Arm Length Blend Speed", EPropertyType::Float, &ArmLengthBlendSpeed, 0.0f, 100.0f, 0.1f });

    OutProps.push_back({ "Enable Camera Lag", EPropertyType::Bool, &bEnableCameraLag });
    OutProps.push_back({ "Camera Lag Speed", EPropertyType::Float, &CameraLagSpeed, 0.0f, 100.0f, 0.1f });
    OutProps.push_back({ "Draw Debug", EPropertyType::Bool, &bDrawDebug });
}

UCameraComponent* USpringArmComponent::ResolveTargetCameraComponent() const
{
    for (USceneComponent* Child : GetChildren())
    {
        if (UCameraComponent* CameraChild = Cast<UCameraComponent>(Child))
        {
            return CameraChild;
        }
    }

    AActor* OwnerActor = GetOwner();
    if (OwnerActor == nullptr)
    {
        return nullptr;
    }

    if (APawn* Pawn = Cast<APawn>(OwnerActor))
    {
        return Pawn->GetCameraComponent();
    }

    for (UActorComponent* Component : OwnerActor->GetComponents())
    {
        if (UCameraComponent* CameraComponent = Cast<UCameraComponent>(Component))
        {
            return CameraComponent;
        }
    }

    return nullptr;
}

FQuat USpringArmComponent::BuildArmWorldRotation() const
{
    const USceneComponent* ParentComponent = GetParent();
    if (ParentComponent == nullptr)
    {
        return GetWorldTransform().GetRotation();
    }

    FRotator ParentRotation = ParentComponent->GetWorldTransform().GetRotation().Rotator();
    FRotator LocalRotation = GetRelativeQuat().Rotator();

    FRotator ArmRotation = LocalRotation;
    if (bInheritPitch)
    {
        ArmRotation.Pitch += ParentRotation.Pitch;
    }
    if (bInheritYaw)
    {
        ArmRotation.Yaw += ParentRotation.Yaw;
    }
    if (bInheritRoll)
    {
        ArmRotation.Roll += ParentRotation.Roll;
    }

    ArmRotation.Normalize();
    return ArmRotation.Quaternion();
}

FVector USpringArmComponent::ComputeSocketWorldLocation(const FQuat& ArmWorldRotation, float ArmLength) const
{
    const FVector PivotWorldLocation = GetWorldLocation();
    const FVector LocalBack = FVector(-1.0f, 0.0f, 0.0f) * ArmLength;
    const FVector ArmOffsetWorld = ArmWorldRotation.RotateVector(LocalBack + SocketOffset);
    return PivotWorldLocation + ArmOffsetWorld;
}

float USpringArmComponent::ResolveCollisionArmLength(const FVector& PivotWorldLocation, const FVector& DesiredSocketWorldLocation) const
{
    const float DesiredArmLength = MathUtil::Clamp(TargetArmLength, 0.0f, 100000.0f);
    if (!bDoCollisionTest || DesiredArmLength <= MathUtil::SmallNumber)
    {
        return DesiredArmLength;
    }

    UWorld* World = nullptr;
    if (!TryGetOwnerWorld(World) || World == nullptr)
    {
        return DesiredArmLength;
    }

    FHitResult Hit;
    const FString IgnoreTag = (GetOwner() != nullptr) ? GetOwner()->GetTag() : FString();
    const bool bHit = World->GetCollisionSystem().LineTraceSingle(
        World,
        PivotWorldLocation,
        DesiredSocketWorldLocation,
        Hit,
        IgnoreTag,
        bDrawDebug);

    if (!bHit || !Hit.IsValid())
    {
        return DesiredArmLength;
    }

    const float SafeDistance = MathUtil::Clamp(
        Hit.Distance - MathUtil::Clamp(CollisionPadding + CollisionProbeRadius, 0.0f, 100.0f),
        0.0f,
        DesiredArmLength);
    return SafeDistance;
}

void USpringArmComponent::ApplyCameraLag(float DeltaTime, const FVector& TargetWorldLocation, FVector& InOutWorldLocation)
{
    if (!bEnableCameraLag || CameraLagSpeed <= 0.0f)
    {
        bLagStateInitialized = false;
        InOutWorldLocation = TargetWorldLocation;
        return;
    }

    if (!bLagStateInitialized)
    {
        LaggedWorldLocation = TargetWorldLocation;
        bLagStateInitialized = true;
    }

    LaggedWorldLocation = VInterpTo(LaggedWorldLocation, TargetWorldLocation, DeltaTime, CameraLagSpeed);
    InOutWorldLocation = LaggedWorldLocation;
}

USpringArmComponent::FSpringArmEvaluationResult USpringArmComponent::EvaluateSpringArm(float DeltaTime)
{
    FSpringArmEvaluationResult Result;
    Result.ArmWorldRotation = BuildArmWorldRotation();

    const float ClampedTargetLength = MathUtil::Clamp(TargetArmLength, 0.0f, 100000.0f);
    const FVector PivotWorldLocation = GetWorldLocation();

    // 1) Desired endpoint without collision reduction.
    const FVector DesiredSocketWorldLocation =
        ComputeSocketWorldLocation(Result.ArmWorldRotation, ClampedTargetLength);

    // 2) Collision test returns a safe length.
    const float DesiredSafeLength =
        ResolveCollisionArmLength(PivotWorldLocation, DesiredSocketWorldLocation);

    // 3) Blend arm length to avoid abrupt camera pops.
    CurrentArmLength = FInterpTo(CurrentArmLength, DesiredSafeLength, DeltaTime, ArmLengthBlendSpeed);

    // 4) Compute blended endpoint from current arm length.
    const FVector BlendedSocketWorldLocation =
        ComputeSocketWorldLocation(Result.ArmWorldRotation, CurrentArmLength);

    // 5) Optional location lag for softer follow.
    Result.CameraWorldLocation = BlendedSocketWorldLocation;
    ApplyCameraLag(DeltaTime, BlendedSocketWorldLocation, Result.CameraWorldLocation);

    return Result;
}

void USpringArmComponent::ApplyCameraPose(UCameraComponent* TargetCamera, const FSpringArmEvaluationResult& Result)
{
    if (TargetCamera == nullptr)
    {
        return;
    }

    // Spring arm writes final world-space location.
    TargetCamera->SetWorldLocation(Result.CameraWorldLocation);

    // Convert world rotation to parent-relative rotation before applying.
    FQuat CameraRelativeRotation = Result.ArmWorldRotation;
    if (USceneComponent* CameraParent = TargetCamera->GetParent())
    {
        const FQuat ParentWorldRotation = CameraParent->GetWorldTransform().GetRotation();
        CameraRelativeRotation = ParentWorldRotation.Inverse() * Result.ArmWorldRotation;
    }

    TargetCamera->SetRelativeRotationQuat(CameraRelativeRotation);
}
