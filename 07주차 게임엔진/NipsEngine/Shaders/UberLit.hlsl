#include "Common.hlsl"

#define UBERLIT_DEBUG_SPEC_MODE 0
// 0: off, 1: NdotL, 2: NdotH, 3: spec

#define LIT 0
#define UNLIT 1
#define LIT_GOURAUD 5
#define LIT_LAMBERT 6
#define LIT_PHONG 7
#define WORLD_NORMAL 8

// USE_NORMALMAP: 0 or 1
#define TRUE 1
#define FALSE 0

// OPAQUE TYPE
#define STATICMESH 0
#define DECAL 1

#ifndef VIEW_MODE
    #define VIEW_MODE 7  // 기본값 설정
#endif

#if OPAQUETYPE == DECAL
Texture2D g_NormalTexture : register(t11);
Texture2D g_DepthTexture : register(t13);
#endif

struct PSOutput
{
    float4 Color : SV_Target0;
    
#if OPAQUETYPE == STATICMESH
    float4 Normal : SV_Target1;
#endif
};

#ifndef FORWARD_PLUS_TILE_SIZE_X
    #define FORWARD_PLUS_TILE_SIZE_X 16
#endif

#ifndef FORWARD_PLUS_TILE_SIZE_Y
    #define FORWARD_PLUS_TILE_SIZE_Y 16
#endif

// StaticMesh Material (b6)
cbuffer StaticMeshBuffer : register(b6)
{
    float3 AmbientColor; // Ka
    float Padding6_1;
    
    float3 DiffuseColor; // Kd
    float Padding6_2;
    
    float3 SpecularColor; // Ks
    float Shininess; // Ns
    
    // ScrollUV
    float2 ScrollUV;
    uint bHasDiffuseMap;
    uint bHasSpecularMap;
    
    uint bHasNormalMap;
    float Padding6_3;
    float Padding6_4;
    float Padding6_5;
};

cbuffer DecalBuffer : register(b7)
{
    row_major float4x4 InverseClipToLocal;
    float FadeAlpha;
    float3 DecalAmbientColor;
    
    float3 DecalDiffuseColor;
    uint bHasDecalDiffuseMap;
    
    float3 DecalSpecularColor;
    uint bHasDecalSpecularMap;
    
    uint bHasDecalNormalMap;
    float3 Padding7;
}

cbuffer SceneDepthBuffer : register(b10)
{
    float2 ViewportUVOffset;
    float2 ViewportUVScale;
    float2 SceneDepthTextureSize;
    float2 Pad;
}

struct VSInput
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float2 UV : TEXCOORD;
    float3 Tangent : TANGENT;
};

struct PSInput
{
    float4 ClipPos : SV_POSITION;
    float3 WorldPos : TEXCOORD0;
    float3 WorldNormal : TEXCOORD1;
    float3 WorldTangent : TEXCOORD2;
    float2 UV : TEXCOORD3;
    float3 VertexDiffuseLighting : TEXCOORD4;
    float3 VertexSpecularLighting : TEXCOORD5;
    
    float4 ScreenPos : TEXCOORD6;
    
};

// Lighting (b13)
struct FAmbientLightInfo
{
    float3 Color;
    float Intensity;
};

struct FDirectionalLightInfo
{
    float3 Direction;
    float Intensity;

    float3 Color;
    float padding;
};

struct FPointLightInfo
{
    float3 Position;
    float Radius;
    float3 Color;
    float Intensity;
};

struct FPointLightCommon
{
    float3 LightDir;
    float Distance;
    float Attenuation;
    float NdotL;
    bool bValid;
};

struct FSpotLightInfo
{
    float3 Position;
    float Radius;

    float3 Color;
    float Intensity;
    
    float3 Direction;
    float InnerConeCos;
    
    float OuterConeCos;
    float3 Padding;
};

cbuffer Lighting : register(b13)
{
    FAmbientLightInfo Ambient;
    uint DirectionalLightCount;
    uint PointLightCount;
    uint SpotLightCount;
    float LightingPad;
};

cbuffer ForwardPlusConstants : register(b11)
{
    uint2 ViewportMin;
    uint2 ViewportSize;
    uint2 DepthTextureSize;
    uint2 TileCount;
    uint bEnable25DMask;
    float3 ForwardPlusPadding;
};

