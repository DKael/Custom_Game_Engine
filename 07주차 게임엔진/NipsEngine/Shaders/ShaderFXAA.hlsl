#include "Common.hlsl"

Texture2D SceneTex : register(t0);
SamplerState LinearClamp : register(s0);

cbuffer FXAAConstants : register(b9)
{
    // 현재 SubViewport 기준 1 texel 크기.
    // shared host RT 위에서 동작하더라도 FXAA의 샘플 간격은 "현재 뷰포트 해상도"를 기준으로 계산한다.
    // 예: viewport가 640x360이면 (1/640, 1/360)
    float2 InvResolution;

    // 현재 SubViewport가 host texture 안에서 시작하는 UV 좌표.
    // 예: 2x2 분할에서 우상단 viewport라면 대략 (0.5, 0.0) 부근이 된다.
    float2 ViewportUVMin;

    // 현재 SubViewport가 host texture 안에서 차지하는 UV 크기.
    // 예: 2x2 분할에서 각 칸이 같은 크기면 대략 (0.5, 0.5)
    float2 ViewportUVSize;

    // FXAA 품질 파라미터.
    // Subpix: 미세한 aliasing을 얼마나 부드럽게 할지
    // EdgeThreshold / EdgeThresholdMin: 어느 정도 대비부터 edge로 볼지
    float FxaaQualitySubpix;
    float FxaaQualityEdgeThreshold;
    float FxaaQualityEdgeThresholdMin;

    // 1이면 실제 FXAA 수행, 0이면 현재 viewport 구간을 그대로 복사(copy path)한다.
    // shared postprocess RT를 항상 최종 출력으로 쓰기 위해, FXAA off여도 pass 자체는 돈다.
    float FxaaEnabled;
    float2 Padding9;
};

static const int FXAA_SEARCH_STEPS = 10;

float Luma(float3 c)
{
    // FXAA는 색 자체보다 "밝기 변화"를 보고 edge를 찾는다.
    // 따라서 RGB를 luma로 변환해서 대비를 계산한다.
    return dot(c, float3(0.299f, 0.587f, 0.114f));
}

float2 ClampViewportUv(float2 localUv)
{
    // edge search가 viewport 바깥으로 나가더라도 현재 SubViewport 경계에 고정되게 한다.
    return clamp(localUv, float2(0.0f, 0.0f), float2(1.0f, 1.0f));
}

float2 ToSceneUv(float2 localUv)
{
    // local viewport UV(0..1)를 host texture UV로 변환한다.
    // 이 변환 덕분에 shared host RT 구조에서도 현재 viewport 부분만 안전하게 샘플할 수 있다.
    return ViewportUVMin + ClampViewportUv(localUv) * ViewportUVSize;
}

float3 SampleRgb(float2 localUv)
{
    // The fullscreen triangle is rasterized only over the active D3D viewport, but the
    // input texture still contains every editor viewport packed into one host RT.
    // Convert the local viewport UV into the matching host UV so the search never
    // samples splitter gaps or neighboring sub-viewports.
    return SceneTex.Sample(LinearClamp, ToSceneUv(localUv)).rgb;
}

struct FxaaVS_Output
{
    float4 Pos : SV_POSITION;
    float2 UV : TEXCOORD0;
};

FxaaVS_Output FxaaVS(uint VertexID : SV_VertexID)
{
    FxaaVS_Output Output;

    // Standard fullscreen triangle generated from SV_VertexID.
    // UV still spans 0..1 over the currently active D3D viewport.
    //
    // 중요한 점:
    // 이 삼각형은 "현재 바인딩된 RT 전체"가 아니라 "현재 D3D viewport"를 덮는다.
    // 즉, host-sized RT 위에서도 RSSetViewports로 지정한 SubViewport 한 칸만 rasterize된다.
    Output.UV = float2((VertexID << 1) & 2, VertexID & 2);
    Output.Pos = float4(Output.UV * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);

    return Output;
}

