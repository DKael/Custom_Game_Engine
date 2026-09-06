#pragma once
#include "ActorComponent.h"

class USceneComponent;
class UMovementComponent : public UActorComponent
{
  public:
    DECLARE_CLASS(UMovementComponent, UActorComponent)

    void SetMoveComponent(USceneComponent* InSceneComponent) { MoveComponent = InSceneComponent; }

	virtual void SetOwner(AActor* InActor) override;

  protected:
    USceneComponent* MoveComponent = nullptr;
};
