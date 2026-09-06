
#include "../Common/Common.hlsli"

#ifdef SKELETAL_MESH
StructuredBuffer<float4x4> SkinningMatrices : register(t16);

struct SkeletalVSInput
{
    float3 Position : POSITION;
    uint4 BoneIndices : BLENDINDICES;
    float4 BoneWeights : BLENDWEIGHT;
};

float4 ShadowVS(SkeletalVSInput input) : SV_POSITION
{
    float4 Pos = float4(input.Position, 1.0f);
    float4 weights = input.BoneWeights / dot(input.BoneWeights, 1.0f);

    float4 SkinnedPos =
        mul(SkinningMatrices[input.BoneIndices.x], Pos) * weights.x +
        mul(SkinningMatrices[input.BoneIndices.y], Pos) * weights.y +
        mul(SkinningMatrices[input.BoneIndices.z], Pos) * weights.z +
        mul(SkinningMatrices[input.BoneIndices.w], Pos) * weights.w;

    float4 worldPos = mul(SkinnedPos, Model);
    float4 post = worldPos;

#else
struct VSInput
{
    float3 Position : POSITION;
};

float4 ShadowVS(VSInput input) : SV_POSITION
{
    float4 worldPos = mul(float4(input.Position, 1.0f), Model);
    float4 post = worldPos;
#endif

#ifdef SHADOW_MAP_PSM
    float4 camClip = mul(post, VirtualViewProj);
    if (abs(camClip.w) > 1e-5f)
    {
        post = float4(camClip.xyz / camClip.w, 1.0f);
    }
#endif

    float4 shadowPos = mul(post, ShadowViewProj);
    return shadowPos;
}

void ShadowPS()
{
}
