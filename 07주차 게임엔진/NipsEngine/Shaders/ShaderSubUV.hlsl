#include "Common.hlsl"

Texture2D SubUVAtlas : register(t0);
SamplerState SubUVSampler : register(s0);

struct VSInput
{
    float3 position : POSITION;
    float2 texCoord : TEXCOORD;
    float4 color    : COLOR;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD;
    float4 color    : COLOR;
};

PSInput VS(VSInput input)
{
    PSInput output;
    output.position = mul(mul(float4(input.position, 1.0f), View), Projection);
    output.texCoord = input.texCoord;
    output.color = input.color;
    return output;
}

float4 PS(PSInput input) : SV_TARGET
{
    float4 col = SubUVAtlas.Sample(SubUVSampler, input.texCoord);
    
    col.rgb *= input.color.rgb;
    // Apply vertex color (including distance fade alpha)
    col.a *= input.color.a;

    // Minimum alpha test to avoid rendering completely transparent pixels
    if (col.a < 0.01f)
    {
        discard;
    }

    if (bIsWireframe < 0.5f)
    {
        return col;
    }

    return lerp(col, float4(WireframeRGB, 1.0f), bIsWireframe);
}
