#pragma once
#include "MovementComponent.h"
class URotationMovementComponent : public UMovementComponent
{
  public:
    DECLARE_CLASS(URotationMovementComponent, UMovementComponent)

    void TickComponent(float DeltaTime) override;

    virtual void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
    virtual void PostEditProperty(const char* PropertyName) override;

    virtual URotationMovementComponent* Duplicate() override;

    void SetRotationRate(const FRotator& InRotationRate);
    void SetRotationRateEuler(const FVector& InEulerDegreesPerSecond);
    void SetPivotTranslation(const FVector& InPivotTranslation) { PivotTranslation = InPivotTranslation; }
    void SetRotationInLocalSpace(bool bInRotationInLocalSpace) { bRotationInLocalSpace = bInRotationInLocalSpace; }
    void SetAxisAngleRotation(const FVector& InRotationAxis, float InAngularSpeedDegreesPerSecond);

  private:
    FVector GetSafeRotationAxis() const;

	FRotator RotationRate = FRotator::ZeroRotator;
	FVector PivotTranslation = FVector::ZeroVector;

	bool bRotationInLocalSpace = false;
    bool bUseAxisAngleRotation = false;
    FVector RotationAxis = FVector::UpVector;
    float AngularSpeedDegreesPerSecond = 90.0f;
};
