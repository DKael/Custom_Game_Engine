// UI 전용 2D 폰트 셰이더 — FUIVertex 레이아웃과 동일 (float2 XY | float2 UV | float4 Color)
// 행렬 변환 없음, Color tint 지원

Texture2D    FontAtlas   : register(t0);
SamplerState FontSampler : register(s0);

struct VSInput
{
    float2 position : POSITION;   // NDC 좌표 (-1~+1)
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
    output.position = float4(input.position, 0.f, 1.f);
    output.texCoord = input.texCoord;
    output.color    = input.color;
    return output;
}

float4 PS(PSInput input) : SV_TARGET
{
    float4 col = FontAtlas.Sample(FontSampler, input.texCoord);
    if (col.r < 0.1f)
        discard;
    // 아틀라스는 흑백 — 알파를 R 채널로, 색상은 input.color로 적용
    return float4(input.color.rgb, col.r * input.color.a);
}
