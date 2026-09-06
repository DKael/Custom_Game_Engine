#include "Common.hlsl"

Texture2D<float> DepthTexture : register(t0);
SamplerState PointSampler : register(s0);

cbuffer FireBallBuffer : register(b8)
{
    row_major float4x4 InverseClipToLocal;
    float Intensity;
    float Radius;
    float RadiusFallOff;
    float Padding;
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

    if (any(screenUV < 0.0f) || any(screenUV > 1.0f))
        clip(-1);
    
    float2 DepthUV = ViewportUVOffset + ViewportUVScale * screenUV; 
    float depthZ = DepthTexture.Sample(PointSampler, DepthUV).r;
   
    if (depthZ >= 1.0f) 
        clip(-1);
    
    float4 clipPos = float4(ndcXY, depthZ, 1.0f);
    float4 localPos = mul(clipPos, InverseClipToLocal);
    localPos /= localPos.w;
    
    float radius = max(Radius, 0.0001f);
    if (any(abs(localPos.xyz) > radius)) 
        clip(-1);

    float distanceToCenter = length(localPos.xyz);
    if (distanceToCenter >= radius)
        clip(-1);

    float falloff = lerp(0.01f, radius, RadiusFallOff);
    float innerRadius = max(radius - falloff, 0.0f);

    float normalizedDistance = saturate(distanceToCenter / radius);
    float decrease = distanceToCenter < RadiusFallOff ? 0.f : (normalizedDistance - RadiusFallOff) / max((1 - RadiusFallOff), 0.01f);

    float coreGlow = pow(1 - decrease, 2.0f);
    //float edgeFade = 1.0f - smoothstep(innerRadius, radius, distanceToCenter);
    //float density = distanceToCenter < RadiusFallOff ? coreGlow : coreGlow * edgeFade;

    float intensity = max(Intensity, 0.0f);
    float3 finalRGB = PrimitiveColor.rgb * coreGlow * intensity;
    //float finalAlpha = saturate(density * PrimitiveColor.a * saturate(intensity));

    //return float4(finalRGB, finalAlpha);
    return float4(finalRGB, 1.f);
}
