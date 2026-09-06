#pragma once
#include "LightComponent.h"
#include "Generated/AmbientLightComponent.generated.h"

UCLASS()
class UAmbientLightComponent : public ULightComponent {
public:
	GENERATED_BODY()

	UAmbientLightComponent() = default;
};