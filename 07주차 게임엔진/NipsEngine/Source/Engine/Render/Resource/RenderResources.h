#pragma once

/*
        Shader, Constant Buffer 등 렌더링에 필요한 리소스들을 관리하는 Class 입니다.
        Renderer에서 필요한 리소스들을 FRenderResources에 추가하여 관리할 수 있습니다.
*/

#include "Render/Resource/Shader.h"
#include "Render/Resource/Buffer.h"

struct FRenderResources
{
    FConstantBuffer FrameBuffer;                  // b0
    FConstantBuffer PerObjectConstantBuffer;      // b1
    FConstantBuffer GizmoPerObjectConstantBuffer; // b2
    FConstantBuffer EditorConstantBuffer;         // b4
    FConstantBuffer OutlineConstantBuffer;        // b5

    FConstantBuffer StaticMeshConstantBuffer; // b6
    FConstantBuffer DecalConstantBuffer;      // b7
    FConstantBuffer FireBallConstantBuffer;   // b8
    FConstantBuffer FogConstantBuffer;				// b9
    FConstantBuffer SceneDepthBuffer;				// b10
    FConstantBuffer ForwardPlusConstantBuffer;   // b11
    FConstantBuffer FXAAConstantBuffer;   // b9
    FConstantBuffer LightingConstantBuffer; // b13

	FStructuredBuffer DirectionalLightBuffer;
    FStructuredBuffer SpotLightBuffer;
    FStructuredBuffer PointlLightBuffer;
    FRWStructuredBuffer TilePointLightGrid;
    FRWStructuredBuffer TilePointLightIndices;
    FRWStructuredBuffer TileSpotLightGrid;
    FRWStructuredBuffer TileSpotLightIndices;

    FShader PrimitiveShader;
    FShader GizmoShader;
    FShader EditorShader;
    FShader GridShader;
    FShader AxisShader;
    FShader SelectionMaskShader;
    FShader OutlineShader;
    FShader StaticMeshShader;
    FShader DecalShader;
    FShader DepthVisualizerShader;
	FShader FogShader;
    FShader FireBallShader;
    FShader LightHitmapOverlayShader;
    FShader FXAAShader;
    FShader UberLitShader;
    FShader DepthPrepassShader;
    FComputeShader TileLightCullingCS;

    TComPtr<ID3D11SamplerState> MeshSamplerState;
    TComPtr<ID3D11SamplerState> FXAASamplerState;
};
