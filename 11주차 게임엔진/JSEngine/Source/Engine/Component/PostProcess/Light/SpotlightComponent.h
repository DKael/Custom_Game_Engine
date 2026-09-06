#pragma once
#include "PointLightComponent.h"
#include "Generated/SpotlightComponent.generated.h"

UCLASS()
class USpotlightComponent : public UPointLightComponent
{
public:
    GENERATED_BODY()

    void PostDuplicate(UObject* Origiunal) override;
    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;

	void Serialize(FArchive& Ar) override;

protected:
	FMatrix ComputeCascadeShadowMatrix(const FMatrix& CamView, const FMatrix& CamProj,
		float SplitNearT, float SplitFarT) const override;
	FMatrix ComputePerspectiveShadowMatrix(const FMatrix& CamView, const FMatrix& CamProj,
		const TArray<FBoundingBox>* VisibleObjectsBounds) const override;

public:
    UPROPERTY(EditAnywhere, DisplayName="Inner Cone Angle", SerializeName="InnerConeAngle", Speed=0.1, Animatable)
    float InnerConeAngle = 10.f;
    UPROPERTY(EditAnywhere, DisplayName="Outer Cone Angle", SerializeName="OuterConeAngle", Speed=0.1, Animatable)
    float OuterConeAngle = 15.f;
};
