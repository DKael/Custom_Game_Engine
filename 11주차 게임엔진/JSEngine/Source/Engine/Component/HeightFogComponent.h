#pragma once
#include "PrimitiveComponent.h"
#include "Generated/HeightFogComponent.generated.h"

UCLASS()
class UHeightFogComponent : public UPrimitiveComponent
{
public:
    GENERATED_BODY()

    UHeightFogComponent();
    ~UHeightFogComponent() override = default;

	virtual void Serialize(FArchive& Ar) override;

    EPrimitiveType GetPrimitiveType() const override { return EPrimitiveType::EPT_FOG; }

    void SetFogDensity(float InFogDensity) { FogDensity = InFogDensity; }
    float GetFogDensity() const { return FogDensity; }

    void SetHeightFalloff(float InHeightFalloff) { HeightFalloff = InHeightFalloff; }
    float GetHeightFalloff() const { return HeightFalloff; }

    void SetFogInscatteringColor(const FVector4& InColor) { FogInscatteringColor = FColor(InColor.X, InColor.Y, InColor.Z, InColor.W); }
    FVector4 GetFogInscatteringColor() const { return FogInscatteringColor.ToVector4(); }

    void SetFogHeight(float InFogHeight) { FogHeight = InFogHeight; }
    float GetFogHeight() const { return FogHeight; }

    void SetFogStartDistance(float InFogStartDistance) { FogStartDistance = InFogStartDistance; }
    float GetFogStartDistance() const { return FogStartDistance; }

    void SetFogCutoffDistance(float InCutoffDistance) { FogCutoffDistance = InCutoffDistance; }
    float GetFogCutoffDistance() const { return FogCutoffDistance; }

    void SetFogMaxOpacity(float InFogMaxOpacity) { FogMaxOpacity = InFogMaxOpacity; }
    float GetFogMaxOpacity() const { return FogMaxOpacity; }

    // --- Property / Serialization ---
    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
    void PostEditProperty(const char* PropertyName) override;

protected:
    FPrimitiveRenderProxy* CreateRenderProxy() override;

private:
    UPROPERTY(EditAnywhere, DisplayName="FogInscatteringColor")
    FColor FogInscatteringColor;
    UPROPERTY(EditAnywhere, DisplayName="FogDensity", Min=0.0, Max=1.0, Speed=0.01)
    float FogDensity = 0;
    UPROPERTY(EditAnywhere, DisplayName="HeightFalloff", Min=0.0, Max=10.0, Speed=0.01)
    float HeightFalloff = 0;
    UPROPERTY(EditAnywhere, DisplayName="FogHeight")
    float FogHeight = 0;
    UPROPERTY(EditAnywhere, DisplayName="FogStartDistance", Min=0.0)
    float FogStartDistance = 0;
    UPROPERTY(EditAnywhere, DisplayName="FogCutoffDistance")
    float FogCutoffDistance = 1000;
    UPROPERTY(EditAnywhere, DisplayName="FogMaxOpacity", Min=0.0, Max=1.0, Speed=0.01)
    float FogMaxOpacity = 1.f;

    // UPrimitiveComponent을(를) 통해 상속됨
    void UpdateWorldAABB() const override;
    bool RaycastMesh(const FRay& Ray, FHitResult& OutHitResult) override;
};
