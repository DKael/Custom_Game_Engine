#pragma once

#include "Core/CoreTypes.h"
#include "Math/Color.h"

// Shared runtime water look. Components own instances; materials remain asset defaults.
struct FWaterSurfaceProfile
{
    float NormalStrength = 0.45f;
    float Alpha = 1.0f;
    float ColorVariationStrength = 0.15f;

    float NormalTilingAX = 4.0f;
    float NormalTilingAY = 4.0f;
    float NormalScrollSpeedAX = 0.03f;
    float NormalScrollSpeedAY = 0.01f;

    float NormalTilingBX = 2.5f;
    float NormalTilingBY = 2.5f;
    float NormalScrollSpeedBX = -0.02f;
    float NormalScrollSpeedBY = 0.015f;

    float WorldUVScaleX = 0.02f;
    float WorldUVScaleY = 0.02f;
    float WorldUVBlendFactor = 1.0f;

    FColor BaseColor = FColor(0.08f, 0.22f, 0.33f, 1.0f);
    float WaterSpecularPower = 96.0f;
    float WaterSpecularIntensity = 0.75f;
    float WaterFresnelPower = 5.0f;
    float WaterFresnelIntensity = 0.45f;
    float WaterLightContributionScale = 1.0f;
    float HorizonFadeStart = -0.08f;
    float HorizonFadeEnd = 0.06f;
    float NdotLFadeWidth = 0.08f;
    bool bEnableWaterSpecular = true;
};

// Water rendering uses a dedicated shader override, but still flows through the
// normal forward static-mesh path. Keep the resource contract centralized here
// so C++ binding code and HLSL register comments stay in sync.
namespace WaterShaderBindings
{
    constexpr uint32 MaterialConstantBuffer = 2u;   // b2 in Shaders/Water.hlsl
    constexpr uint32 FirstTextureRegister = 0u;     // t0 = WaterNormalA
    constexpr uint32 NormalATexture = 0u;           // t0
    constexpr uint32 NormalBTexture = 1u;           // t1
    constexpr uint32 DiffuseTexture = 2u;           // t2
    constexpr uint32 TextureCount = 3u;             // t0-t2
    constexpr uint32 GlobalLightInfoConstantBuffer = 3u; // b3
    constexpr uint32 GlobalLightStructuredBuffer = 3u;   // t3
    constexpr uint32 VisibleLightInfoConstantBuffer = 4u; // b4
    constexpr uint32 PointLightStructuredBuffer = 8u;     // t8
    constexpr uint32 SpotLightStructuredBuffer = 9u;      // t9
}

namespace WaterMaterialParameterNames
{
    constexpr const char* IsWater = "bIsWater";
    constexpr const char* DiffuseMap = "DiffuseMap";
    constexpr const char* NormalMap = "NormalMap";
    constexpr const char* WaterNormalA = "WaterNormalA";
    constexpr const char* WaterNormalB = "WaterNormalB";
}

namespace WaterRenderingLimits
{
    constexpr uint32 MaxLocalLights = 8u;
}

namespace WaterDefaultAssets
{
    // Default editor spawn asset only. Runtime water logic must not assume a
    // specific mesh beyond "reasonable flat water surface".
    constexpr const char* MeshPath = "Asset/Mesh/Water/Wave.obj";
    constexpr const char* FlatTileMeshPath = "Asset/Mesh/Water/FlatTile.obj";
}
