#include "Common.hlsl"

cbuffer StaticMeshBuffer : register(b6)
{
    float3 DiffuseColor; // 12 bytes
    uint   bHasDiffuseMap; // 4 bytes
};

cbuffer PerObjectPerformanceBuffer : register(b7)
{
    uint ComponentIndex; // 4 bytes
    float3 Padding;      // 12 bytes
};

struct InstanceData
{
    row_major float4x4 WorldMatrix;
};

Texture2D DiffuseMap : register(t0);
StructuredBuffer<InstanceData> WorldMatrices : register(t1);
SamplerState SampleState : register(s0);

struct VSInput
{
    float3 Position : POSITION;
    float2 UV : TEXCOORD;
};

struct PSInput
{
    float4 Position : SV_POSITION;
    float2 UV : TEXCOORD0;
};

PSInput mainVS(VSInput input)
{
    PSInput output;
    row_major matrix WorldMatrix = WorldMatrices[ComponentIndex].WorldMatrix;
    float4 WorldPos = mul(float4(input.Position, 1.0f), WorldMatrices[ComponentIndex].WorldMatrix);
    output.Position = mul(WorldPos, ViewProjection);
    output.UV = input.UV;
    return output;
}

float4 mainPS(PSInput input) : SV_TARGET
{
    float3 TexColor = DiffuseMap.Sample(SampleState, input.UV).xyz;
    float3 FinalColor = lerp(DiffuseColor, TexColor, (bool)bHasDiffuseMap);
  
    return float4(FinalColor, 1.0f);
}