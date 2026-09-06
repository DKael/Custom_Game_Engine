#include "DecalComponent.h"
#include <cstring>
#include "Core/ResourceManager.h"
#include "Render/Resource/Material.h"

DEFINE_CLASS(UDecalComponent, UPrimitiveComponent)
REGISTER_FACTORY(UDecalComponent)

UDecalComponent* UDecalComponent::Duplicate()
{
    UDecalComponent* NewComp = UObjectManager::Get().CreateObject<UDecalComponent>();

    NewComp->SetActive(this->IsActive());
    NewComp->SetOwner(nullptr);
    
    NewComp->SetRelativeLocation(this->GetRelativeLocation());
    NewComp->SetRelativeRotation(this->GetRelativeRotation());
    NewComp->SetRelativeScale(this->GetRelativeScale());
    
    NewComp->SetVisibility(this->IsVisible());
    NewComp->SetOutlineEnabled(this->IsOutlineEnabled());

    NewComp->DecalMaterial = this->DecalMaterial;
    NewComp->MaterialName = this->MaterialName;
    NewComp->SortOrder = this->SortOrder;
    NewComp->FadeAmount = this->FadeAmount;

    NewComp->bDistanceFade = this->bDistanceFade;
    NewComp->FadeStartDistance = this->FadeStartDistance;
    NewComp->FadeEndDistance = this->FadeEndDistance;

    NewComp->DuplicateSubObjects();

    return NewComp;
}

void UDecalComponent::UpdateWorldAABB() const
{
    WorldAABB.Reset();

    // The shader considers the decal volume as a unit cube [-0.5, 0.5] 
    // transformed by the WorldMatrix (which now includes DecalSize).
    static constexpr FVector LocalCorners[8] =
    {
        FVector(-0.5f, -0.5f, -0.5f),
        FVector( 0.5f, -0.5f, -0.5f),
        FVector(-0.5f,  0.5f, -0.5f),
        FVector( 0.5f,  0.5f, -0.5f),
        FVector(-0.5f, -0.5f,  0.5f),
        FVector( 0.5f, -0.5f,  0.5f),
        FVector(-0.5f,  0.5f,  0.5f),
        FVector( 0.5f,  0.5f,  0.5f)
    };

    const FMatrix& WorldMatrix = GetWorldMatrix();

    for (const FVector& Corner : LocalCorners)
    {
        const FVector WorldPos = WorldMatrix.TransformPosition(Corner);
        WorldAABB.Expand(WorldPos);
    }
}

bool UDecalComponent::RaycastMesh(const FRay& Ray, FHitResult& OutHitResult)
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

void UDecalComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    UPrimitiveComponent::GetEditableProperties(OutProps);
    
    OutProps.push_back({"Material", EPropertyType::String, &MaterialName});
    OutProps.push_back({"Sort Order", EPropertyType::Int, &SortOrder});
    OutProps.push_back({"Fade Amount", EPropertyType::Float, &FadeAmount});
    OutProps.push_back({"Distance Fade", EPropertyType::Bool, &bDistanceFade});
    OutProps.push_back({"Fade Start Distance", EPropertyType::Float, &FadeStartDistance, 0.0f, 10000.0f, 10.0f});
    OutProps.push_back({"Fade End Distance", EPropertyType::Float, &FadeEndDistance, 0.0f, 10000.0f, 10.0f});
    OutProps.push_back({"bUseSurfaceNormal", EPropertyType::Bool, &bUseSurfaceNormal});
}

void UDecalComponent::PostEditProperty(const char* PropertyName)
{
    UPrimitiveComponent::PostEditProperty(PropertyName);

    if (strcmp(PropertyName, "Material") == 0)
    {
        if (!MaterialName.empty())
        {
            FMaterial* Mat = FResourceManager::Get().FindMaterial(MaterialName);
            if (Mat)
            {
                SetDecalMaterial(Mat);
            }
        }
    }
}

void UDecalComponent::SetDecalMaterial(FMaterial* InMaterial)
{
    if (DecalMaterial != InMaterial)
    {
        DecalMaterial = InMaterial;
        MaterialName = DecalMaterial ? DecalMaterial->Name : "";
        MarkRenderStateDirty();
    }
}

void UDecalComponent::MarkRenderStateDirty()
{
    // Notify renderer if needed
}
