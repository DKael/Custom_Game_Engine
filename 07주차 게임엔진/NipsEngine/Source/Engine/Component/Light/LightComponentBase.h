#pragma once

#include "Component/SceneComponent.h"

class ULightComponentBase : public USceneComponent
{
  public:
    DECLARE_CLASS(ULightComponentBase, USceneComponent)

    ULightComponentBase() = default;
    ~ULightComponentBase() = default;

	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
    void PostEditProperty(const char* PropertyName) override;

    const FColor& GetColor() const;
    void   SetColor(const FColor& NewColor);

	float GetIntensity() const;
    void  SetIntensity(float NewIntensity);

	void SetOnColorChanged(std::function<void(const FColor&)> Callback) { OnColorChanged = std::move(Callback); }

  private:
    FColor Color;
    float  Intensity = 1.0f;
    std::function<void(const FColor&)> OnColorChanged;
};
