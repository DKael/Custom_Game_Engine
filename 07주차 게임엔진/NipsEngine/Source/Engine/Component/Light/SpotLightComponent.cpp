#include "SpotLightComponent.h"
#include "Object/ObjectFactory.h"
#include <algorithm>

DEFINE_CLASS(USpotLightComponent, ULightComponent)
REGISTER_FACTORY(USpotLightComponent)

USpotLightComponent* USpotLightComponent::Duplicate()
{
    USpotLightComponent* NewComp = UObjectManager::Get().CreateObject<USpotLightComponent>();
    NewComp->SetActive(this->IsActive());
    NewComp->SetOwner(nullptr);
    NewComp->SetRelativeLocation(this->GetRelativeLocation());
    NewComp->SetRelativeRotation(this->GetRelativeRotation());
    NewComp->SetRelativeScale(this->GetRelativeScale());
    NewComp->SetColor(this->GetColor());
    NewComp->SetIntensity(this->GetIntensity());
    NewComp->SetInnerConeAngle(this->GetInnerConeAngle());
    NewComp->SetOuterConeAngle(this->GetOuterConeAngle());
    NewComp->SetRadius(this->GetRadius());
    NewComp->DuplicateSubObjects();
    return NewComp;
}

void USpotLightComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) 
{
    ULightComponent::GetEditableProperties(OutProps);
    OutProps.push_back({"Radius", EPropertyType::Float, &Radius, 0.1f});
    OutProps.push_back({"InnerConeAngle", EPropertyType::Float, &InnerConeAngle, 0.f, OuterConeAngle});
    OutProps.push_back({"OuterConeAngle", EPropertyType::Float, &OuterConeAngle, InnerConeAngle, 80.f});
}

const FVector USpotLightComponent::GetDirection() const
{
	return GetWorldMatrix().GetForwardVector(); 
}

float USpotLightComponent::GetInnerConeAngle() const { return InnerConeAngle; }

void USpotLightComponent::SetInnerConeAngle(float InAngle)
{
    InnerConeAngle = std::clamp(InAngle, 0.0f, OuterConeAngle);
}

// --- Outer Cone Angle ---
float USpotLightComponent::GetOuterConeAngle() const 
{
	return OuterConeAngle; 
}

void USpotLightComponent::SetOuterConeAngle(float InAngle)
{
    // 외부 각도는 내부 각도보다 작아지지 않도록 하며, 최대 80~90도 정도로 제한합니다.
    OuterConeAngle = std::clamp(InAngle, InnerConeAngle, 80.0f);
}

// --- Set Both Angles ---
void USpotLightComponent::SetConeAngles(float InInnerAngle, float InOuterAngle)
{
    // 먼저 외부 각도를 설정하여 범위를 넓힌 뒤 내부 각도를 설정합니다.
    OuterConeAngle = std::clamp(InOuterAngle, 0.0f, MaxConeAngle);
    InnerConeAngle = std::clamp(InInnerAngle, 0.0f, OuterConeAngle);
}