#include "Common.hlsl"

cbuffer SceneDepthBuffer : register(b10)
{
    float2 ViewportUVOffset;
    float2 ViewportUVScale;
    float2 DepthTextureSize;
    float2 Pad;
}

Texture2D<float> DepthTexture : register(t0);

struct VS_OUTPUT
{
    float4 Pos : SV_POSITION;
    float2 UV : TEXCOORD0;
};

VS_OUTPUT VS(uint VertexID : SV_VertexID)
{
    VS_OUTPUT output;
    output.UV = float2((VertexID << 1) & 2, VertexID & 2);
    output.Pos = float4(output.UV * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
    
    return output;
}

float4 PS(VS_OUTPUT input) : SV_TARGET
{
    float2 DepthUV = ViewportUVOffset + ViewportUVScale * input.UV;
    int2 DepthPixel = int2(DepthUV * DepthTextureSize);
    DepthPixel = clamp(DepthPixel, int2(0, 0), int2(DepthTextureSize) - int2(1, 1));
    float depthZ = DepthTexture.Load(int3(DepthPixel, 0));
    
    if (depthZ >= 1.0f)
        return float4(0, 0, 0, 1.0f);
    
    if (Projection._44 == 1.0f)
    {
        return float4(depthZ, depthZ, depthZ, 1.0f);
    }
    
    float m32 = Projection._43;
    
    float m22 = Projection._13;
    
    float linearDepth = abs(m32 / (depthZ - m22));
    
    float MaxDistance = 300.0f;

    float finalDepthColor = saturate(linearDepth / MaxDistance);

    return float4(finalDepthColor, finalDepthColor, finalDepthColor, 1.0f);
}
