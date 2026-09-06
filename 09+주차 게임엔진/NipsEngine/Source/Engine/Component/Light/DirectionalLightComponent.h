#pragma once
#include "LightComponent.h"
#include <algorithm>

enum class EShadowMode
{
    CSM = 0,
    PSM,
    Max
};

class UDirectionalLightComponent : public ULightComponent
{
public:
    DECLARE_CLASS(UDirectionalLightComponent, ULightComponent)

    static constexpr const char* BillboardTexturePath = "Asset/Texture/Icons/S_LightDirectional.PNG";

    UDirectionalLightComponent();
    ~UDirectionalLightComponent() override = default;

    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
    void Serialize(FArchive& Ar) override;
    void PostDuplicate(UObject* Original) override;

    const char* GetBillboardTexturePath() const override { return BillboardTexturePath; }

    // Directional shadow settings.
    EShadowMode GetShadowMode() const { return ShadowMode; }
    void SetShadowMode(EShadowMode InShadowMode)
    {
        const int32 ShadowModeValue = static_cast<int32>(InShadowMode);
        if (ShadowModeValue >= 0 && ShadowModeValue < static_cast<int32>(EShadowMode::Max))
        {
            ShadowMode = InShadowMode;
        }
    }

    float GetShadowDistance() const { return ShadowDistance; }
    float GetCascadeSplitWeight() const { return CascadeSplitWeight;  }
    float GetPSMVirtualSlideBack() const { return PSMVirtualSlideBack; }
    bool IsDayNightAttenuationEnabled() const { return bDayNightAttenuationEnabled; }
    void SetPSMVirtualSlideBack(float InVirtualSlideBack)
    {
        PSMVirtualSlideBack = std::max(0.0f, InVirtualSlideBack);
    }
    void SetDayNightAttenuationEnabled(bool bInEnabled)
    {
        bDayNightAttenuationEnabled = bInEnabled;
    }

private:
    EShadowMode ShadowMode = EShadowMode::CSM;
    // Kept per-instance because each light can use a different cascade range.
    float ShadowDistance = 500.0f;
    // 0.0f = linear split, 1.0f = logarithmic split (higher precision near camera).
    float CascadeSplitWeight = 0.5f;
    float PSMVirtualSlideBack = 100.0f;
    bool bDayNightAttenuationEnabled = true;
};
