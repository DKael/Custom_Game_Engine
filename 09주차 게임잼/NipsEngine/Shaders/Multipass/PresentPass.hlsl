#include "../Common.hlsl"

// scene composition과 swap chain output을 분리하는 최종 출력 pass.
Texture2D SceneFinalColor : register(t0);
SamplerState SampleState : register(s0);

struct VSOutput
{
    float4 ClipPos : SV_POSITION;
    float2 UV : TEXCOORD0;
};

VSOutput mainVS(uint VertexID : SV_VertexID)
{
    VSOutput Output;

    if (VertexID == 0)
    {
        Output.ClipPos = float4(-1.0f, -1.0f, 0.0f, 1.0f);
        Output.UV = float2(0.0f, 1.0f);
    }
    else if (VertexID == 1)
    {
        Output.ClipPos = float4(-1.0f, 3.0f, 0.0f, 1.0f);
        Output.UV = float2(0.0f, -1.0f);
    }
    else
    {
        Output.ClipPos = float4(3.0f, -1.0f, 0.0f, 1.0f);
        Output.UV = float2(2.0f, 1.0f);
    }

    return Output;
}

float4 mainPS(VSOutput Input) : SV_TARGET
{
    // 최종 scene result를 그대로 출력한다.
    return SceneFinalColor.Sample(SampleState, Input.UV);
}
