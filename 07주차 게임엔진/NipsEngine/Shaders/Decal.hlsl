#include "Common.hlsl"

Texture2D<float> DepthTexture : register(t0);
Texture2D<float4> DecalTexture : register(t1);

SamplerState PointSampler : register(s0);

cbuffer DecalBuffer : register(b7)
{
    row_major float4x4 InverseClipToLocal;
    float FadeAlpha;
    float3 DecalAmbientColor;
    
    float3 DecalDiffuseColor;
    float bHasDecalDiffuseMap;
    
    float3 DecalSpecularColor;
    uint bHasDecalNormalMap;
}

cbuffer SceneDepthBuffer : register(b10)
{
    float2 ViewportUVOffset;
    float2 ViewportUVScale;
    float2 DepthTextureSize;
    float2 Pad;
}

struct VS_INPUT
{
    float3 Pos : POSITION;
};

struct VS_OUTPUT
{
    float4 Pos : SV_POSITION;
    float4 ScreenPos : TEXCOORD0;
};

VS_OUTPUT VS(VS_INPUT input)
{
    VS_OUTPUT output;
    
    float4 worldPos = mul(float4(input.Pos, 1.0f), Model);
    float4 viewPos = mul(worldPos, View);
    output.Pos = mul(viewPos, Projection);

    output.ScreenPos = output.Pos;

    return output;
}

float4 PS(VS_OUTPUT input) : SV_TARGET
{
    float2 ndcXY = input.ScreenPos.xy / input.ScreenPos.w;
    
    float2 screenUV = ndcXY * float2(0.5f, -0.5f) + 0.5f;
    float2 DepthUV = ViewportUVOffset + ViewportUVScale * screenUV;
    
    float depthZ = DepthTexture.Sample(PointSampler, DepthUV).r;
    
    if (depthZ >= 1.0f) 
        clip(-1);
    
    float4 clipPos = float4(ndcXY, depthZ, 1.0f);
    float4 localPos = mul(clipPos, InverseClipToLocal);
    localPos /= localPos.w;
    
    if (any(abs(localPos.xyz) > 0.501f)) 
        clip(-1);

    
    float2 decalUV = float2(localPos.y, -localPos.z) + 0.5f;
    float4 finalColor = DecalTexture.Sample(PointSampler, decalUV);
    finalColor.a *= FadeAlpha;
    
    if (finalColor.a < 0.05f) 
        clip(-1);

    return finalColor;
}