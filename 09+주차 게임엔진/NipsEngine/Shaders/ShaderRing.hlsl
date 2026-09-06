#include "Common.hlsl"

struct VS_INPUT
{
    float3 Pos : POSITION;
    float4 Color : COLOR;
    float2 LocalPosition : TEXCOORD0;
    float2 Radius : TEXCOORD1;
};

struct PS_INPUT
{
    float4 Pos : SV_POSITION;
    float4 Color : COLOR;
    float2 LocalPosition : TEXCOORD0;
    float2 Radius : TEXCOORD1;
};

PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT output;

    float4 worldPos = float4(input.Pos, 1.0f);
    float4 viewPos = mul(worldPos, View);
    output.Pos = mul(viewPos, Projection);
    output.Color = input.Color;
    output.LocalPosition = input.LocalPosition;
    output.Radius = input.Radius;

    return output;
}

float4 mainPS(PS_INPUT input) : SV_TARGET
{
    const float distanceFromCenter = length(input.LocalPosition);
    const float innerRadius = input.Radius.x;
    const float outerRadius = input.Radius.y;
    const float feather = max(fwidth(distanceFromCenter), 0.001f);

    const float innerMask = smoothstep(innerRadius - feather, innerRadius + feather, distanceFromCenter);
    const float outerMask = 1.0f - smoothstep(outerRadius - feather, outerRadius + feather, distanceFromCenter);
    const float alpha = input.Color.a * innerMask * outerMask;

    clip(alpha - 0.001f);
    return float4(input.Color.rgb, alpha);
}