float4 FxaaPS(FxaaVS_Output Input) : SV_Target
{
    const float2 texel = InvResolution;
    const float2 localUV = ClampViewportUv(Input.UV);

    // 중심 픽셀의 색과 luma.
    float3 rgbM = SampleRgb(localUV);
    float lumaM = Luma(rgbM);

    // Disabled path still copies the current viewport from SceneColor to the shared
    // post-process RT so later overlay passes and final display use one texture.
    if (FxaaEnabled < 0.5f)
    {
        return float4(rgbM, 1.0f);
    }

    float lumaN = Luma(SampleRgb(localUV + float2(0.0f, -texel.y)));
    float lumaS = Luma(SampleRgb(localUV + float2(0.0f, texel.y)));
    float lumaW = Luma(SampleRgb(localUV + float2(-texel.x, 0.0f)));
    float lumaE = Luma(SampleRgb(localUV + float2(texel.x, 0.0f)));

    float lumaMin = min(lumaM, min(min(lumaN, lumaS), min(lumaW, lumaE)));
    float lumaMax = max(lumaM, max(max(lumaN, lumaS), max(lumaW, lumaE)));
    float lumaRange = lumaMax - lumaMin;

    // early-out 판단.
    // 주변 밝기 변화가 작으면 aliasing이 거의 없다고 보고 원본을 그대로 반환한다.
    // 이 단계가 없으면 평평한 영역까지 불필요하게 blur될 수 있다.
    float rangeThreshold = max(FxaaQualityEdgeThresholdMin,
                               lumaMax * FxaaQualityEdgeThreshold);

    if (lumaRange < rangeThreshold)
    {
        return float4(rgbM, 1.0f);
    }

    float lumaNW = Luma(SampleRgb(localUV + texel * float2(-1.0f, -1.0f)));
    float lumaNE = Luma(SampleRgb(localUV + texel * float2(1.0f, -1.0f)));
    float lumaSW = Luma(SampleRgb(localUV + texel * float2(-1.0f, 1.0f)));
    float lumaSE = Luma(SampleRgb(localUV + texel * float2(1.0f, 1.0f)));

    // 주변 8개 샘플의 luma 패턴으로 edge가 수평/수직 중 어느 쪽에 가까운지 추정한다.
    // FXAA는 이 방향 정보를 바탕으로 "어느 축을 따라 더 찾아가야 하는지"를 결정한다.
    float edgeHorizontal =
        abs((lumaNW + lumaSW) - 2.0f * lumaW) +
        abs((lumaN + lumaS) - 2.0f * lumaM) * 2.0f +
        abs((lumaNE + lumaSE) - 2.0f * lumaE);

    float edgeVertical =
        abs((lumaNW + lumaNE) - 2.0f * lumaN) +
        abs((lumaW + lumaE) - 2.0f * lumaM) * 2.0f +
        abs((lumaSW + lumaSE) - 2.0f * lumaS);

    // True means the visible edge runs horizontally, so the blend moves along Y
    // while the edge span search walks along X.
    bool isHorizontal = edgeHorizontal >= edgeVertical;

    float luma1 = isHorizontal ? lumaN : lumaW;
    float luma2 = isHorizontal ? lumaS : lumaE;

    float gradient1 = abs(luma1 - lumaM);
    float gradient2 = abs(luma2 - lumaM);

    // 중심 픽셀 기준으로 어느 쪽 기울기가 더 가파른지 선택한다.
    // 이 값은 edge 중심선으로 half-texel 이동할 때 어느 방향으로 갈지 정하는 데 사용된다.
    bool is1Steepest = gradient1 >= gradient2;
    float gradientScale = 0.25f * max(gradient1, gradient2);

    float stepLength = isHorizontal ? texel.y : texel.x;
    float lumaLocalAverage;

    if (is1Steepest)
    {
        stepLength = -stepLength;
        lumaLocalAverage = 0.5f * (luma1 + lumaM);
    }
    else
    {
        lumaLocalAverage = 0.5f * (luma2 + lumaM);
    }

    float2 currentUv = localUV;
    if (isHorizontal)
        currentUv.y += 0.5f * stepLength;
    else
        currentUv.x += 0.5f * stepLength;

    // edge를 "따라가는" 방향의 1 texel step.
    // 수평 edge면 x축 방향으로 span을 찾고, 수직 edge면 y축 방향으로 span을 찾는다.
    float2 searchStep = isHorizontal ? float2(texel.x, 0.0f)
                                     : float2(0.0f, texel.y);

    float2 uvNeg = currentUv - searchStep;
    float2 uvPos = currentUv + searchStep;

    float lumaEndNeg = Luma(SampleRgb(uvNeg)) - lumaLocalAverage;
    float lumaEndPos = Luma(SampleRgb(uvPos)) - lumaLocalAverage;

    bool doneNeg = abs(lumaEndNeg) >= gradientScale;
    bool donePos = abs(lumaEndPos) >= gradientScale;

    // edge span search.
    // 중심에서 양쪽으로 이동하면서 "이 edge가 어디까지 이어지는지" 찾는다.
    // 최종적으로 현재 픽셀이 edge span 안에서 어느 쪽에 더 가까운지 계산해 보정량을 만든다.
    [unroll]
    for (int i = 0; i < FXAA_SEARCH_STEPS; ++i)
    {
        if (!doneNeg)
        {
            uvNeg -= searchStep;
            lumaEndNeg = Luma(SampleRgb(uvNeg)) - lumaLocalAverage;
            doneNeg = abs(lumaEndNeg) >= gradientScale;
        }

        if (!donePos)
        {
            uvPos += searchStep;
            lumaEndPos = Luma(SampleRgb(uvPos)) - lumaLocalAverage;
            donePos = abs(lumaEndPos) >= gradientScale;
        }
    }

    float distanceNeg = isHorizontal ? (localUV.x - uvNeg.x) : (localUV.y - uvNeg.y);
    float distancePos = isHorizontal ? (uvPos.x - localUV.x) : (uvPos.y - localUV.y);

    bool isNegNearest = distanceNeg < distancePos;
    float distanceNearest = min(distanceNeg, distancePos);
    float edgeSpan = max(distanceNeg + distancePos, 1e-4f);

    // edge span 안에서의 상대 위치를 0~1 느낌의 offset으로 바꾼다.
    // span 중앙 쪽으로 샘플을 조금 이동시켜 계단 현상을 부드럽게 만든다.
    float pixelOffset = -distanceNearest / edgeSpan + 0.5f;

    float lumaCenterDelta = lumaM - lumaLocalAverage;
    float lumaEndNearest = isNegNearest ? lumaEndNeg : lumaEndPos;

    // 잘못된 방향으로 번지는 것을 막기 위한 안전장치.
    // edge 중심과 nearest edge end의 밝기 변화 방향이 맞지 않으면 edge offset을 버린다.
    bool correctVariation = (lumaCenterDelta < 0.0f) != (lumaEndNearest < 0.0f);
    float edgeOffset = correctVariation ? pixelOffset : 0.0f;

    float lumaAverage =
        (2.0f * (lumaN + lumaS + lumaW + lumaE) +
         lumaNW + lumaNE + lumaSW + lumaSE) / 12.0f;

    float subPixel1 = saturate(abs(lumaAverage - lumaM) / max(lumaRange, 1e-4f));
    float subPixel2 = subPixel1 * subPixel1 * (3.0f - 2.0f * subPixel1);
    float subPixelOffset = subPixel2 * subPixel2 * FxaaQualitySubpix;

    // 최종 이동량은
    // 1) edge span 기반 보정
    // 2) sub-pixel aliasing 보정
    // 중 더 강한 쪽을 선택한다.
    float finalOffset = max(edgeOffset, subPixelOffset);

    float2 finalUv = localUV;
    if (isHorizontal)
        finalUv.y += finalOffset * stepLength;
    else
        finalUv.x += finalOffset * stepLength;

    // 최종적으로 보정된 위치에서 다시 색을 읽어 반환한다.
    // 이때 SampleRgb는 내부에서 viewport-local clamp + host UV 변환을 수행한다.
    return float4(SampleRgb(finalUv), 1.0f);
}
