#include "SpotLightActor.h"
#include "Component/BillboardComponent.h"

DEFINE_CLASS(ASpotLightActor, AActor)
REGISTER_FACTORY(ASpotLightActor)

void ASpotLightActor::InitDefaultComponents()
{
    USceneComponent* Root = AddComponent<USceneComponent>();
    SetRootComponent(Root);

    LightComponent = AddComponent<USpotLightComponent>();
    LightComponent->AttachToComponent(Root);

    LightBillboard = AddComponent<UBillboardComponent>();
    LightBillboard->AttachToComponent(Root);
    LightBillboard->SetTextureName("Asset\\Texture\\S_LightSpot.png");
    LightBillboard->SetRelativeLocation(FVector(0.0f, 0.0f, 0.0f));
    LightBillboard->SetRelativeScale(FVector(5.0f, 5.0f, 5.0f));
    LightBillboard->SetOutlineEnabled(false);

	LightComponent->SetOnColorChanged(
        [this](const FColor& NewColor)
        {
            if (LightBillboard)
                LightBillboard->SetTintColor(NewColor);
        });
}
