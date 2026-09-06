#include "MovementComponent.h"
#include "Object/ObjectFactory.h"
#include "GameFramework/AActor.h"

DEFINE_CLASS(UMovementComponent, UActorComponent)
REGISTER_FACTORY(UMovementComponent)

void UMovementComponent::SetOwner(AActor* InActor)
{
    if (!InActor)
        return;

    UActorComponent::SetOwner(InActor);
    SetMoveComponent(InActor->GetRootComponent());
}