#pragma once

#include "LightComponent.h"

class UDirectionalLightComponent : public ULightComponent
{
public:
	DECLARE_CLASS(UDirectionalLightComponent, ULightComponent)

	UDirectionalLightComponent() = default;
	~UDirectionalLightComponent() = default;

	virtual UDirectionalLightComponent* Duplicate() override;
    virtual UDirectionalLightComponent* DuplicateSubObjects() override { return this; }

	ELightType GetLightType() const override;

	FVector GetLightDirection() const;
};