#include "Common.hlsl"

struct GRID_VS_OUTPUT
{
    float4 Pos : SV_POSITION;
    float3 WorldPos : TEXCOORD0;
    float2 LocalPos : TEXCOORD1;
};

float2 SnapToGrid(float2 value, float gridSize)
{
    return floor(value / gridSize) * gridSize;
}

float ComputeGridLineAlpha(float2 coord, float2 derivative, float lineThickness)
{
    const float2 grid = abs(frac(coord - 0.5f) - 0.5f) / max(derivative, 0.0001f.xx);
    const float lineDistance = min(grid.x, grid.y);
    return saturate(lineThickness - lineDistance);
}

float ComputeAxisSuppressionAlpha(float2 coord, float2 derivative, float lineThickness)
{
    const float2 axisDistance = abs(coord) / max(derivative, 0.0001f.xx);
    const float axisLineDistance = min(axisDistance.x, axisDistance.y);
    return saturate(lineThickness - axisLineDistance);
}

float ComputeSingleAxisAlpha(float coord, float derivative, float lineThickness)
{
    const float axisDistance = abs(coord) / max(derivative, 0.0001f);
    return saturate(lineThickness - axisDistance);
}

GRID_VS_OUTPUT GridVS(uint VertexID : SV_VertexID)
{
    GRID_VS_OUTPUT output;

    static const float2 Positions[6] =
    {
        float2(-1.0f, -1.0f),
        float2(1.0f, -1.0f),
        float2(-1.0f, 1.0f),
        float2(1.0f, -1.0f),
        float2(-1.0f, 1.0f),
        float2(1.0f, 1.0f),
    };

    const float2 localPos = Positions[VertexID];
    const float2 snappedCenter = SnapToGrid(CameraPosition.xy, GridSize);
    const float2 planePos = snappedCenter + localPos * Range;
    const float3 worldPos = float3(planePos, 0.0f);

    output.WorldPos = worldPos;
    output.LocalPos = localPos;
    output.Pos = mul(float4(worldPos, 1.0f), mul(View, Projection));
    return output;
}

float4 GridPS(GRID_VS_OUTPUT input) : SV_Target
{
    const float dist = distance(input.WorldPos, CameraPosition.xyz);
    if (dist < 0.5f)
    {
        discard;
    }

    const float2 minorCoord = input.WorldPos.xy / GridSize;
    float2 minorDerivative = fwidth(input.LocalPos.xy) * (Range / GridSize);
    minorDerivative = max(minorDerivative, 0.0001f.xx);

    float minorAlpha = ComputeGridLineAlpha(minorCoord, minorDerivative, LineThickness);
    if (AxisThickness > 0.0f)
    {
        const float minorAxisSuppression =
            ComputeAxisSuppressionAlpha(minorCoord, minorDerivative, max(LineThickness, AxisThickness));
        minorAlpha *= (1.0f - minorAxisSuppression);
    }
    minorAlpha *= MinorIntensity;

    const float majorGridSize = GridSize * MajorLineInterval;
    const float2 majorCoord = input.WorldPos.xy / majorGridSize;
    float2 majorDerivative = fwidth(input.LocalPos.xy) * (Range / majorGridSize);
    majorDerivative = max(majorDerivative, 0.0001f.xx);

    float majorAlpha = ComputeGridLineAlpha(majorCoord, majorDerivative, MajorLineThickness);
    if (AxisThickness > 0.0f)
    {
        const float majorAxisSuppression =
            ComputeAxisSuppressionAlpha(majorCoord, majorDerivative, max(MajorLineThickness, AxisThickness));
        majorAlpha *= (1.0f - majorAxisSuppression);
    }
    majorAlpha *= MajorIntensity;

    const float minorLodFade = saturate(0.8f - max(minorDerivative.x, minorDerivative.y));
    const float majorLodFade = saturate(1.2f - max(majorDerivative.x, majorDerivative.y));
    minorAlpha *= minorLodFade;
    majorAlpha *= majorLodFade;

    const float fade = pow(saturate(1.0f - dist / MaxDistance), 2.0f);
    const float gridAlpha = max(minorAlpha, majorAlpha) * fade;
    const float3 gridColor = lerp(float3(0.35f, 0.35f, 0.35f), float3(0.55f, 0.55f, 0.55f), saturate(majorAlpha));

    float axisXAlpha = 0.0f;
    float axisYAlpha = 0.0f;
    if (AxisThickness > 0.0f)
    {
        axisXAlpha = ComputeSingleAxisAlpha(minorCoord.x, minorDerivative.x, AxisThickness) * fade;
        axisYAlpha = ComputeSingleAxisAlpha(minorCoord.y, minorDerivative.y, AxisThickness) * fade;
    }

    const float finalAlpha = max(gridAlpha, max(axisXAlpha, axisYAlpha));
    if (finalAlpha < 0.01f)
    {
        discard;
    }

    const float totalWeight = gridAlpha + axisXAlpha + axisYAlpha;
    float3 color = gridColor;
    if (totalWeight > 0.0001f)
    {
        color =
            (gridColor * gridAlpha +
             float3(0.2f, 1.0f, 0.2f) * axisXAlpha +
             float3(1.0f, 0.2f, 0.2f) * axisYAlpha) / totalWeight;
    }

    return float4(color, finalAlpha);
}
