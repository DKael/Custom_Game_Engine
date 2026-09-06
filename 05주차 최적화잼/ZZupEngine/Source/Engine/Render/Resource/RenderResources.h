#pragma once

/*
	Shader, Constant Buffer 등 렌더링에 필요한 리소스들을 관리하는 Class 입니다.
	Renderer에서 필요한 리소스들을 FRenderResources에 추가하여 관리할 수 있습니다.
*/

#include "Render/Resource/Shader.h"
#include "Render/Resource/Buffer.h"

struct FRenderResources
{
	FConstantBuffer FrameBuffer;					        // b0
    FConstantBuffer PerObjectConstantBuffer;                // b1
    FConstantBuffer GizmoPerObjectConstantBuffer;           // b2
    FConstantBuffer EditorConstantBuffer;                   // b4
	FConstantBuffer OutlineConstantBuffer;				    // b5
    FConstantBuffer StaticMeshConstantBuffer;				// b6
	FConstantBuffer PerformanceStaticMeshConstantBuffer;    // b6
	FConstantBuffer PerObjectPerformanceBuffer;			    // b7

    FShader PrimitiveShader;
    FShader GizmoShader;
    FShader EditorShader;
	FShader SelectionMaskShader;
    FShader OutlineShader;
    FShader StaticMeshShader;
    FShader PerformanceStaticMeshShader;
    FShader DepthPrepassShader;         // Depth Prepass 전용 (VS only)

	FComputeShader  HiZDownsampleCS;    // HiZ reverse-Z min-depth downsample (Step 3)
	FComputeShader  HiZCopyMip0CS;      // DepthPrepass -> HiZ mip0 복사
	FConstantBuffer HiZConstantBuffer;  // b0 for HiZ CS (SrcW, SrcH, DstW, DstH)

	FComputeShader  OcclusionCullCS;           // GPU 가시성 테스트 (Step 5)
	FConstantBuffer OcclusionCullConstantBuffer; // b0 for Occlusion CS

	TComPtr<ID3D11SamplerState> MeshSamplerState;
};
