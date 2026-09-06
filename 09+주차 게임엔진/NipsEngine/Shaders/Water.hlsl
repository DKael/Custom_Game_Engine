#ifndef NIPS_SAMPLE_STATE_DECLARED
#define NIPS_SAMPLE_STATE_DECLARED
SamplerState SampleState : register(s0);
#endif

// b0: shared frame data populated from FShaderBindingInstance::ApplyFrameParameters
cbuffer WaterFrame : register(b0)
{
    row_major float4x4 View;
    row_major float4x4 Projection;
    float3 CameraPosition;
    float _WaterFramePad0;
    float bIsWireframe;
    float bLightingEnabled;
    float _WaterFramePad1;
    float _WaterFramePad2;
    float3 WireframeRGB;
    float _WaterFramePad3;
}

// b1: per-object transform data populated from FPerObjectConstants
cbuffer WaterPerObject : register(b1)
{
    row_major float4x4 Model;
    row_major float4x4 WorldInvTrans;
    float4 PrimitiveColor;
}

// b2: per-draw water data from UWaterComponent -> FWaterUniformData.
// Runtime water animation/highlight values are not written into shared material instances.
cbuffer WaterMaterial : register(b2)
{
    float Time;
    float NormalStrength;
    float Alpha;
    float WaterSpecularPower;

    float2 NormalTilingA;
    float2 NormalScrollSpeedA;

    float2 NormalTilingB;
    float2 NormalScrollSpeedB;

    float3 BaseColor;
    float WaterSpecularIntensity;

    uint bHasNormalMapA;
    uint bHasNormalMapB;
    float ColorVariationStrength;
    float WaterFresnelPower;

    float WaterFresnelIntensity;
    float WaterLightContributionScale;
    uint bEnableWaterSpecular;
    uint WaterLocalLightCount;

    uint bHasDiffuseMap;
    float WorldUVScaleX;
    float WorldUVScaleY;
    float WorldUVBlendFactor;

    float HorizonFadeStart;
    float HorizonFadeEnd;
    float NdotLFadeWidth;
    float _WaterDirectionalFadePad0;
}

// t0/t1: optional water normal or noise textures.
// t2: optional diffuse tint texture from mesh material slot.
Texture2D WaterNormalA : register(t0);
Texture2D WaterNormalB : register(t1);
Texture2D DiffuseMap : register(t2);

// b3/t3 are shared with the forward lighting path.
// Direction convention:
// Global directional light Direction is treated as "surface -> light" in this engine.
cbuffer WaterLightingInfo : register(b3)
{
    uint SceneGlobalLightCount;
    float3 _WaterLightingInfoPad0;
}

struct FGPULight
{
    uint Type;
    float Intensity;
    float Radius;
    float FalloffExponent;

    float3 Color;
    float SpotInnerCos;

    float3 Position;
    float SpotOuterCos;

    float3 Direction;
    uint bCastShadows;

    int ShadowMapIndex;
    float ShadowBias;
    float Padding0;
    float Padding1;
};

StructuredBuffer<FGPULight> GlobalLights : register(t3);

// b4/t8/t9 are reused from the existing local-light forward buffers.
// WaterLocalLightCount caps both point and spot loops for stable prototype cost.
cbuffer VisibleLightInfo : register(b4)
{
    uint TileCountX;
    uint TileCountY;
    uint TileSize;
    uint MaxPointLightsPerTile;
    uint MaxSpotLightsPerTile;
    uint PointLightCount;
    uint SpotLightCount;
    float _VisibleLightInfoPad0;
}

struct FVisibleLightData
{
    float3 WorldPos;
    float Radius;

    float3 Color;
    float Intensity;

    float RadiusFalloff;
    uint Type;
    float SpotInnerCos;
    float SpotOuterCos;

    float3 Direction;
    uint bCastShadows;

    int ShadowMapIndex;
    float ShadowBias;
    float Padding0;
    float Padding1;
};

StructuredBuffer<FVisibleLightData> PointLights : register(t8);
StructuredBuffer<FVisibleLightData> SpotLights : register(t9);