// Light Data (t0-t5)
StructuredBuffer<FPointLightInfo> PointLights : register(t0);
StructuredBuffer<FSpotLightInfo> SpotLights : register(t1);
StructuredBuffer<uint> TilePointLightIndices : register(t2);
StructuredBuffer<uint> TileSpotLightIndices : register(t3);
StructuredBuffer<uint2> TilePointLightGrid : register(t4);
StructuredBuffer<uint2> TileSpotLightGrid : register(t5);

// Directional Lights (t10)

StructuredBuffer<FDirectionalLightInfo> DirectionalLights : register(t10);

// StaticMesh/Decal Textures (t6-t9)
Texture2D DiffuseMap : register(t6);
Texture2D AmbientMap : register(t7);
Texture2D SpecularMap : register(t8);
Texture2D BumpMap : register(t9);

SamplerState SampleState : register(s0);

float3 GetDiffuseTexPS(float2 uv)
{
    if ((bool) bHasDiffuseMap)
    {
        return DiffuseMap.Sample(SampleState, uv).rgb;
    }
    return DiffuseColor;
}

float3 GetSpecularTexPS(float2 uv)
{
    if ((bool) bHasSpecularMap)
    {
        return SpecularMap.Sample(SampleState, uv).rgb;
    }
    return SpecularColor;
}

float3 CalculateAmbientLight(FAmbientLightInfo Light, float3 MaterialAmbientColor, float3 DiffuseTex)
{
    return Light.Color * Light.Intensity * MaterialAmbientColor * DiffuseTex;
}

FPointLightCommon EvaluatePointLightCommon(
    FPointLightInfo Light,
    float3 WorldPos,
    float3 N)
{
    FPointLightCommon Result;
    Result.bValid = false;
    Result.LightDir = 0.0f.xxx;
    Result.Distance = 0.0f;
    Result.Attenuation = 0.0f;
    Result.NdotL = 0.0f;

    float3 L = Light.Position - WorldPos;
    float distSq = dot(L, L);
    float radiusSq = Light.Radius * Light.Radius;

    if (distSq > radiusSq)
        return Result;

    float dist = sqrt(distSq);
    float3 lightDir = L / max(dist, 1e-5f);

    N = normalize(N);
    float NdotL = saturate(dot(N, lightDir));
    if (NdotL <= 0.0f)
        return Result;

    float attenuation = saturate(1.0f - dist / Light.Radius);
    attenuation *= attenuation;

    Result.bValid = true;
    Result.LightDir = lightDir;
    Result.Distance = dist;
    Result.Attenuation = attenuation;
    Result.NdotL = NdotL;
    return Result;
}

uint GetTileIndexFromScreenPos(float2 screenPos)
{
    uint tileCountX = max(TileCount.x, 1u);
    uint tileCountY = max(TileCount.y, 1u);

    float2 localPixel = max(screenPos - float2(ViewportMin), 0.0f.xx);
    uint2 tileCoord = uint2(localPixel / float2(FORWARD_PLUS_TILE_SIZE_X, FORWARD_PLUS_TILE_SIZE_Y));
    tileCoord.x = min(tileCoord.x, tileCountX - 1u);
    tileCoord.y = min(tileCoord.y, tileCountY - 1u);

    return tileCoord.y * tileCountX + tileCoord.x;
}

