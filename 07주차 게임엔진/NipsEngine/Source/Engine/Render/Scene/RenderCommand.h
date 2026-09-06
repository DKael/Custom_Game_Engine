#pragma once

/*
        Constants Buffer에 사용될 구조체와
        에 담길 RenderCommand 구조체를 정의하고 있습니다.
        RenderCommand는 Renderer에서 Draw Call을 1회 수행하기 위해 필요한 정보를 담고 있습니다.
*/

#include "Render/Common/RenderTypes.h"
#include "Render/Resource/Buffer.h"
#include "Render/Device/D3DDevice.h"
#include "Core/CoreMinimal.h"
#include "Core/ResourceTypes.h"

#include "Math/Matrix.h"
#include "Math/Vector.h"
#include "Math/Vector2.h"

struct ID3D11ShaderResourceView;

enum class ERenderCommandType
{
    Primitive,
    Gizmo,
    SelectionMask,
    PostProcessOutline,
    Billboard,
    DebugBox,
    Grid,       // Grid 패스 — LineBatcher 경유
    Font,       // TextRenderComponent — FontBatcher 경유
    SubUV,      // SubUVComponent     — SubUVBatcher 경유
    StaticMesh, // UStaticMeshComponent — OBJ 메시 퐁셰이딩
    Decal,
    FireBall, // 사실 라이트랑 비슷한놈 아닐까요
    Fog,

};

// PerObject
struct FPerObjectConstants
{
    FMatrix  Model;
    FMatrix  InvModel;
    FVector4 Color;
};

struct FFrameConstants
{
    FMatrix View;
    FMatrix InvView;
    FMatrix Projection;
    FMatrix InvProjection;
    float   bIsWireframe = 0.0f;
    FVector WireframeColor;

	FMatrix InverseViewProjection;
    FVector CameraWorldPos;
	float Padding0;
};

struct FGizmoConstants
{
    FVector4 ColorTint;
    uint32   bIsInnerGizmo;
    uint32   bClicking;
    uint32   SelectedAxis;
    float    HoveredAxisOpacity;
};

struct FEditorConstants
{
    FVector4 CameraPosition = FVector4::ZeroPoint();

    float MaxDistance = 10000.0f;
    float Range = 100.0f;
    float GridSize = 1.0f;
    float LineThickness = 1.0f;

    float MajorLineThickness = 1.25f;
    float MajorLineInterval = 10.0f;
    float MinorIntensity = 0.65f;
    float MajorIntensity = 1.0f;

    float    AxisThickness = 1.75f;
    float    AxisLength = 3000.0f;
    FVector2 EditorPadding0 = FVector2(0.0f, 0.0f);
};

struct FOutlineConstants
{
    FVector4 OutlineColor = FVector4(1.0f, 0.5f, 0.0f, 1.0f); // RGBA
    float    OutlineThicknessPixels = 2.0f;
    FVector2 ViewportSize = FVector2(1.0f, 1.0f);
    float    Padding0 = 0.0f;
};

struct FAABBConstants
{
    FVector Min;
    float   Padding0;

    FVector Max;
    float   Padding1;

    FColor Color;
};

struct FGridConstants
{
    float  GridSpacing = 1.0f;
    int32  GridHalfLineCount = 100;
    uint32 bOrthographic = 0;
    float  LineThickness = 1.0f;

    float MajorLineThickness = 1.35f;
    float MajorLineInterval = 10.0f;
    float MinorIntensity = 0.65f;
    float MajorIntensity = 1.0f;

    float RangeScale = 1.0f;
    float MaxDistanceScale = 1.5f;
    float AxisThickness = 1.75f;
    float AxisLengthScale = 1.0f;
};

struct FFontConstants
{
    const FString*       Text = nullptr; // 컴포넌트 소유 문자열 참조 (프레임 내 유효)
    const FFontResource* Font = nullptr;
    float                Scale = 1.0f;
};

struct FSubUVConstants
{
    const FParticleResource* Particle = nullptr;
    uint32                   FrameIndex = 0;
    float                    Width = 1.0f;
    float                    Height = 1.0f;
};
struct FBillboardConstants
{
    ID3D11ShaderResourceView* SRV = nullptr;
    float                     Width = 1.0f;
    float                     Height = 1.0f;
};
struct FDecalConstants
{
    FMatrix InverseClipToLocal;
    float   FadeAlpha;
    FVector AmbientColor;

	FVector DiffuseColor;
    uint32  bHasDiffuseMap;

	FVector SpecularColor;
    uint32  bHasSpecularMap;

    uint32  bHasNormalMap;
    FVector padding;

	ID3D11ShaderResourceView* DiffuseSRV = {nullptr};
    ID3D11ShaderResourceView* AmbientSRV = {nullptr};
    ID3D11ShaderResourceView* SpecularSRV = {nullptr};
    ID3D11ShaderResourceView* BumpSRV = {nullptr};
};

struct FSceneDepthConstants
{
    FVector2 ViewportUVOffset;
    FVector2 ViewportUVScale;
    FVector2 DepthTextureSize;
    FVector2 Pad = FVector2(0.0f, 0.0f);
};