static const uint LIGHT_TYPE_DIRECTIONAL = 0u;
static const uint MAX_WATER_LOCAL_LIGHTS = 8u;

struct FWaterVSInput
{
    float3 Position : POSITION;
    float4 Color : COLOR;
    float3 Normal : NORMAL;
    float2 UV : TEXCOORD;
    float3 Tangent : TANGENT;
    float3 Bitangent : BITANGENT;
};

struct FWaterPSInput
{
    float4 ClipPos : SV_POSITION;
    float3 WorldPos : TEXCOORD0;
    float3 WorldNormal : TEXCOORD1;
    float2 UV : TEXCOORD2;
};

struct FWaterPSOutput
{
    float4 Color : SV_TARGET0;
    float4 Normal : SV_TARGET1;
    float4 WorldPos : SV_TARGET2;
};

float4 ApplyWaterMVP(float3 Position)
{
    const float4 WorldPosition = mul(float4(Position, 1.0f), Model);
    const float4 ViewPosition = mul(WorldPosition, View);
    return mul(ViewPosition, Projection);
}

float3 BuildFallbackWorldTangent(float3 WorldNormal)
{
    const float3 UpCandidate = abs(WorldNormal.z) < 0.999f ? float3(0.0f, 0.0f, 1.0f) : float3(0.0f, 1.0f, 0.0f);
    return normalize(cross(UpCandidate, WorldNormal));
}

float2 BuildWaterSampleUV(FWaterPSInput Input)
{
    const float2 MeshUV = Input.UV;
    const float2 WorldProjectedUV = Input.WorldPos.xy * float2(WorldUVScaleX, WorldUVScaleY);
    const float WorldBlend = saturate(WorldUVBlendFactor);
    return (WorldBlend >= 0.999f) ? WorldProjectedUV : lerp(MeshUV, WorldProjectedUV, WorldBlend);
}

float3 SampleWaterNormalA(float2 BaseUV)
{
    if (bHasNormalMapA == 0u)
    {
        return float3(0.0f, 0.0f, 1.0f);
    }

    const float2 LayerUV = BaseUV * NormalTilingA + Time * NormalScrollSpeedA;
    return WaterNormalA.Sample(SampleState, LayerUV).xyz * 2.0f - 1.0f;
}

float3 SampleWaterNormalB(float2 BaseUV)
{
    if (bHasNormalMapB == 0u)
    {
        return float3(0.0f, 0.0f, 1.0f);
    }

    const float2 LayerUV = BaseUV * NormalTilingB + Time * NormalScrollSpeedB;
    return WaterNormalB.Sample(SampleState, LayerUV).xyz * 2.0f - 1.0f;
}

float3 ResolveWaterWorldNormal(FWaterPSInput Input, float2 UV)
{
    const float3 GeometricNormal = normalize(Input.WorldNormal);

    const float3 N1 = SampleWaterNormalA(UV);
    const float3 N2 = SampleWaterNormalB(UV);

    float3 WaterNormalTS = normalize(N1 + N2);
    WaterNormalTS.xy *= NormalStrength;
    WaterNormalTS = normalize(WaterNormalTS);

    // Water-only stable TBN: avoid seam artifacts from imported tangent splits.
    const float3 WorldTangent = BuildFallbackWorldTangent(GeometricNormal);
    const float3 WorldBitangent = normalize(cross(GeometricNormal, WorldTangent));

    const float3x3 TBN = float3x3(WorldTangent, WorldBitangent, GeometricNormal);
    return normalize(mul(WaterNormalTS, TBN));
}

float ComputeWaterSpecular(float3 NormalWS, float3 ViewDirWS, float3 LightDirWS, float SpecularPower)
{
    const float3 HalfVec = normalize(ViewDirWS + LightDirWS);
    const float NdotH = saturate(dot(NormalWS, HalfVec));
    return pow(NdotH, max(SpecularPower, 1.0f));
}

