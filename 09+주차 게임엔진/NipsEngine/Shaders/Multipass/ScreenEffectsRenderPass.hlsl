// 카메라 효과
// Vignette. LetterBox, FadeInOut 같이 단순 SceneColorSRV만 사용하는 Pass 처리
// 분기 처리를 나중에 우버 셰이더로 바꾸면 좋음
Texture2D SceneColor : register(t0);
SamplerState SampleState : register(s0);

cbuffer ScreenEffectsConstant : register(b10)
{
    float FadeAmount;
    float3 FadeColor;

    float LetterBoxAmount;
    
    float bGammaCorrectionEnabled;
    float Gamma;
    
    float VignetteIntensity;
    float CurrentAspectRatio;
    float  VignetteRadius;
    float VignetteSoftness;
    float3 VignetteColor;
    float3 Padding;
}

struct VSOutput
{
    float4 ClipPos : SV_Position;
    float2 UV : TEXCOORD0;
};


VSOutput mainVS(uint VertexID : SV_VertexID)
{
    VSOutput Out;
    float2 Pos;
    
    if (VertexID == 0)
        Pos = float2(-1.0f, -1.0f);
    else if (VertexID == 1)
        Pos = float2(-1.0f, 3.0f);
    else
        Pos = float2(3.0f, -1.0f);

    Out.ClipPos = float4(Pos, 0.0f, 1.0f);
    Out.UV = float2(Pos.x * 0.5f + 0.5f, -Pos.y * 0.5f + 0.5f);
    return Out;
}

float4 mainPS(VSOutput PSInput) : SV_TARGET
{
    float2 UV = PSInput.UV;
    float3 Color = SceneColor.Sample(SampleState, UV).rgb;
   
    
    // Vignette
    if (VignetteIntensity > 0.f)
    {
        float2 NDC = UV * 2.0f - 1.0f;
        
        // 투영행렬로 인해 찌그러진 거 복원
        NDC.x *= CurrentAspectRatio;

        float Dist = length(NDC);
        float Mask = 1.0f - smoothstep(VignetteRadius - VignetteSoftness, VignetteRadius, Dist);
        float EdgeAmount = (1.0f - Mask) * VignetteIntensity;

        Color = lerp(Color, VignetteColor, EdgeAmount);
    }

    // LetterBox
    if (LetterBoxAmount > 0.0f)
    {
        bool bInLetterBox = UV.y <= LetterBoxAmount || UV.y >= (1.0f - LetterBoxAmount);
        if (bInLetterBox)
        {
            Color = float3(0.f, 0.f, 0.f);
        }   
    }
    
    if (FadeAmount > 0.0f)
    {
        Color = lerp(Color, FadeColor, FadeAmount);
    }
    
    if (bGammaCorrectionEnabled != 0 && Gamma > 0.0f)
    {
        Color = pow(saturate(Color), 1.0f / Gamma);
    }
    return float4(Color, 1.f);
}
