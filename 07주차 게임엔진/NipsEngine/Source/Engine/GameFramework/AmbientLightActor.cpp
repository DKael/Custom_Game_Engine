#include "AmbientLightActor.h"

DEFINE_CLASS(AAmbientLightActor, AActor)
REGISTER_FACTORY(AAmbientLightActor)

void AAmbientLightActor::InitDefaultComponents()
{
    USceneComponent* Root = AddComponent<USceneComponent>();
    SetRootComponent(Root);

    LightComponent = AddComponent<UAmbientLightComponent>();
    LightComponent->AttachToComponent(Root);
}
