#pragma once

#include "Component/Light/LightComponent.h"

class USpotLightComponent : public ULightComponent
{
  public:
    DECLARE_CLASS(USpotLightComponent, ULightComponent)
    USpotLightComponent() = default;
    ~USpotLightComponent() = default;

	virtual USpotLightComponent* Duplicate() override;
    virtual USpotLightComponent* DuplicateSubObjects() override { return this; }

	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;

	const FVector GetDirection() const;

	float GetInnerConeAngle() const;
    void  SetInnerConeAngle(float InAngle);

    float GetOuterConeAngle() const;
    void  SetOuterConeAngle(float InAngle);

    void SetConeAngles(float InInnerAnglae, float InOuterAngle);


	float GetRadius() { return Radius; }
    void SetRadius(float NewRadius) { Radius = NewRadius; }


	ELightType GetLightType() const override { return ELightType::Spot; };

  private:
    float InnerConeAngle = 0.0f;
    float OuterConeAngle = 45.0f;
    float MaxConeAngle = 80.0f;

    float Radius = 10.f;
};
