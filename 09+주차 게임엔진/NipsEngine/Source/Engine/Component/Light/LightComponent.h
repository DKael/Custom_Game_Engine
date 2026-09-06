#pragma once
#include "Component/SceneComponent.h"
#include "Component/BillboardComponent.h"
#include "Render/Common/RenderTypes.h"
#include "GameFramework/World.h"
#include "Core/PropertyTypes.h"

class ULightComponentBase : public USceneComponent
{
public:
    DECLARE_CLASS(ULightComponentBase, USceneComponent)

    ULightComponentBase() = default;
    ~ULightComponentBase() override = default;
    
    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
    void PostEditProperty(const char* PropertyName) override;

    void PostDuplicate(UObject* Original) override;

    void Serialize(FArchive& Ar) override;

    void BeginPlay() override;
    void EndPlay() override;

    virtual const char* GetBillboardTexturePath() const { return nullptr; }

public:
    const FColor& GetLightColor() const { return LightColor; }
    float GetIntensity() const { return Intensity; }
    bool IsVisible() const { return bVisible; }
    bool IsCastShadows() const { return bCastShadows; } // Keep UE-style meaning with engine naming consistency.
    bool IsDebugDrawEnabled() const { return bDebugDraw; }

    void SetLightColor(const FColor& InColor) { LightColor = InColor; }
    void SetIntensity(float InIntensity) { Intensity = InIntensity; }
    void SetVisible(bool bInVisible) { bVisible = bInVisible; }
    void SetCastShadows(bool bInCastShadows) { bCastShadows = bInCastShadows; }
    void SetDebugDrawEnabled(bool bInDebugDraw) { bDebugDraw = bInDebugDraw; }

    const FLightHandle& GetLightHandle() const { return LightHandle; }
    void SetLightHandle(const FLightHandle& InLightHandle) { LightHandle = InLightHandle; }

private:
    void SerializeLightCommon(FArchive& Ar);
    void CopyDuplicateStateFrom(const ULightComponentBase& Original);

protected:
    void RegisterComponentWithWorld(class UWorld& World) override;
    void UnregisterComponentFromWorld(class UWorld& World) override;

private:
    FColor LightColor = FColor(1.0f, 1.0f, 1.0f, 1.0f);
    float Intensity = 1.0f;
    bool bVisible = true;
    bool bCastShadows = true;
    bool bDebugDraw = false;

    FLightHandle LightHandle;
};

class ULightComponent : public ULightComponentBase
{
public:
    DECLARE_CLASS(ULightComponent, ULightComponentBase)

    ULightComponent();
    ~ULightComponent() override = default;

    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
    void Serialize(FArchive& Ar) override;
    void PostDuplicate(UObject* Original) override;

    float GetShadowResolutionScale() const { return ShadowResolutionScale; }
    float GetShadowBias() const { return ShadowBias; }
    float GetShadowSlopeBias() const { return ShadowSlopeBias; }
    float GetShadowSharpen() const { return ShadowSharpen; }
    bool IsShadowTexelSnapped() const { return bShadowTexelSnapped; }

public:
    ELightType GetLightType() const { return LightType; }

protected:
    void SetLightType(ELightType InLightType) { LightType = InLightType; }
    bool bShadowTexelSnapped = true;

private:
    void SerializeShadowSettings(FArchive& Ar);
    void CopyDuplicateStateFrom(const ULightComponent& Original);

    ELightType LightType = ELightType::Max;

    float ShadowResolutionScale = 1.0f;
    float ShadowBias = 0.001f;
    float ShadowSlopeBias = 1.0f;
    float ShadowSharpen = 0.5f; 
};
