// UI 전용 셰이더
// AddQuad에서 이미 NDC로 변환된 좌표가 들어옴

// 막 패스라 대충 바인딩
Texture2D    UITexture : register(t0);
SamplerState UISampler : register(s0);

struct VSInput
{
    float2 position : POSITION;  // NDC 좌표 
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
    output.position = float4(input.position, 0.0f, 1.0f);  
    output.texCoord = input.texCoord;
    output.color    = input.color;
    return output;
}

float4 PS(PSInput input) : SV_TARGET
{
    float4 texColor = UITexture.Sample(UISampler, input.texCoord);
    return texColor * input.color;  // 텍스처 색상 * tint 색상
}
