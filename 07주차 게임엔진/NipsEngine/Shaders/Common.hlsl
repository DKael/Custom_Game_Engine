/* Constant Buffers */


cbuffer FrameBuffer : register(b0)
{
    row_major float4x4 View;
    row_major float4x4 InvView;
    row_major float4x4 Projection;
    row_major float4x4 InvProjection;
    float bIsWireframe;
    float3 WireframeRGB;
    
    row_major float4x4 InverseViewProjection;
    
    float3 CameraWorldPos;
    float Padding0;
}

cbuffer PerObjectBuffer : register(b1)
{
    row_major float4x4 Model;
    row_major float4x4 InvModel;
    float4 PrimitiveColor; 
};

cbuffer GizmoBuffer : register(b2)
{
    float4 GizmoColorTint;
    uint bIsInnerGizmo;
    uint bClicking;
    uint SelectedAxis;
    float HoveredAxisOpacity;
};

// 현재 사용 안 하는 버퍼
//cbuffer OverlayBuffer : register(b3)
//{
//    float2 OverlayCenterScreen;
//    float2 ViewportSize;

//    float OverlayRadius;
//    float3 Padding2;

//    float4 OverlayColor;
//};

cbuffer EditorBuffer : register(b4)
{
    float4 CameraPosition;
    
    float MaxDistance; // 거리 fade 최대 거리
    float Range; // 카메라 주변에 생성할 grid 반경
    float GridSize; // minor grid 한 칸 크기
    float LineThickness; // minor line 두께 계수

    float MajorLineThickness; // major line 두께 계수
    float MajorLineInterval; // 몇 칸마다 major line을 그릴지 (예: 10)
    float MinorIntensity; // minor line 밝기/alpha 계수
    float MajorIntensity; // major line 밝기/alpha 계수
    
    float AxisThickness; // 축선 두께 배수
    float AxisLength; // 축 길이
    float2 EditorPadding0;
};

cbuffer OutlineConstants : register(b5)
{
    float4 OutlineColor;
    float OutlineThicknessPixels;
    float2 OutlineViewportSize;
    float OutlinePadding0;
};

float4 ApplyMVP(float3 pos)
{
    // Common.hlsl이 변경되었을 때 이에 의존하는 셰이더들이 정상적으로 리로드되는지 테스트하기 위한 코드.
    //pos.y += 1.0f;

    float4 world = mul(float4(pos, 1.0f), Model);
    float4 view = mul(world, View);
    return mul(view, Projection);
}

//// 누군가 리펙토링을 한다면 역행렬은 CPU에서 넘겨주시길..
//// 급하게 퐁 셰이딩 적용해서 이렇게 넣은 겁니다.
//float3x3 Inverse3x3(float3x3 m)
//{
//    float det = m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1])
//              - m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0])
//              + m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);

//    float invDet = 1.0 / det;

//    float3x3 result;
//    result[0][0] = (m[1][1] * m[2][2] - m[1][2] * m[2][1]) * invDet;
//    result[0][1] = -(m[0][1] * m[2][2] - m[0][2] * m[2][1]) * invDet;
//    result[0][2] = (m[0][1] * m[1][2] - m[0][2] * m[1][1]) * invDet;
//    result[1][0] = -(m[1][0] * m[2][2] - m[1][2] * m[2][0]) * invDet;
//    result[1][1] = (m[0][0] * m[2][2] - m[0][2] * m[2][0]) * invDet;
//    result[1][2] = -(m[0][0] * m[1][2] - m[0][2] * m[1][0]) * invDet;
//    result[2][0] = (m[1][0] * m[2][1] - m[1][1] * m[2][0]) * invDet;
//    result[2][1] = -(m[0][0] * m[2][1] - m[0][1] * m[2][0]) * invDet;
//    result[2][2] = (m[0][0] * m[1][1] - m[0][1] * m[1][0]) * invDet;
//    return result;
//}

