#include "FireBallComponent.h"

DEFINE_CLASS(UFireBallComponent, UPrimitiveComponent)
REGISTER_FACTORY(UFireBallComponent)

UFireBallComponent* UFireBallComponent::Duplicate()
{
    UFireBallComponent* NewComp = UObjectManager::Get().CreateObject<UFireBallComponent>();

    NewComp->SetActive(this->IsActive());
    NewComp->SetOwner(nullptr);

    NewComp->SetRelativeLocation(this->GetRelativeLocation());
    NewComp->SetRelativeRotation(this->GetRelativeRotation());
    NewComp->SetRelativeScale(this->GetRelativeScale());

    NewComp->SetVisibility(this->IsVisible());
    NewComp->SetOutlineEnabled(this->IsOutlineEnabled());

    NewComp->Intensity = this->Intensity;
    NewComp->Radius = this->Radius;
    NewComp->RadiusFallOff = this->RadiusFallOff;
    NewComp->Color = this->Color;

    NewComp->DuplicateSubObjects();

    return NewComp;
}

void UFireBallComponent::UpdateWorldAABB() const
{
    WorldAABB.Reset();

    const float HalfExtent = Radius > 0.001f ? Radius : 0.001f;
    const FVector LocalCorners[8] =
    {
        FVector(-HalfExtent, -HalfExtent, -HalfExtent),
        FVector( HalfExtent, -HalfExtent, -HalfExtent),
        FVector(-HalfExtent,  HalfExtent, -HalfExtent),
        FVector( HalfExtent,  HalfExtent, -HalfExtent),
        FVector(-HalfExtent, -HalfExtent,  HalfExtent),
        FVector( HalfExtent, -HalfExtent,  HalfExtent),
        FVector(-HalfExtent,  HalfExtent,  HalfExtent),
        FVector( HalfExtent,  HalfExtent,  HalfExtent)
    };

    const FMatrix& WorldMatrix = GetWorldMatrix();
    for (const FVector& Corner : LocalCorners)
    {
        WorldAABB.Expand(WorldMatrix.TransformPosition(Corner));
    }
}

void UFireBallComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    UPrimitiveComponent::GetEditableProperties(OutProps);

    OutProps.push_back({"Intensity", EPropertyType::Float, &Intensity, 0.0f, 8.0f, 0.05f});
    OutProps.push_back({"Radius", EPropertyType::Float, &Radius});
    OutProps.push_back({"Radius FallOff", EPropertyType::Float, &RadiusFallOff, 0.01f, 1.f, 0.01f});
    OutProps.push_back({"Color", EPropertyType::Vec4, &Color});
}