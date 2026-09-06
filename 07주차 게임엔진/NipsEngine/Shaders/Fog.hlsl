#include "common.hlsl"

cbuffer SceneDepthBuffer : register(b10)
{
    float2 ViewportUVOffset;
    float2 ViewportUVScale;
    float2 DepthTextureSize;
    float2 Pad;
}

cbuffer FogBuffer : register(b9)
{
    float4 FogInscatteringColor;

    float FogDensity;
    float FogHeightFalloff;
    float StartDistance;
    float FogCutoffDistance;

    float FogMaxOpacity;
    float FogHeight;
    float Pad0;
    float Pad1;

    uint  bEnabled;
    float3 Pad3;
};

Texture2D<float> DepthTexture : register(t0);

struct VS_OUTPUT
{
    float4 Pos : SV_POSITION; 
    float2 UV : TEXCOORD0;
};


VS_OUTPUT VS(uint VertexID : SV_VertexID)
{
    // 점 3개의 삼각형 내에서 쿼드를 뽑습니다.
    VS_OUTPUT output;
    float2 Position[3] =
    {
        float2(-1.f, 1.f),
        float2( 3.f, 1.f),
        float2(-1.f, -3.f)
    };
    
    float2 UV[3] =
    {
        float2(0.f, 0.f),
        float2(2.f, 0.f),
        float2(0.f, 2.f)
    };
    output.Pos = float4(Position[VertexID], 0.f, 1.f);
    output.UV  = UV[VertexID];
    
    return output;
}



float3 ClipToWorld(float ndcX, float ndcY, float depthZ)
{
    float4 homogeneous = mul(float4(ndcX, ndcY, depthZ, 1.0f), InverseViewProjection);
    return homogeneous.xyz / homogeneous.w;
}

// 높이 기반 포그의 밀도 적분값 계산
// 수평에 가까운 레이는 상수 밀도로, 그 외에는 지수 감쇠 적분으로 처리
float ComputeHeightFogIntegral(float3 rayDir, float startDist, float endDist)
{
    static const float Epsilon  = 1e-6f;
    static const float Boundary = 100.0f;

    float heightFalloff = max(FogHeightFalloff, Epsilon);
    float slope         = heightFalloff * rayDir.z;
    float hStart        = CameraWorldPos.z + rayDir.z * startDist - FogHeight;
    float hEnd          = CameraWorldPos.z + rayDir.z * endDist   - FogHeight;

    if (abs(slope) < Epsilon)
    {
        // 수평 레이 
        float density = FogDensity * exp(clamp(-heightFalloff * hStart, -Boundary, Boundary));
        return density * (endDist - startDist);
    }
    else
    {
        // 경사진 레이
        float eStart = exp(clamp(-heightFalloff * hStart, -Boundary, Boundary));
        float eEnd   = exp(clamp(-heightFalloff * hEnd,   -Boundary, Boundary));
        return FogDensity * (eStart - eEnd) / slope;
    }
}

float4 PS(VS_OUTPUT input) : SV_TARGET
{
    static const float Epsilon      = 1e-6f;
    static const float FallbackDist = 1e6f;

    float2 depthUV    = ViewportUVOffset + ViewportUVScale * input.UV;
    int2   depthPixel = clamp(int2(depthUV * DepthTextureSize),
                              int2(0, 0), int2(DepthTextureSize) - int2(1, 1));
    
    float ndcX = input.UV.x * 2.0f - 1.0f;
    float ndcY = 1.0f - input.UV.y * 2.0f;
    float  depthZ     = DepthTexture.Load(int3(depthPixel, 0));
    
    float3 worldPos = ClipToWorld(ndcX, ndcY, depthZ);
    float3 rayVec   = worldPos - CameraWorldPos;
    float  rayLen   = length(rayVec);

    if (rayLen < Epsilon)
        return float4(FogInscatteringColor.rgb, 0.0f);

    
    float3 rayDir;
    if (depthZ >= 1.0f)
    {
        // 스카이박스: 방향만 사용, 거리는 CutoffDistance로 고정
        float3 worldFar = ClipToWorld(ndcX, ndcY, 1.0f);
        rayDir = normalize(worldFar - CameraWorldPos);
        rayLen = (FogCutoffDistance > 0.0f) ? FogCutoffDistance : FallbackDist;
    }
    else
    {
        rayDir = rayVec / rayLen;
        if (rayLen < StartDistance)
            return float4(FogInscatteringColor.rgb, 0.0f);
    }

    float startDist = StartDistance;
    // CutoffDistance는 포그가 렌더링될 최대 거리이므로, 오브젝트가 그보다 가까우면 실제 거리를 사용
    float endDist   = (FogCutoffDistance > 0.0f) ? min(FogCutoffDistance, rayLen) : rayLen;

    if (endDist <= startDist)
        return float4(FogInscatteringColor.rgb, 0.0f);
    
    
    float fogIntegral   = ComputeHeightFogIntegral(rayDir, startDist, endDist);
    float transmittance = saturate(1.0f - exp(-max(fogIntegral, 0.0f)));
    transmittance       = saturate(transmittance * FogMaxOpacity);

    return float4(FogInscatteringColor.rgb, transmittance);
}