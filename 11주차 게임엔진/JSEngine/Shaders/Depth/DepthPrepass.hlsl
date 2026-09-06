#include "../Common/Common.hlsli"


#ifdef SKELETAL_MESH
StructuredBuffer<float4x4> SkinningMatrices : register(t16);

struct SkeletalVSInput
{
    float3 Position : POSITION;
    uint4 BoneIndices : BLENDINDICES;
    float4 BoneWeights : BLENDWEIGHT;
};

float4 DepthPrepassVS(SkeletalVSInput input) : SV_POSITION
{
    float4 Pos = float4(input.Position, 1.0f);
    float4 weights = input.BoneWeights / dot(input.BoneWeights, 1.0f);

    float4 SkinnedPos =
        mul(SkinningMatrices[input.BoneIndices.x], Pos) * weights.x +
        mul(SkinningMatrices[input.BoneIndices.y], Pos) * weights.y +
        mul(SkinningMatrices[input.BoneIndices.z], Pos) * weights.z +
        mul(SkinningMatrices[input.BoneIndices.w], Pos) * weights.w;

    return ApplyMVP(SkinnedPos.xyz);
}
#else
struct VSInput
{
    float3 Position : POSITION;
};

float4 DepthPrepassVS(VSInput input) : SV_POSITION
{
    return ApplyMVP(input.Position);
}
#endif

void DepthPrepassPS() {}
