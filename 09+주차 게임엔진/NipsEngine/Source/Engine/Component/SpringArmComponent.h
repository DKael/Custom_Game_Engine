#pragma once

#include "Component/SceneComponent.h"
#include "Core/CollisionTypes.h"

class UCameraComponent;

// Study-friendly spring arm component.
// Responsibilities:
// - Compute desired camera position from arm length and socket offset.
// - Optionally shorten arm when a collision is detected.
// - Optionally smooth camera location with simple lag.
class USpringArmComponent : public USceneComponent
{
public:
    DECLARE_CLASS(USpringArmComponent, USceneComponent)

    USpringArmComponent() = default;

    // Per-frame spring arm update.
    // Runtime sequence:
    // 1) Resolve target camera
    // 2) Compute desired world rotation
    // 3) Compute desired arm length (collision aware)
    // 4) Smooth arm length + optional camera lag
    // 5) Apply final world location/rotation to camera
    virtual void TickComponent(float DeltaTime) override;
    virtual void Serialize(FArchive& Ar) override;
    virtual void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;

private:
    // Final per-frame result consumed by ApplyCameraPose.
    struct FSpringArmEvaluationResult
    {
        FQuat ArmWorldRotation = FQuat::Identity;
        FVector CameraWorldLocation = FVector::ZeroVector;
    };

    // Resolve camera source this spring arm should drive.
    // Priority: direct child camera > pawn camera > first camera component on owner.
    UCameraComponent* ResolveTargetCameraComponent() const;
    // Compute world-space arm rotation from local rotation + inheritance toggles.
    FQuat BuildArmWorldRotation() const;
    // Compute desired camera socket location from pivot/rotation/arm length.
    FVector ComputeSocketWorldLocation(const FQuat& ArmWorldRotation, float ArmLength) const;
    // Perform collision trace and return shortened safe arm length if hit occurred.
    float ResolveCollisionArmLength(const FVector& PivotWorldLocation, const FVector& DesiredSocketWorldLocation) const;
    // Optional camera-location lag (pure visual smoothing).
    void ApplyCameraLag(float DeltaTime, const FVector& TargetWorldLocation, FVector& InOutWorldLocation);
    // Evaluate full spring-arm output for current frame.
    FSpringArmEvaluationResult EvaluateSpringArm(float DeltaTime);
    // Push evaluated pose to camera while preserving parent-relative transform correctness.
    void ApplyCameraPose(UCameraComponent* TargetCamera, const FSpringArmEvaluationResult& Result);

private:
    // Arm shape.
    float TargetArmLength = 350.0f;
    FVector SocketOffset = FVector::ZeroVector;

    // Rotation inheritance.
    bool bInheritPitch = true;
    bool bInheritYaw = true;
    bool bInheritRoll = true;

    // Collision settings.
    bool bDoCollisionTest = true;
    float CollisionProbeRadius = 12.0f;
    float CollisionPadding = 5.0f;

    // Smoothing settings.
    float ArmLengthBlendSpeed = 14.0f;
    bool bEnableCameraLag = true;
    float CameraLagSpeed = 16.0f;

    // Debug draw flag forwarded to collision trace utility.
    bool bDrawDebug = false;

    // Runtime state used by interpolation.
    float CurrentArmLength = TargetArmLength;
    FVector LaggedWorldLocation = FVector::ZeroVector;
    bool bLagStateInitialized = false;
};
