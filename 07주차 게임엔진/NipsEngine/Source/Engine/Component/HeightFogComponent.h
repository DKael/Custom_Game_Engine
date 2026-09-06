#pragma once
#include "Component/PrimitiveComponent.h"
#include "Math/Vector4.h"

/*
    씬에 Exponential Height Fog 파라미터를 공급하는 컴포넌트.
    메시처럼 드로우콜을 생성하지 않고, RenderCollector가 월드에서 꺼내
    RenderBus::SetFogSettings()에 전달하는 방식으로 동작한다.
*/
class UHeightFogComponent : public UPrimitiveComponent
{
public:
    DECLARE_CLASS(UHeightFogComponent, UPrimitiveComponent)

    UHeightFogComponent() = default;

    virtual UHeightFogComponent* Duplicate() override;
    virtual UHeightFogComponent* DuplicateSubObjects() override { return this; }

    // 포그는 형상이 없으므로 AABB는 점으로, 레이캐스트는 항상 miss
    virtual void           UpdateWorldAABB() const override;
	virtual bool           RaycastMesh(const FRay& Ray, FHitResult& OutHitResult) override;
    virtual EPrimitiveType GetPrimitiveType() const override { return EPrimitiveType::EPT_Fog; }

    // 포그 컴포넌트는 선택 시 아웃라인이 필요 없음
    virtual bool SupportsOutline() const override { return false; }

    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;

    FVector4 GetInscatteringColor() const { return FogInscatteringColor; }
    float    GetDensity()           const { return FogDensity; }
    float    GetHeightFalloff()     const { return FogHeightFalloff; }
    float    GetStartDistance()     const { return StartDistance; }
    float    GetCutoffDistance()    const { return FogCutoffDistance; }
    float    GetMaxOpacity()        const { return FogMaxOpacity; }

protected:
    FVector4 FogInscatteringColor = FVector4(0.5f, 0.6f, 0.7f, 1.0f);

    float FogDensity       = 0.10f;
    float FogHeightFalloff = 0.2f;
    float StartDistance    = 0.0f;
    float FogCutoffDistance = 0.0f;
    float FogMaxOpacity    = 1.0f;
};
