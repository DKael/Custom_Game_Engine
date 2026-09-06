#pragma once

#include "MovementComponent.h"

class UPrimitiveComponent;

class UFloatingMovementComponent : public UMovementComponent
{
public:
    DECLARE_CLASS(UFloatingMovementComponent, UMovementComponent)

    void TickComponent(float DeltaTime) override;

    float GetMaxSpeed() const override { return DriftSpeed; }

    void Serialize(FArchive& Ar) override;
    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;

private:
    void CacheBaseTransformIfNeeded();

private:
    float BobAmplitude = 0.18f;
    float BobFrequency = 0.65f;
    float BobPhase = 0.0f;
    FVector DriftDirection = FVector(1.0f, 0.35f, 0.0f);
    float DriftSpeed = 0.05f;
    FVector TiltAmplitude = FVector(1.5f, 2.0f, 0.0f);
    float TiltFrequency = 0.45f;
    bool bUseDeterministicPhase = true;

    bool bHasCachedBaseTransform = false;
    FVector BaseLocation = FVector::ZeroVector;
    FVector BaseRotation = FVector::ZeroVector;
    FVector LastAppliedTiltOffset = FVector::ZeroVector;
    float ElapsedTime = 0.0f;
};
