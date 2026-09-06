#pragma once
#include "LightComponent.h"
#include "Generated/DirectionalLightComponent.generated.h"

UCLASS()
class UDirectionalLightComponent : public ULightComponent
{
public:
    GENERATED_BODY()

protected:
	FMatrix ComputePerspectiveShadowMatrix(const FMatrix& CamView, const FMatrix& CamProj,
		const TArray<FBoundingBox>* VisibleObjectsBounds) const override;

public:
	UPROPERTY(EditAnywhere, DisplayName="MaxDistance", Min=0.0, Max=1000.0, Speed=10.0)
	float CSMMaxDistance = { 300.f };
	UPROPERTY(EditAnywhere, DisplayName="Lambda", Min=0.0, Max=1.0, Speed=0.01)
	float CSMPractialLambda = { 0.25f };

};
