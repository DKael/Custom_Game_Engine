#pragma once

/*
        실제 렌더링을 담당하는 Class 입니다. (Rendering 최상위 클래스)
*/

#include "Render/Common/RenderTypes.h"
#include "Render/Resource/VertexTypes.h"

#include "Render/Scene/RenderBus.h"
#include "Render/Device/D3DDevice.h"
#include "Core/FileWatcher.h"
#include "Render/Resource/ShaderManager.h"
#include "Render/Resource/RenderResources.h"
#include "Render/LineBatcher.h"
#include "Render/FontBatcher.h"
#include "Render/SubUVBatcher.h"

#include <cstddef>
#include <functional>

#include "Render/Scene/LightInfo.h"

// 패스별 Batcher 바인딩 — Clear → Collect → Flush 패턴
struct FPassBatcherBinding
{
    std::function<void()>                                                     Clear;
    std::function<void(const FRenderCommand&, const FRenderBus&)>             Collect;
    std::function<void(ERenderPass, const FRenderBus&, ID3D11DeviceContext*)> Flush;

    explicit operator bool() const { return Flush != nullptr; }
};

// 패스별 기본 렌더 상태 — Single Source of Truth
struct FPassRenderState
{
    EDepthStencilState       DepthStencil = EDepthStencilState::Default;
    EBlendState              Blend = EBlendState::Opaque;
    ERasterizerState         Rasterizer = ERasterizerState::SolidBackCull;
    D3D11_PRIMITIVE_TOPOLOGY Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    FShader*                 Shader = nullptr;        // nullptr → batcher가 자체 셰이더 사용
    bool                     bWireframeAware = false; // Wireframe 모드 시 래스터라이저 전환
};

struct FGridShaderPassState
{
    bool             bDrawGrid = false;
    bool             bDrawAxis = false;
    FEditorConstants Constants = {};
};

class FRenderer
{
  public:
    void Create(HWND hWindow);
    void Release();

    void PrepareBatchers(const FRenderBus& InRenderBus);
    void BeginFrame();
    void Render(const FRenderBus& InRenderBus, const FFXAASettings* InFXAASettings = nullptr);
    void EndFrame();
    void UseBackBufferRenderTargets();
    void UseViewportRenderTargets();

    FD3DDevice&       GetFD3DDevice() { return Device; }
    FRenderResources& GetResources() { return Resources; }

  private:
    void InitializePassRenderStates();
    void InitializePassBatchers();

    void ApplyPassRenderState(ERenderPass Pass, ID3D11DeviceContext* Context, EViewMode ViewMode);
    void BindShaderByType(const FRenderCommand& InCmd, ID3D11DeviceContext* Context,
                          ERenderCommandType& LastCommandType, const FRenderBus& InRenderBus);

    void RenderScenePasses(ID3D11DeviceContext* Context, const FRenderBus& InRenderBus);
    void RenderPostProcess(ID3D11DeviceContext* Context, const FRenderBus& InRenderBus,
                           const FFXAASettings* InFXAASettings);
    void RenderEditorOverlay(ID3D11DeviceContext* Context, const FRenderBus& InRenderBus);
    void ExecuteSinglePass(ERenderPass Pass, ID3D11DeviceContext* Context, const FRenderBus& InRenderBus);
    void BuildGridShaderConstants(const FRenderCommand& InCommand, const FRenderBus& InRenderBus,
                                  FEditorConstants& OutConstants) const;
    void DrawGridShaderPass(ID3D11DeviceContext* InDeviceContext);

    void DrawCommand(ID3D11DeviceContext* InDeviceContext, const FRenderCommand& InCommand);
    void DrawPostProcessOutline(ID3D11DeviceContext* InDeviceContext);
    void DrawPostProcessFog(ID3D11DeviceContext* InDeviceContext, const FRenderBus& InRenderbus);
    void DrawLightHitmapOverlay(ID3D11DeviceContext* InDeviceContext, const FRenderBus& InRenderBus);

