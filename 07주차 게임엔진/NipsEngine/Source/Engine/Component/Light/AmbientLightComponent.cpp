#include "AmbientLightComponent.h"
#include "Object/ObjectFactory.h"

DEFINE_CLASS(UAmbientLightComponent, ULightComponent)
REGISTER_FACTORY(UAmbientLightComponent)

UAmbientLightComponent* UAmbientLightComponent::Duplicate()
{
	UAmbientLightComponent* NewComp = UObjectManager::Get().CreateObject<UAmbientLightComponent>();
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

ELightType UAmbientLightComponent::GetLightType() const
{ 
	return ELightType::Ambient; 
}