#include "Component/HeightFogComponent.h"

DEFINE_CLASS(UHeightFogComponent, UPrimitiveComponent)
REGISTER_FACTORY(UHeightFogComponent)

UHeightFogComponent* UHeightFogComponent::Duplicate()
{
    UHeightFogComponent* NewComp = UObjectManager::Get().CreateObject<UHeightFogComponent>();

    NewComp->SetActive(this->IsActive());
    NewComp->SetOwner(nullptr);

    NewComp->SetRelativeLocation(this->GetRelativeLocation());
    NewComp->SetRelativeRotation(this->GetRelativeRotation());
    NewComp->SetRelativeScale(this->GetRelativeScale());

    NewComp->FogInscatteringColor = this->FogInscatteringColor;
    NewComp->FogDensity           = this->FogDensity;
    NewComp->FogHeightFalloff     = this->FogHeightFalloff;
    NewComp->StartDistance        = this->StartDistance;
    NewComp->FogCutoffDistance    = this->FogCutoffDistance;
    NewComp->FogMaxOpacity        = this->FogMaxOpacity;

    NewComp->DuplicateSubObjects();

    return NewComp;
}

void UHeightFogComponent::UpdateWorldAABB() const
{
	WorldAABB.Reset();

	// The shader considers the decal volume as a unit cube [-0.5, 0.5] 
	// transformed by the WorldMatrix (which now includes DecalSize).
	static constexpr FVector LocalCorners[8] =
	{
		FVector(-0.5f, -0.5f, -0.5f),
		FVector(0.5f, -0.5f, -0.5f),
		FVector(-0.5f,  0.5f, -0.5f),
		FVector(0.5f,  0.5f, -0.5f),
		FVector(-0.5f, -0.5f,  0.5f),
		FVector(0.5f, -0.5f,  0.5f),
		FVector(-0.5f,  0.5f,  0.5f),
		FVector(0.5f,  0.5f,  0.5f)
	};

	const FMatrix& WorldMatrix = GetWorldMatrix();

	for (const FVector& Corner : LocalCorners)
	{
		const FVector WorldPos = WorldMatrix.TransformPosition(Corner);
		WorldAABB.Expand(WorldPos);
	}
}

bool UHeightFogComponent::RaycastMesh(const FRay& Ray, FHitResult& OutHitResult)
{
	UpdateWorldAABB();

	float HitT = 0.0f;
	if (WorldAABB.IntersectRay(Ray, HitT))
	{
		OutHitResult.bHit = true;
		OutHitResult.HitComponent = this;
		OutHitResult.Distance = HitT;
		OutHitResult.Location = Ray.Origin + Ray.Direction * HitT;

		OutHitResult.Normal = -Ray.Direction;
		return true;
	}

	return false;
}

void UHeightFogComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    UPrimitiveComponent::GetEditableProperties(OutProps);

    OutProps.push_back({ "Inscattering Color", EPropertyType::LinearColor, &FogInscatteringColor });
    OutProps.push_back({ "Fog Density",        EPropertyType::Float, &FogDensity,        0.0f,  1.0f,   0.001f });
    OutProps.push_back({ "Height Falloff",     EPropertyType::Float, &FogHeightFalloff,  0.0f,  10.0f,  0.01f  });
    OutProps.push_back({ "Start Distance",     EPropertyType::Float, &StartDistance,     0.0f,  5000.0f, 1.0f  });
    OutProps.push_back({ "Cutoff Distance",    EPropertyType::Float, &FogCutoffDistance, 0.0f,  50000.0f, 10.0f });
    OutProps.push_back({ "Max Opacity",        EPropertyType::Float, &FogMaxOpacity,     0.0f,  1.0f,   0.01f  });
}