    void DrawDepthVisualizer(ID3D11DeviceContext* InDeviceContext);
    void UpdateFrameBuffer(ID3D11DeviceContext* Context, const FRenderBus& InRenderBus);
    void UpdateSceneDepthBuffer(ID3D11DeviceContext* InDeviceContext);
    void ApplyFXAA(ID3D11DeviceContext* InDeviceContext, const FFXAASettings* InFXAASettings);
    void UpdateLightingBuffer(ID3D11DeviceContext* Context, const FRenderBus& InRenderBus);
    // void UpdateLightingBufferNoScore(ID3D11DeviceContext* Context, const FRenderBus& InRenderBus);

    // 기본 패스 실행기 — SetupRenderState + DrawCommand 루프
    void ExecuteDefaultPass(ERenderPass Pass, const TArray<FRenderCommand>& Commands, const FRenderBus& Bus,
                            ID3D11DeviceContext* Context);

    // LineBatcher Flush 공통 — EditorConstants 업데이트 + EditorShader 바인딩
    void FlushLineBatcher(FLineBatcher& Batcher, ERenderPass Pass, const FRenderBus& Bus, ID3D11DeviceContext* Context);

	void ExecuteDepthPrepass(ID3D11DeviceContext* Context, const FRenderBus& InRenderBus);
    void DispatchTileLightCulling(ID3D11DeviceContext* Context, const FRenderBus& InRenderBus);

  private:
    FD3DDevice       Device;
    FRenderTargetSet CurrentRenderTargets;
    FRenderResources Resources;
    FLineBatcher     EditorLineBatcher;
    FLineBatcher     GridLineBatcher;
    FFontBatcher     FontBatcher;
    FSubUVBatcher    SubUVBatcher;

    // 패스별 커맨드 정렬이 필요한 경우 정렬된 복사본 반환, 아니면 원본 참조
    const TArray<FRenderCommand>& GetAlignedCommands(ERenderPass Pass, const TArray<FRenderCommand>& Commands);
    TArray<FRenderCommand>        SortedCommandBuffer; // 재할당 방지용 멤버 버퍼

	FPassRenderState    PassRenderStates[(uint32)ERenderPass::MAX];
	FPassBatcherBinding PassBatchers[(uint32)ERenderPass::MAX];
	FGridShaderPassState GridShaderPassState;
	ID3D11ShaderResourceView* SubUVCachedSRV = nullptr;
	bool bUsePostProcessSceneColor = false;

	FShaderManager ShaderManager;
	FFileWatcher ShaderFileWatcher;

    // Light Info 정렬용 배열 및 구조체. 배열은 매 프레임 재할당을 피하고 재활용 하기 위한 용도.
    struct FLightSortKey
    {
        float  Score;
        uint32 Index;
    };

	  TArray<FLightSortKey> PointSortScratch;
	  TArray<FLightSortKey> SpotSortScratch;
    TArray<FPointLightConstatns> PointUploadScratch;
    TArray<FSpotLightInfo>       SpotUploadScratch;
};

	////	Primitive and Gizmo Input Layout
	//D3D11_INPUT_ELEMENT_DESC PrimitiveInputLayout[2] =
	//{
	//	{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0,  static_cast<uint32>(offsetof(FVertex, Position)), D3D11_INPUT_PER_VERTEX_DATA, 0 },
	//	{ "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, static_cast<uint32>(offsetof(FVertex, Color)), D3D11_INPUT_PER_VERTEX_DATA, 0 },
	//};

	//// StaticMesh (FNormalVertex) Input Layout
	//D3D11_INPUT_ELEMENT_DESC NormalVertexInputLayout[4] =
	//{
	//	{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, static_cast<uint32>(offsetof(FNormalVertex, Position)), D3D11_INPUT_PER_VERTEX_DATA, 0 },
	//	{ "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, static_cast<uint32>(offsetof(FNormalVertex, Color)),    D3D11_INPUT_PER_VERTEX_DATA, 0 },
	//	{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, static_cast<uint32>(offsetof(FNormalVertex, Normal)),   D3D11_INPUT_PER_VERTEX_DATA, 0 },
	//	{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, static_cast<uint32>(offsetof(FNormalVertex, UVs)),      D3D11_INPUT_PER_VERTEX_DATA, 0 },
	//};

    // Depth Prepass 전용 Input Layout — Position만 선언
    //D3D11_INPUT_ELEMENT_DESC DepthPrepassInputLayout[1] = {
    //    {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
    //};
