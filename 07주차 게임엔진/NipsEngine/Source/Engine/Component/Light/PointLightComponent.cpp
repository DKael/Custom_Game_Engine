#include "PointLightComponent.h"
#include "Object/ObjectFactory.h"

DEFINE_CLASS(UPointLightComponent, ULightComponent)
REGISTER_FACTORY(UPointLightComponent)

UPointLightComponent* UPointLightComponent::Duplicate()
{
    UPointLightComponent* NewComp = UObjectManager::Get().CreateObject<UPointLightComponent>();
    NewComp->SetActive(this->IsActive());
    NewComp->SetOwner(nullptr);
    NewComp->SetRelativeLocation(this->GetRelativeLocation());
    NewComp->SetRelativeRotation(this->GetRelativeRotation());
    NewComp->SetRelativeScale(this->GetRelativeScale());
    NewComp->SetColor(this->GetColor());
    NewComp->SetIntensity(this->GetIntensity());
    NewComp->SetRadius(this->GetRadius());
    NewComp->DuplicateSubObjects();
    return NewComp;
}

void UPointLightComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    ULightComponent::GetEditableProperties(OutProps);
    OutProps.push_back({"Radius", EPropertyType::Float, &Radius});
}