void CalculateLightingLambertTile(float3 WorldPos, float3 N, uint TileIndex, out float3 OutDiffuse, float3 InAmbientColor)
{
    OutDiffuse = CalculateAmbientLight(Ambient, InAmbientColor, 1.0f.xxx);

    for (uint i = 0; i < DirectionalLightCount; ++i)
    {
        float3 L = normalize(-DirectionalLights[i].Direction);
        float NdotL = saturate(dot(N, L));
        OutDiffuse += DirectionalLights[i].Color * DirectionalLights[i].Intensity * NdotL;
    }

    uint2 pointGrid = TilePointLightGrid[TileIndex];
    for (uint i = 0; i < pointGrid.y; ++i)
    {
        uint lightIndex = TilePointLightIndices[pointGrid.x + i];
        FPointLightCommon Common = EvaluatePointLightCommon(PointLights[lightIndex], WorldPos, N);
        if (Common.bValid)
        {
            OutDiffuse += PointLights[lightIndex].Color *
                          PointLights[lightIndex].Intensity *
                          Common.NdotL *
                          Common.Attenuation;
        }
    }

    uint2 spotGrid = TileSpotLightGrid[TileIndex];
    for (uint i = 0; i < spotGrid.y; ++i)
    {
        uint lightIndex = TileSpotLightIndices[spotGrid.x + i];
        float3 Lvec = SpotLights[lightIndex].Position - WorldPos;
        float dist = length(Lvec);

        if (dist > SpotLights[lightIndex].Radius)
            continue;

        float3 L = Lvec / max(dist, 1e-5f);
        float NdotL = saturate(dot(N, L));
        if (NdotL <= 0.0f)
            continue;

        float3 lightDir = normalize(-SpotLights[lightIndex].Direction);
        float spotCos = dot(L, lightDir);
        float spotFactor = smoothstep(SpotLights[lightIndex].OuterConeCos, SpotLights[lightIndex].InnerConeCos, spotCos);
        spotFactor *= spotFactor;

        float attenuation = 1.0f - (dist / SpotLights[lightIndex].Radius);
        attenuation *= attenuation;

        OutDiffuse += SpotLights[lightIndex].Color *
                      SpotLights[lightIndex].Intensity *
                      NdotL *
                      attenuation *
                      spotFactor;
    }
}

void CalculateLightingBlinnPhongTile(
    float3 WorldPos,
    float3 N,
    uint TileIndex,
    out float3 OutDiffuse,
    out float3 OutSpecular,
    float3 InAmbientColor,
    float InShininess)
{
    CalculateLightingLambertTile(WorldPos, N, TileIndex, OutDiffuse, InAmbientColor);
    OutSpecular = 0.0f.xxx;

    for (uint i = 0; i < DirectionalLightCount; ++i)
    {
        float3 L = normalize(-DirectionalLights[i].Direction);
        float NdotL = saturate(dot(N, L));
        if (NdotL <= 0.0f)
            continue;

        float3 V = normalize(CameraWorldPos - WorldPos);
        float3 H = normalize(L + V);
        float NdotH = saturate(dot(N, H));

        OutSpecular += DirectionalLights[i].Color *
                       DirectionalLights[i].Intensity *
                       pow(NdotH, max(InShininess, 1.0f));
    }

    uint2 pointGrid = TilePointLightGrid[TileIndex];
    for (uint i = 0; i < pointGrid.y; ++i)
    {
        uint lightIndex = TilePointLightIndices[pointGrid.x + i];
        FPointLightCommon Common = EvaluatePointLightCommon(PointLights[lightIndex], WorldPos, N);
        if (!Common.bValid)
            continue;

        float3 V = normalize(CameraWorldPos - WorldPos);
        float3 H = normalize(Common.LightDir + V);
        float NdotH = saturate(dot(normalize(N), H));

        OutSpecular += PointLights[lightIndex].Color *
                       PointLights[lightIndex].Intensity *
                       pow(NdotH, max(InShininess, 1.0f)) *
                       Common.Attenuation;
    }

    uint2 spotGrid = TileSpotLightGrid[TileIndex];
    for (uint i = 0; i < spotGrid.y; ++i)
    {
        uint lightIndex = TileSpotLightIndices[spotGrid.x + i];
        float3 Lvec = SpotLights[lightIndex].Position - WorldPos;
        float dist = length(Lvec);

        if (dist > SpotLights[lightIndex].Radius)
            continue;

        float3 L = Lvec / max(dist, 1e-5f);
        float NdotL = saturate(dot(N, L));
        if (NdotL <= 0.0f)
            continue;

        float3 lightDir = normalize(-SpotLights[lightIndex].Direction);
        float spotCos = dot(L, lightDir);
        float spotFactor = smoothstep(SpotLights[lightIndex].OuterConeCos, SpotLights[lightIndex].InnerConeCos, spotCos);
        spotFactor *= spotFactor;

        float attenuation = 1.0f - (dist / SpotLights[lightIndex].Radius);
        attenuation *= attenuation;

        float3 V = normalize(CameraWorldPos - WorldPos);
        float3 H = normalize(L + V);
        float NdotH = saturate(dot(N, H));

        OutSpecular += SpotLights[lightIndex].Color *
                       SpotLights[lightIndex].Intensity *
                       pow(NdotH, max(InShininess, 1.0f)) *
                       attenuation *
                       spotFactor;
    }
}

