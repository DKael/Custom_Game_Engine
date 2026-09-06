#include "DirectionalLightActor.h"
#include "Component/BillboardComponent.h"

DEFINE_CLASS(ADirectionalLightActor, AActor)
REGISTER_FACTORY(ADirectionalLightActor)

void ADirectionalLightActor::InitDefaultComponents()
{
    USceneComponent* Root = AddComponent<USceneComponent>();
    SetRootComponent(Root);

    LightComponent = AddComponent<UDirectionalLightComponent>();
    LightComponent->AttachToComponent(Root);
	
    LightBillboard = AddComponent<UBillboardComponent>();
    LightBillboard->AttachToComponent(Root);
    LightBillboard->SetTextureName("Asset\\Texture\\S_LightDirectional.png");
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