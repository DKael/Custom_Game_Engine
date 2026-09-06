#pragma once

#include "Component/Light/LightComponentBase.h"

enum ELightType
{
	Ambient,
	Directional,
	Point,
	Spot
};

class ULightComponent : public ULightComponentBase
{
	public:
    DECLARE_CLASS(ULightComponent, ULightComponentBase)

    ULightComponent() = default;
    ~ULightComponent() = default;

	virtual ELightType GetLightType() const = 0;
};
