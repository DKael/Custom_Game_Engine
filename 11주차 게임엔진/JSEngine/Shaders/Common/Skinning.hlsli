#ifndef SKINNING_H
#define SKINNING_H

#if USE_CPU_SKINNING
#else
StructuredBuffer<float4x4> SkinningMatrices : register(t16);
#endif

struct SkinnedVertex
{
    float3 Position;
    float3 Normal;
    float3 Tangent;
};

float4 NormalizeWeights(float4 w)
{
    float sum = w.x + w.y + w.z + w.w;
    return (sum > 0.0f) ? w / sum : float4(1, 0, 0, 0);
}

SkinnedVertex ApplySkinning(
    float3 position,
    float3 normal,
    float3 tangent,
    uint4 indices,
    float4 weights
)
{
    SkinnedVertex o;

#if USE_CPU_SKINNING
    o.Position = position;
    o.Normal = normal;
    o.Tangent = tangent;
#else
    float4 w = NormalizeWeights(weights);
    float4 pos = float4(position, 1.0f);

    float4 skinnedPos = 0;
    float3 skinnedNormal = 0;
    float3 skinnedTangent = 0;

    [unroll]
    for (int i = 0; i < 4; i++)
    {
        uint idx = indices[i];
        float weight = w[i];

        skinnedPos += mul(SkinningMatrices[idx], pos) * weight;
        skinnedNormal += mul((float3x3) SkinningMatrices[idx], normal) * weight;
        skinnedTangent += mul((float3x3) SkinningMatrices[idx], tangent) * weight;
    }

    o.Position = skinnedPos.xyz;
    o.Normal = normalize(skinnedNormal);
    o.Tangent = normalize(skinnedTangent);
#endif

    return o;
}

#endif