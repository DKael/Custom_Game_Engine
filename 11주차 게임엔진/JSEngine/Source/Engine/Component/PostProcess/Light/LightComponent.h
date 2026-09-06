#pragma once
#include "LightComponentBase.h"
#include "Render/Common/ShadowTypes.h"
#include "Core/EngineTypes.h"
#include "Generated/LightComponent.generated.h"

class UMaterialInterface;

UCLASS()
class ULightComponent : public ULightComponentBase {
public:
	GENERATED_BODY()

	ULightComponent() = default;

	FMatrix GetLightViewProj(const FMatrix& CamView, const FMatrix& CamProj,
		const TArray<FBoundingBox>* VisibleObjectsBounds = nullptr) const;
	
	/* Cascade ShadowMap 전용 */
	FMatrix GetLightViewProj(const FMatrix& CamView, const FMatrix& CamProj,
		float SplitNearT, float SplitFarT, const TArray<FBoundingBox>* VisibleObjectsBounds = nullptr) const;

	void PostDuplicate(UObject* Original) override;
	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	
	void Serialize(FArchive& Ar) override;

public:
	EShadowMap GetShadowMapType() const { return eShadowMapType; }
	void SetShadowMapType(EShadowMap InType) { eShadowMapType = InType; }

protected:
	virtual FMatrix ComputePerspectiveShadowMatrix(const FMatrix& CamView, const FMatrix& CamProj,
		const TArray<FBoundingBox>* VisibleObjectsBounds) const { return FMatrix::Identity; }

	virtual FMatrix ComputeCascadeShadowMatrix(const FMatrix& CamView, const FMatrix& CamProj,
		float SplitNearT, float SplitFarT) const;

	virtual void PrintShadowMapDebugInfo(TArray<FPropertyDescriptor>& OutProps) const;

protected:
	~ULightComponent() = default;

public:
	UPROPERTY(EditAnywhere, DisplayName="Shadow Resolution Scale", SerializeName="ShadowResolutionScale")
	int32 ShadowResolutionScale = 2048;
	UPROPERTY(EditAnywhere, DisplayName="Constant Bias ( DepthBias ^ (1 / TextureBit))", SerializeName="ConstantBias", Min=0.0, Max=0.01, Speed=0.001)
	float ConstantBias = { 0.003f };
	UPROPERTY(EditAnywhere, DisplayName="Slope-Scaled Bias", SerializeName="SlopeScaledBias", Min=0.0, Max=1.0, Speed=0.01)
	float SlopeScaledBias = { 0.12f } ;
	UPROPERTY(EditAnywhere, DisplayName="Shadow Sharpen", SerializeName="ShadowSharpen")
	float ShadowSharpen = 0.5f;

	// 디버그용으로 Shadow Atlas에서 해당 라이트의 타일 위치와 크기를 저장하는 변수, 현재 지워도됩니다
    FVector4 DebugShadowAtlasScaleOffset;
    bool bHasDebugShadowAtlasTile = false;
    float DebugShadowCubeIndex;
    bool bHasDebugShadowCubeTile = false;
protected:
	UPROPERTY(EditAnywhere, DisplayName="ShadowMapType", SerializeName="ShadowMapType")
	EShadowMap eShadowMapType = EShadowMap::CSM;
};
