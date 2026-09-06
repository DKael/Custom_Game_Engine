cbuffer constants : register(b0)
{
    float3 Offset;
    float WipeProgress;
    float4 Scale;
    float FlashTimer;
    float3 _pad;
    float4 BlockColor;

}
struct VS_INPUT
{
    float4 position : POSITION;
    float4 color : COLOR;
    
};

struct PS_INPUT
{
    float4 position : SV_POSITION; // Transformed position to pass to the pixel shader
    float4 color : COLOR; // Color to pass to the pixel shader
    float localX : TEXCOORD0;
    float localY : TEXCOORD1;
};

PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT output;
    
    float3 scaledPos = input.position.xyz * Scale.xyz;
    float3 finalPos = scaledPos + Offset.xyz;
    
    output.position = float4(finalPos, input.position.w);
    output.localX = input.position.x;
    output.localY = input.position.y;
    // Pass the color to the pixel shader
    output.color = input.color;
    
    return output;
}

float4 mainPS(PS_INPUT input) : SV_TARGET
{
    float4 borderColor = float4(input.color.rgb, 1) * BlockColor;
    float4 fillColor = float4(input.color.rgb, 1) * BlockColor; 
    float4 baseColor = lerp(borderColor, fillColor, input.color.a);

    if (WipeProgress > -1.5f)
    {
        float diagonalPos = input.localX + input.localY;
        
        float sweep = saturate(1.0f - abs(diagonalPos - WipeProgress) / 0.17f);
        return lerp(baseColor, float4(1, 1, 1, 1), sweep);
    }
    
    if (FlashTimer > 0.f)
    {
        float d = length(float2(input.localX, input.localY)); 
        float t = saturate(FlashTimer);
        float glow = t * (1.0f / (d + 0.5f)) * 1.5f;
        baseColor = lerp(baseColor, float4(1, 1, 1, 1), saturate(glow));
    }
    return baseColor;
}