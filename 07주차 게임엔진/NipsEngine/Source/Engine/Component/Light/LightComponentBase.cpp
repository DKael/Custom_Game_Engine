#include "LightComponentBase.h"

DEFINE_CLASS(ULightComponentBase, USceneComponent)

void ULightComponentBase::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) 
{
    USceneComponent::GetEditableProperties(OutProps);

	OutProps.push_back({"Color", EPropertyType::LinearColor, &Color});
    OutProps.push_back({"Intensity", EPropertyType::Float, &Intensity});
}

const FColor& ULightComponentBase::GetColor() const { 
	return Color; 
}

void ULightComponentBase::SetColor(const FColor& NewColor) 
{ 
	Color = NewColor; 
	if (OnColorChanged)
	{
		OnColorChanged(Color);
    }
}

// Intensity
float ULightComponentBase::GetIntensity() const 
{ 
	return Intensity; 
}

void ULightComponentBase::SetIntensity(float NewIntensity) 
{
	Intensity = std::max(0.0f, NewIntensity); 
}

void ULightComponentBase::PostEditProperty(const char* PropertyName) 
{
	if (strcmp(PropertyName, "Color") == 0 && OnColorChanged)
	{
		OnColorChanged(Color);
	}
}