struct FFogConstants
{
    FVector4 InscatteringColor = FVector4(0.5f, 0.6f, 0.7f, 1.0f);

    float Density = 0.02f;
    float HeightFalloff = 0.2f;
    float StartDistance = 0.0f;
    float CutoffDistance = 0.0f;

    float MaxOpacity = 1.0f;
    float FogHeight = 0.0f;
    float Pad0 = 0.0f;
    float Pad1 = 0.0f;

    FVector CameraWorldPos = FVector::ZeroVector;
    float   Pad2 = 0.0f;

    FMatrix InverseViewProjection = FMatrix::Identity;

    uint32 bEnabled = 0;
    float  Pad3[3] = {0.0f, 0.0f, 0.0f};
};

struct FFireBallConstants
{
    FMatrix InverseClipToLocal;
    float   Intensity;
    float   Radius;
    float   RadiusFallOff;
    float   Padding;
};
struct FFXAAConstants
{
    FVector2 InvResolution = FVector2(1.0f, 1.0f);
    FVector2 ViewportUVMin = FVector2(0.0f, 0.0f);
    FVector2 ViewportUVSize = FVector2(1.0f, 1.0f);
    float    FxaaQualitySubpix = 0.75f;
    float    FxaaQualityEdgeThreshold = 0.125f;
    float    FxaaQualityEdgeThresholdMin = 0.0312f;
    float    FxaaEnabled = 0.0f;
    float    Padding[2] = {0.0f, 0.0f};
};

struct FAmbientLightConstants
{

    FVector Color = {0.0f, 0.0f, 0.0f};
    float   Intensity = 0.0f;
};

struct FDirectionalLightConstants
{
    FVector Direction = {0.0f, 0.0f, 0.0f};
    float   Intensity = 0.0f;
    FVector Color = {0.0f, 0.0f, 0.0f};
    float   Padding = 0.0f;
};

struct FPointLightConstatns
{
    FVector Position = {0.f, 0.f, 0.f};
    float   Radius = 10.f;
    FVector Color = {1.0f, 1.0f, 1.0f};
    float   Intensity = 10.0f;
};

struct FLightingConstants
{
    FAmbientLightConstants AmbientLight;
    uint32                 DirectionalLightCount = 0;
    uint32                 PointLightCount = 0;
    uint32                 SpotLightCount = 0;
    float                  Padding = 0.0f;
};

struct ForwardPlusConstants
{
    uint32 ViewportMin[2];
    uint32 ViewportSize[2];
    uint32 DepthTextureSize[2];
    uint32 TileCount[2];
    uint32 bEnable25DMask;
    float  Padding[3] = {
        0.0f,
    };
};

// StaticMeshBuffer (b6) — UberLit.hlsl 대응
// 완전 Obj전용입니다. 추후 Bump를 Normal로 바꾸면 됩니다.
struct FStaticMeshConstants
{
    // Phong Material
    FVector AmbientColor = {0.2f, 0.2f, 0.2f};
    float   _Pad0 = 0.0f;

    FVector DiffuseColor = {0.8f, 0.8f, 0.8f};
    float   _Pad1 = 0.0f;

    FVector SpecularColor = {0.5f, 0.5f, 0.5f};
    float   Shininess = 32.0f;

    // ScrollUV
    float  ScrollX = 0.f;
    float  ScrollY = 0.f;
    uint32 bHasDiffuseMap = 0;
    uint32 bHasSpecularMap = 0;

    uint32 bHasNormalMap = 0;
    float  Padding1 = 0.f;
    float  Padding2 = 0.f;
    float  Padding3 = 0.f;

    // Texture SRV (CPU-only, cbuffer 범위 밖)
    ID3D11ShaderResourceView* DiffuseSRV = {nullptr};
    ID3D11ShaderResourceView* AmbientSRV = {nullptr};
    ID3D11ShaderResourceView* SpecularSRV = {nullptr};
    ID3D11ShaderResourceView* BumpSRV = {nullptr};
};

struct FRenderCommand
{
    //	VB, IB 모두 담고 있는 MB
    FMeshBuffer*        MeshBuffer = nullptr;
    uint32              SectionIndexStart = {};
    uint32              SectionIndexCount = {};
    float               SortKey = 0.f; // Decal Sort 순서.
    FPerObjectConstants PerObjectConstants = {};

    union
    {
        FGizmoConstants      Gizmo;
        FEditorConstants     Editor;
        FOutlineConstants    Outline;
        FAABBConstants       AABB;
        FGridConstants       Grid;
        FFontConstants       Font;
        FSubUVConstants      SubUV;
        FBillboardConstants  Billboard; // ← 추가
        FStaticMeshConstants StaticMesh;
        FDecalConstants      Decal;
        FFireBallConstants   FireBall;
    } Constants;

    EDepthStencilState DepthStencilState = static_cast<EDepthStencilState>(-1);
    EBlendState        BlendState = static_cast<EBlendState>(-1);
    ERenderCommandType Type = ERenderCommandType::Primitive;
};
