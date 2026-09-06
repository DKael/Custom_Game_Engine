#include "DirectionalLightComponent.h"
#include "Object/ObjectFactory.h"

DEFINE_CLASS(UDirectionalLightComponent, ULightComponent)
REGISTER_FACTORY(UDirectionalLightComponent)

UDirectionalLightComponent* UDirectionalLightComponent::Duplicate()
{
    UDirectionalLightComponent* NewComp = UObjectManager::Get().CreateObject<UDirectionalLightComponent>();
    NewComp->SetActive(this->IsActive());
    NewComp->SetOwner(nullptr);
    NewComp->SetRelativeLocation(this->GetRelativeLocation());
    NewComp->SetRelativeRotation(this->GetRelativeRotation());
    NewComp->SetRelativeScale(this->GetRelativeScale());
    NewComp->SetColor(this->GetColor());
    NewComp->SetIntensity(this->GetIntensity());
    NewComp->DuplicateSubObjects();
    return NewComp;
}

FVector UDirectionalLightComponent::GetLightDirection() const 
{ 
	return GetForwardVector().GetSafeNormal();
	// return (-GetForwardVector()).GetSafeNormal(); 
}

ELightType UDirectionalLightComponent::GetLightType() const
{
	return ELightType::Directional; 
}