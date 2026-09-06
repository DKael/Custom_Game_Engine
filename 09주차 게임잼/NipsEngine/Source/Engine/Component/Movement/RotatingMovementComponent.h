#pragma once

#include "MovementComponent.h"

class URotatingMovementComponent : public UMovementComponent
{
public:
    DECLARE_CLASS(URotatingMovementComponent, UMovementComponent)

    virtual void TickComponent(float DeltaTime) override;

    // RotationRate in degrees per second (X/Roll, Y/Pitch, Z/Yaw).
    void SetRotationSpeed(const FVector& InRotationRate) { RotationRate = InRotationRate; }
    const FVector& GetRotationRate() const { return RotationRate; }

    void SetPivotTranslation(const FVector& InPivot) { PivotTranslation = InPivot; }
    const FVector& GetPivotTranslation() const { return PivotTranslation; }

    void SetRotationInLocalSpace(bool bInLocalSpace) { bRotationInLocalSpace = bInLocalSpace; }
    bool IsRotationInLocalSpace() const { return bRotationInLocalSpace; }

    virtual float GetMaxSpeed() const override { return 0.0f; } // Rotation-only component.

    virtual void Serialize(FArchive& Ar) override;
    virtual void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;

private:
    // Angular velocity in degrees per second (Roll/Pitch/Yaw).
    FVector RotationRate = FVector(90.0f, 0.f, 0.f);
    // Pivot offset in local space.
    FVector PivotTranslation = FVector::ZeroVector;
    
    bool bRotationInLocalSpace = true;
};