void CalculateLightingLambertMaterial(float3 WorldPos, float3 N, out float3 OutDiffuse, float3 InAmbientColor)
{
    OutDiffuse = CalculateAmbientLight(Ambient, InAmbientColor, 1.0f.xxx);

    for (uint i = 0; i < DirectionalLightCount; ++i)
    {
        float3 L = normalize(-DirectionalLights[i].Direction);
        float NdotL = saturate(dot(N, L));
        OutDiffuse += DirectionalLights[i].Color * DirectionalLights[i].Intensity * NdotL;
    }

    for (uint j = 0; j < PointLightCount; ++j)
    {
        FPointLightCommon Common = EvaluatePointLightCommon(PointLights[j], WorldPos, N);
        if (Common.bValid)
        {
            OutDiffuse += PointLights[j].Color * PointLights[j].Intensity * Common.NdotL * Common.Attenuation;
        }
    }

    for (uint k = 0; k < SpotLightCount; ++k)
    {
        float3 Lvec = SpotLights[k].Position - WorldPos;
        float dist = length(Lvec);

        if (dist > SpotLights[k].Radius)
            continue;

        float3 L = Lvec / max(dist, 1e-5f);
        float NdotL = saturate(dot(N, L));
        if (NdotL <= 0.0f)
            continue;

        float3 lightDir = normalize(-SpotLights[k].Direction);
        float spotCos = dot(L, lightDir);
        float spotFactor = smoothstep(SpotLights[k].OuterConeCos, SpotLights[k].InnerConeCos, spotCos);
        spotFactor *= spotFactor;

        float attenuation = 1.0f - (dist / SpotLights[k].Radius);
        attenuation *= attenuation;

        OutDiffuse += SpotLights[k].Color * SpotLights[k].Intensity * NdotL * attenuation * spotFactor;
    }
}

void CalculateLightingLambert(float3 WorldPos, float3 N, out float3 OutDiffuse)
{
    CalculateLightingLambertMaterial(WorldPos, N, OutDiffuse, AmbientColor);
}

void CalculateLightingBlinnPhong(
    float3 WorldPos,
    float3 N,
    out float3 OutDiffuse,
    out float3 OutSpecular,
    float3 InAmbientColor,
    float InShininess)
{
    CalculateLightingLambertMaterial(WorldPos, N, OutDiffuse, InAmbientColor);
    OutSpecular = 0.0f.xxx;

    for (uint i = 0; i < DirectionalLightCount; ++i)
    {
        float3 L = normalize(-DirectionalLights[i].Direction);
        float NdotL = saturate(dot(N, L));
        if (NdotL <= 0.0f)
            continue;

        float3 V = normalize(CameraWorldPos - WorldPos);
        float3 H = normalize(L + V);
        float NdotH = saturate(dot(N, H));

        OutSpecular += DirectionalLights[i].Color *
                       DirectionalLights[i].Intensity *
                       pow(NdotH, max(InShininess, 1.0f));
    }

    for (uint j = 0; j < PointLightCount; ++j)
    {
        FPointLightCommon Common = EvaluatePointLightCommon(PointLights[j], WorldPos, N);
        if (!Common.bValid)
            continue;

        float3 V = normalize(CameraWorldPos - WorldPos);
        float3 H = normalize(Common.LightDir + V);
        float NdotH = saturate(dot(normalize(N), H));

        OutSpecular += PointLights[j].Color *
                       PointLights[j].Intensity *
                       pow(NdotH, max(InShininess, 1.0f)) *
                       Common.Attenuation;
    }

    for (uint k = 0; k < SpotLightCount; ++k)
    {
        float3 Lvec = SpotLights[k].Position - WorldPos;
        float dist = length(Lvec);

        if (dist > SpotLights[k].Radius)
            continue;

        float3 L = Lvec / max(dist, 1e-5f);
        float NdotL = saturate(dot(N, L));
        if (NdotL <= 0.0f)
            continue;

        float3 lightDir = normalize(-SpotLights[k].Direction);
        float spotCos = dot(L, lightDir);
        float spotFactor = smoothstep(SpotLights[k].OuterConeCos, SpotLights[k].InnerConeCos, spotCos);
        spotFactor *= spotFactor;

        float attenuation = 1.0f - (dist / SpotLights[k].Radius);
        attenuation *= attenuation;

        float3 V = normalize(CameraWorldPos - WorldPos);
        float3 H = normalize(L + V);
        float NdotH = saturate(dot(N, H));

        OutSpecular += SpotLights[k].Color *
                       SpotLights[k].Intensity *
                       pow(NdotH, max(InShininess, 1.0f)) *
                       attenuation *
                       spotFactor;
    }
}

