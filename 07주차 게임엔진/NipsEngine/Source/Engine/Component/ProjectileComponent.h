#pragma once

#include "MovementComponent.h"
class UProjectileMovementComponent : public UMovementComponent
{
  public:
    DECLARE_CLASS(UProjectileMovementComponent, UMovementComponent)

    void TickComponent(float DeltaTime) override;

    virtual void BeginPlay() override;

    virtual void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;

    virtual UProjectileMovementComponent* Duplicate() override;

    void SetInitialVelocity(FVector InVelocity) { InitialVelocity = InVelocity; }
    void SetInitialAcceleration(FVector InAcceleration) { InitialAcceleration = InAcceleration; }
    void SetGravityEnabled(bool InEnable) { bIsGravityEnabled = InEnable; }
    void SetGravitationalAcceleration(float InValue) { GravitationalAcceleration = InValue; }

  private:
    FVector CalculateVelocity(float DeltaTime);

    FVector Velocity;
    FVector InitialVelocity;

    FVector Acceleration;
    FVector InitialAcceleration;

    float GravitationalAcceleration = -9.80665f;
    bool  bIsGravityEnabled = false;
};
