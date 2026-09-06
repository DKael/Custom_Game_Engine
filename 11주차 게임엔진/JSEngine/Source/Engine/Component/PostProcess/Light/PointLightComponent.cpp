#include "PointLightComponent.h"
#include "Object/ObjectFactory.h"
#include <Render/Resource/ShadowAtlasManager.h>

void UPointLightComponent::PostDuplicate(UObject* Original)
{
    ULightComponent::PostDuplicate(Original);
}

void UPointLightComponent::Serialize(FArchive& Ar)
{
	ULightComponent::Serialize(Ar);
}

FMatrix UPointLightComponent::ComputePerspectiveShadowMatrix(const FMatrix& CamView, const FMatrix& CamProj,
	const TArray<FBoundingBox>* VisibleObjectsBounds) const
{
	return FMatrix::Identity;
}

