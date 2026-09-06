#pragma once
#include "PrimitiveComponent.h"
#include "Math/Color.h"

class UFireBallComponent : public UPrimitiveComponent
{
  public:
    DECLARE_CLASS(UFireBallComponent, UPrimitiveComponent)

    UFireBallComponent() = default;

    virtual UFireBallComponent* Duplicate() override;
    virtual void                UpdateWorldAABB() const override;
    virtual bool                RaycastMesh(const FRay& Ray, FHitResult& OutHitResult) override { return false; }
    virtual EPrimitiveType      GetPrimitiveType() const override { return EPrimitiveType::EPT_FireBall; }
    virtual bool                SupportsOutline() const override { return true; }

    virtual void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;

    FMatrix         GetWorldMatrixWithRadius() const { return GetWorldMatrix().ApplyScale(2 * Radius); }
    virtual FMatrix GetWorldToDecalMatrix() const { return GetWorldMatrix().GetInverse(); }

    float         GetIntensity() const { return Intensity; }
    float         GetRadius() const { return Radius; }
    float         GetRadiusFallOff() const { return RadiusFallOff; }
    const FColor& GetColor() const { return Color; }

    void SetRadius(float InRadius) { Radius = InRadius > 0.f ? InRadius : 0.f; }
    void SetIntensity(float InIntensity) { Intensity = InIntensity > 0.f ? InIntensity : 0.f; }
    void SetRadiusFallOff(float InRadiusFallOff) { RadiusFallOff = InRadiusFallOff > 0.f ? InRadiusFallOff : 0.f; }

  private:
    float  Intensity = 1.0f;
    float  Radius = 0.5f;
    float  RadiusFallOff = 0.2f;
    FColor Color = FColor(1.0f, 0.45f, 0.1f, 0.85f);
};
