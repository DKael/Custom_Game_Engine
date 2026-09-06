#pragma once

#include "Component/ActorComponent.h"
#include "Render/Common/WaterRenderingCommon.h"

struct FWaterUniformData;

class UWaterComponent : public UActorComponent
{
public:
    DECLARE_CLASS(UWaterComponent, UActorComponent)

    UWaterComponent() = default;
    ~UWaterComponent() override = default;

    void Serialize(FArchive& Ar) override;
    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
    void PostEditProperty(const char* PropertyName) override;

    void FillWaterUniformData(FWaterUniformData& OutData, float TimeSeconds, uint32 LocalLightCount) const;
    void ApplyWaterSurfaceProfile(const FWaterSurfaceProfile& InProfile);

private:
    // Per-object runtime values feed FWaterUniformData directly at draw time.
    FWaterSurfaceProfile SurfaceProfile = {};
};