float ComputeWaterDistanceAttenuation(float Distance, float Radius)
{
    float Attenuation = saturate(1.0f - (Distance / max(Radius, 1.0e-4f)));
    return Attenuation * Attenuation;
}

float ComputeDirectionalSpecularFade(float3 NormalWS, float3 LightDirWS)
{
    const float FadeWidth = max(NdotLFadeWidth, 1.0e-4f);
    const float NdotL = dot(NormalWS, LightDirWS);
    const float NormalFacingFade = smoothstep(-FadeWidth, FadeWidth, NdotL);

    float HorizonFade = 1.0f;
    const float HorizonRange = HorizonFadeEnd - HorizonFadeStart;
    if (abs(HorizonRange) > 1.0e-4f)
    {
        HorizonFade = smoothstep(HorizonFadeStart, HorizonFadeEnd, LightDirWS.z);
    }
    else
    {
        HorizonFade = (LightDirWS.z >= HorizonFadeEnd) ? 1.0f : 0.0f;
    }

    return NormalFacingFade * HorizonFade;
}

float3 ComputeDirectionalWaterSpecular(float3 NormalWS, float3 ViewDirWS)
{
    float3 SpecularAccum = 0.0f.xxx;
    [loop]
    for (uint LightIndex = 0u; LightIndex < SceneGlobalLightCount; ++LightIndex)
    {
        const FGPULight Light = GlobalLights[LightIndex];
        if (Light.Type != LIGHT_TYPE_DIRECTIONAL)
        {
            continue;
        }

        const float3 LightDirWS = normalize(Light.Direction);
        const float Spec = ComputeWaterSpecular(NormalWS, ViewDirWS, LightDirWS, WaterSpecularPower);
        const float DirectionalFade = ComputeDirectionalSpecularFade(NormalWS, LightDirWS);
        SpecularAccum += Light.Color * (Light.Intensity * Spec * DirectionalFade);
    }
    return SpecularAccum;
}

float3 ComputePointWaterSpecular(float3 WorldPos, float3 NormalWS, float3 ViewDirWS)
{
    float3 SpecularAccum = 0.0f.xxx;
    const uint LocalLightLimit = min(min(PointLightCount, WaterLocalLightCount), MAX_WATER_LOCAL_LIGHTS);

    [loop]
    for (uint LightIndex = 0u; LightIndex < LocalLightLimit; ++LightIndex)
    {
        const FVisibleLightData Light = PointLights[LightIndex];
        const float3 ToLight = Light.WorldPos - WorldPos;
        const float Distance = length(ToLight);
        if (Distance <= 1.0e-4f || Distance >= Light.Radius)
        {
            continue;
        }

        const float3 LightDirWS = ToLight / Distance;
        const float Attenuation = ComputeWaterDistanceAttenuation(Distance, Light.Radius);

        const float Spec = ComputeWaterSpecular(NormalWS, ViewDirWS, LightDirWS, WaterSpecularPower);
        SpecularAccum += Light.Color * (Light.Intensity * Spec * Attenuation);
    }
    return SpecularAccum;
}

float3 ComputeSpotWaterSpecular(float3 WorldPos, float3 NormalWS, float3 ViewDirWS)
{
    float3 SpecularAccum = 0.0f.xxx;
    const uint LocalLightLimit = min(min(SpotLightCount, WaterLocalLightCount), MAX_WATER_LOCAL_LIGHTS);

    [loop]
    for (uint LightIndex = 0u; LightIndex < LocalLightLimit; ++LightIndex)
    {
        const FVisibleLightData Light = SpotLights[LightIndex];
        const float3 ToLight = Light.WorldPos - WorldPos;
        const float Distance = length(ToLight);
        if (Distance <= 1.0e-4f || Distance >= Light.Radius)
        {
            continue;
        }

        const float3 LightDirWS = ToLight / Distance;
        float Attenuation = ComputeWaterDistanceAttenuation(Distance, Light.Radius);
        if (Attenuation <= 0.0f)
        {
            continue;
        }

        // Match the existing forward spot-light convention:
        // Light.Direction points outward from the spot source, so the receiver
        // test uses dot(SpotDir, -L) where L is surface -> light.
        const float3 SpotDir = normalize(Light.Direction);
        const float CosAngle = dot(SpotDir, -LightDirWS);
        const float ConeRange = max(Light.SpotInnerCos - Light.SpotOuterCos, 1.0e-4f);
        Attenuation *= saturate((CosAngle - Light.SpotOuterCos) / ConeRange);
        if (Attenuation <= 0.0f)
        {
            continue;
        }

        const float Spec = ComputeWaterSpecular(NormalWS, ViewDirWS, LightDirWS, WaterSpecularPower);
        SpecularAccum += Light.Color * (Light.Intensity * Spec * Attenuation);
    }

    return SpecularAccum;
}