void CalculateLightingBlinnPhong(
    float3 WorldPos,
    float3 N,
    out float3 OutDiffuse,
    out float3 OutSpecular)
{
    CalculateLightingBlinnPhong(WorldPos, N, OutDiffuse, OutSpecular, AmbientColor, Shininess);
}

PSInput VS(VSInput input)
{
    PSInput output;

    float4 worldPos = mul(float4(input.Position, 1.0f), Model);
    output.WorldPos = worldPos.xyz;
    output.ClipPos = mul(mul(worldPos, View), Projection);
    output.ScreenPos = output.ClipPos;
    
    float3x3 normalMatrix = transpose(InvModel);
    output.WorldNormal = normalize(mul(input.Normal, normalMatrix));
    output.WorldTangent = normalize(mul(input.Tangent, normalMatrix));
    
    output.UV = input.UV + ScrollUV;

    
    // Gouraud Lighting
    {
        float3 N = normalize(output.WorldNormal);

        CalculateLightingBlinnPhong(
            output.WorldPos,
            N,
            output.VertexDiffuseLighting,
            output.VertexSpecularLighting);

    }

    return output;
}

PSOutput PS(PSInput input)
{
    PSOutput Output;
    float4 finalColor = { 0.f, 0.f, 0.f, 1.0f };
    float3 N = normalize(input.WorldNormal);
    uint TileIndex = GetTileIndexFromScreenPos(input.ClipPos.xy);
    
    float3 DiffuseTex = { 1.0f, 1.0f, 1.0f };
    float3 SpecularTex = { 1.0f, 1.0f, 1.0f };
    
    float3 DiffuseLighting;
    
    //----------------------------------------------------------------------------------------------------------------------------------------
    //                                      Decal PixelShader Logic
    //----------------------------------------------------------------------------------------------------------------------------------------
    #if OPAQUETYPE == DECAL
        float2 ndcXY = input.ScreenPos.xy / input.ScreenPos.w;
        float2 screenUV = ndcXY * float2(0.5f, -0.5f) + 0.5f;
        float2 DepthUV = ViewportUVOffset + ViewportUVScale * screenUV;

        float depthZ = g_DepthTexture.Sample(SampleState, DepthUV).r;
        if (depthZ >= 1.0f) 
            discard;
        
        float4 clipPos = float4(ndcXY, depthZ, 1.0f);
        float4 localPos = mul(clipPos, InverseClipToLocal);
        localPos /= localPos.w;
        
        if (any(abs(localPos.xyz) > 0.501f)) 
            clip(-1);
        
        float2 decalUV = float2(localPos.y, -localPos.z) + 0.5f;
        
        float4 sampledDiffuse = DiffuseMap.Sample(SampleState, decalUV);
        SpecularTex = SpecularMap.Sample(SampleState, decalUV).rgb;
        
        DiffuseTex = sampledDiffuse.rgb;
        float alpha = sampledDiffuse.a * FadeAlpha;
    
        if (alpha < 0.05f) 
            clip(-1);
    
        //Normal Sampling
        float3 sceneNormal = g_NormalTexture.Sample(SampleState, DepthUV).rgb;
        float3 finalNormal = normalize(sceneNormal);
        
        //월드 좌표 복원
        float4 worldPosDecal = mul(clipPos, InverseViewProjection);
        worldPosDecal /= worldPosDecal.w;
        
    
        //Output.Color.xyz = finalNormal;
        //Output.Color.w = 1.0f;
        //return Output;
        
        #if USE_NORMALMAP == TRUE
            float3 decalNormalTex = BumpMap.Sample(SampleState, decalUV).rgb * 2.0f - 1.0f;
            float3 baseN = normalize(sceneNormal);
    
            float3 up = abs(baseN.z) < 0.999f ? float3(0,0,1) : float3(0,1,0);
            float3 T = normalize(cross(up, baseN));
            float3 B = cross(baseN, T);
    
            finalNormal = normalize(
            decalNormalTex.x * T +
            decalNormalTex.y * B +
            decalNormalTex.z * baseN
            );
        #endif

        //조명 계산
        float3 dDiffuse = 0;
        float3 dSpecular = 0;
        
        #if VIEW_MODE == UNLIT
            finalColor.xyz = DiffuseTex;
        
        #elif (VIEW_MODE == LIT_LAMBERT) || (VIEW_MODE == LIT_GOURAUD)
            #if LIGHT_CULLING_FLAG
                CalculateLightingLambertTile(worldPosDecal.xyz, finalNormal, TileIndex, DiffuseLighting, DecalAmbientColor);
            #else
                CalculateLightingLambertMaterial(worldPosDecal.xyz, finalNormal, DiffuseLighting, DecalAmbientColor);
            #endif
            finalColor.xyz = DiffuseTex * DiffuseLighting;
        #elif VIEW_MODE == LIT_PHONG
            #if LIGHT_CULLING_FLAG
                CalculateLightingBlinnPhongTile(
                    worldPosDecal.xyz,
                    finalNormal,
                    TileIndex,
                    dDiffuse,
                    dSpecular,
                    DecalAmbientColor,
                    50.0f);
            #else
                CalculateLightingBlinnPhong(
                    worldPosDecal.xyz,
                    finalNormal,
                    dDiffuse,
                    dSpecular,
                    DecalAmbientColor,
                    50.0f);
            #endif
            finalColor.xyz = (DiffuseTex * dDiffuse) + (SpecularTex * dSpecular);
        #elif VIEW_MODE == WORLD_NORMAL
            finalColor = float4(finalNormal * 0.5f + 0.5f, 1.0f);

        #endif

    finalColor.a = alpha;
    if (finalColor.a < 0.05f)
        discard;
    Output.Color = finalColor;
    return Output;
    //----------------------------------------------------------------------------------------------------------------------------------------
    //                                      // StaticMesh PixelShader Logic
    //----------------------------------------------------------------------------------------------------------------------------------------
    #elif OPAQUETYPE == STATICMESH 
        DiffuseTex = DiffuseColor;
    
        #if USE_NORMALMAP == TRUE
            float3 T = normalize(input.WorldTangent);
            T = normalize(T - dot(T, N) * N);
            float3 B = cross(N, T);
            float3x3 TBN = float3x3(T, B, N);

            float3 NormalTex = BumpMap.Sample(SampleState, input.UV).rgb * 2.0f - 1.0f;
            N = normalize(mul(NormalTex, TBN));
        #endif
        
        SpecularTex = GetSpecularTexPS(input.UV);
        DiffuseTex = GetDiffuseTexPS(input.UV);
        
        #if VIEW_MODE == UNLIT
            finalColor.xyz = DiffuseTex;
    
        #elif VIEW_MODE == LIT_GOURAUD
            finalColor.xyz =
                DiffuseTex * input.VertexDiffuseLighting +
                SpecularTex * input.VertexSpecularLighting;

        #elif VIEW_MODE == LIT_LAMBERT
            #if LIGHT_CULLING_FLAG
                CalculateLightingLambertTile(input.WorldPos, N, TileIndex, DiffuseLighting, AmbientColor);
            #else
                CalculateLightingLambert(input.WorldPos, N, DiffuseLighting);
            #endif
            finalColor.xyz = DiffuseTex * DiffuseLighting;
        
        //ShaderHotReload 테스트용 코드 
            //finalColor.xyz = DiffuseTex * float3(1.0f, 1.0f, 1.0f);

        #elif VIEW_MODE == LIT_PHONG
            float3 SpecularLighting;
            #if LIGHT_CULLING_FLAG
                CalculateLightingBlinnPhongTile(
                    input.WorldPos,
                    N,
                    TileIndex,
                    DiffuseLighting,
                    SpecularLighting,
                    AmbientColor,
                    Shininess);
            #else
                CalculateLightingBlinnPhong(input.WorldPos, N, DiffuseLighting, SpecularLighting);
            #endif
            finalColor.xyz = DiffuseTex * DiffuseLighting + SpecularTex * SpecularLighting;

        #elif VIEW_MODE == WORLD_NORMAL
            finalColor.xyz = N * 0.5f + 0.5f;
        #endif

        Output.Color = finalColor;
        #endif

        #if OPAQUETYPE == STATICMESH
            Output.Normal = float4(N, 1.0f);
    #endif
    
    return Output;
}
