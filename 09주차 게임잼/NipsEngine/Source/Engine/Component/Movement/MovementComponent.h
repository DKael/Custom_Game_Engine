#pragma once

#include "Component/ActorComponent.h"
#include "Core/CollisionTypes.h"

class USceneComponent;

// Abstract base class for movement components.
class UMovementComponent : public UActorComponent
{
public:
    DECLARE_CLASS(UMovementComponent, UActorComponent)

    virtual void TickComponent(float DeltaTime) override = 0;

    virtual void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
    virtual void Serialize(FArchive& Ar) override;

    void SetUpdatedComponent(USceneComponent* InComponent);
    USceneComponent* GetUpdatedComponent() const { return UpdatedComponent; }

    // Moves UpdatedComponent by Delta with collision handling.
    // bool SafeMoveUpdatedComponent(const FVector& Delta, FHitResult& OutHit);

    // Slides along blocking surfaces instead of stopping immediately.
    // void SlideAlongSurface(const FVector& Delta, const FHitResult& Hit, float RemainingTimeFraction);

    // Accumulates world-space input vectors.
    void AddInputVector(const FVector& WorldDirection, float Scale = 1.0f);

    // Returns and clears the pending input vector.
    FVector ConsumeInputVector();

    virtual float GetMaxSpeed() const = 0;
    // virtual void SetMaxSpeed(float ) const = 0;
    virtual bool IsExceedingMaxSpeed(float MaxSpeed) const;

    FVector GetVelocity() const { return Velocity; }
    void    SetVelocity(const FVector& InVelocity) { Velocity = InVelocity; }

    FVector GetPendingInputVector() const { return PendingInputVector; }
    void    SetPendingInputVector(const FVector InVector) { PendingInputVector = InVector; }

    FVector GetPlaneConstraintNormal() const { return PlaneConstraintNormal; }
    void    SetPlaneConstraintNormal(const FVector& InNormal) { PlaneConstraintNormal = InNormal; }

    // Applies plane constraints when bConstrainToPlane is enabled.
    FVector ConstrainDirectionToPlane(const FVector& Direction) const;
    FVector ConstrainLocationToPlane(const FVector& Location) const;

    // Immediately zeroes out velocity.
    void StopMovementImmediately() { Velocity = FVector::ZeroVector; }

protected:
    // Moves UpdatedComponent by Delta without high-level collision helpers.
    void MoveUpdatedComponent(const FVector& Delta);

protected:
    USceneComponent* UpdatedComponent = nullptr;
    FVector Velocity = FVector(-1.0f, 0.0f, 1.0f);
    FVector PendingInputVector = FVector::ZeroVector;           // Accumulated movement input.
    FVector PlaneConstraintNormal = FVector(0.0f, 0.0f, 1.0f);  // Constraint plane normal.

    bool bUpdateOnlyIfRendered = false; // Update only when the owner is rendered.
    bool bConstrainToPlane = false;     // Restrict movement to PlaneConstraintNormal.
};