FWaterPSInput mainVS(FWaterVSInput Input)
{
    FWaterPSInput Output;
    Output.WorldPos = mul(float4(Input.Position, 1.0f), Model).xyz;
    Output.WorldNormal = normalize(mul(Input.Normal, (float3x3)WorldInvTrans));
    Output.UV = Input.UV;
    Output.ClipPos = ApplyWaterMVP(Input.Position);
    return Output;
}

FWaterPSOutput mainPS(FWaterPSInput Input)
{
    FWaterPSOutput Output;

    const float2 WaterSampleUV = BuildWaterSampleUV(Input);
    const float2 DiffuseSampleUV = WaterSampleUV + Time * NormalScrollSpeedA;
    const float3 WaterWorldNormal = ResolveWaterWorldNormal(Input, WaterSampleUV);
    const float3 ViewDir = normalize(CameraPosition - Input.WorldPos);
    const float Fresnel = pow(1.0f - saturate(dot(WaterWorldNormal, ViewDir)), 3.0f);
    const float WaveTint = WaterWorldNormal.x * 0.5f + WaterWorldNormal.y * 0.5f;
    const float FallbackFlow = sin((WaterSampleUV.x + WaterSampleUV.y) * 6.0f + Time * 1.7f);
    const bool bHasAnyWaterNormal = (bHasNormalMapA != 0u) || (bHasNormalMapB != 0u);
    const float AnimatedSignal = bHasAnyWaterNormal ? WaveTint : (FallbackFlow * 0.5f);
    const float3 DiffuseTint = (bHasDiffuseMap != 0u) ? DiffuseMap.Sample(SampleState, DiffuseSampleUV).rgb : 1.0f.xxx;

    // Stage 1: subtle animated color response only. Specular/refraction/foam are reserved for later stages.
    float3 FinalColor = BaseColor * DiffuseTint;
    FinalColor *= (1.0f + AnimatedSignal * ColorVariationStrength);
    FinalColor += Fresnel * (0.08f * PrimitiveColor.rgb);

    // Stage 2: animated normal drives specular highlight shape/motion.
    if (bLightingEnabled > 0.5f && bEnableWaterSpecular != 0u)
    {
        float3 SpecularColor = ComputeDirectionalWaterSpecular(WaterWorldNormal, ViewDir);
        SpecularColor += ComputePointWaterSpecular(Input.WorldPos, WaterWorldNormal, ViewDir);
        SpecularColor += ComputeSpotWaterSpecular(Input.WorldPos, WaterWorldNormal, ViewDir);

        const float FresnelBoost = 1.0f + pow(1.0f - saturate(dot(WaterWorldNormal, ViewDir)), max(WaterFresnelPower, 1.0f))
            * WaterFresnelIntensity;
        FinalColor += SpecularColor * (WaterSpecularIntensity * WaterLightContributionScale * FresnelBoost);
    }

    FinalColor *= PrimitiveColor.rgb;

    if (bIsWireframe > 0.5f)
    {
        FinalColor = WireframeRGB;
    }

    Output.Color = float4(saturate(FinalColor), saturate(Alpha * PrimitiveColor.a));
    Output.Normal = float4(WaterWorldNormal * 0.5f + 0.5f, 1.0f);
    Output.WorldPos = float4(Input.WorldPos, 1.0f);
    return Output;
}
