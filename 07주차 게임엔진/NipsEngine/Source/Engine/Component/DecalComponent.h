#pragma once
#include "PrimitiveComponent.h"

/**
 * UDecalComponent is used to project a material onto existing scene geometry.
 * It uses a box volume to define the projection area.
 */
class UDecalComponent : public UPrimitiveComponent
{
public:
    DECLARE_CLASS(UDecalComponent, UPrimitiveComponent)

    UDecalComponent(){}

    virtual UDecalComponent* Duplicate() override;
	virtual UDecalComponent* DuplicateSubObjects() override { return this;  };
    virtual void             UpdateWorldAABB() const override;
    virtual bool             RaycastMesh(const FRay& Ray, FHitResult& OutHitResult) override;
    virtual EPrimitiveType   GetPrimitiveType() const override { return EPrimitiveType::EPT_Decal; }
    virtual bool             SupportsOutline() const override { return false; }

    virtual void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
    virtual void PostEditProperty(const char* PropertyName) override;

    void SetDecalMaterial(class FMaterial* InMaterial);
    class FMaterial* GetDecalMaterial() const { return DecalMaterial; }
    virtual FMatrix          GetWorldToDecalMatrix() const { return GetWorldMatrix().GetInverse(); }

    void SetSortOrder(int32 InSortOrder) { SortOrder = InSortOrder; }
    int32 GetSortOrder() const { return SortOrder; }

    void SetFadeAmount(float InFade) { FadeAmount = InFade; }
    float GetFadeAmount() const { return FadeAmount; }

    void SetDistanceFade(bool bEnable) { bDistanceFade = bEnable; }
    bool IsDistanceFadeEnabled() const { return bDistanceFade; }
    void SetFadeStartDistance(float InDistance) { FadeStartDistance = InDistance; }
    float GetFadeStartDistance() const { return FadeStartDistance; }
    void SetFadeEndDistance(float InDistance) { FadeEndDistance = InDistance; }
    float GetFadeEndDistance() const { return FadeEndDistance; }
    bool  IsUsingSurfaceNormal() const { return bUseSurfaceNormal; }

private:
    class FMaterial* DecalMaterial = nullptr;
    FString MaterialName;
    int32 SortOrder = 0;
    float FadeAmount = 1.0f;

    bool bDistanceFade = false;
    float FadeStartDistance = 100.0f;
    float FadeEndDistance = 1000.0f;

	bool bUseSurfaceNormal = true;

    void MarkRenderStateDirty();
};
