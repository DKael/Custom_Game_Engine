#pragma once

/*
	Direct3D Device, Context, Swapchain을 관리하는 Class 입니다.
*/

#include "Render/Common/RenderTypes.h"
#include "Core/CoreTypes.h"

struct ID3D11Debug;

enum class EDepthStencilState
{
	Default,
	DepthReadOnly,
	StencilWrite,
	StencilWriteOnlyEqual,

	// --- 기즈모 전용 ---
	GizmoInside,
	GizmoOutside,

	// --- Occlusion Culling 전용 ---
	DepthPrepassWrite,   // Depth prepass: depth 쓰기, stencil 없음
};

enum class EBlendState
{
	Opaque,
	AlphaBlend,
	NoColor
};

enum class ERasterizerState
{
	SolidBackCull,
	SolidFrontCull,
	SolidNoCull,
	WireFrame,
};

struct FRenderTargetSet
{
	ID3D11RenderTargetView* SceneColorRTV = nullptr;
	ID3D11ShaderResourceView* SceneColorSRV = nullptr;
	ID3D11RenderTargetView* SelectionMaskRTV = nullptr;
	ID3D11ShaderResourceView* SelectionMaskSRV = nullptr;
	ID3D11DepthStencilView* DepthStencilView = nullptr;
	float Width = 0.0f;
	float Height = 0.0f;

	bool IsValid() const
	{
		return SceneColorRTV != nullptr && DepthStencilView != nullptr && Width > 0.0f && Height > 0.0f;
	}
};

class FD3DDevice
{
private:
	TComPtr<ID3D11Device> Device;
	TComPtr<ID3D11DeviceContext> DeviceContext;
	TComPtr<IDXGISwapChain> SwapChain;

	TComPtr<ID3D11Texture2D> FrameBuffer;
	TComPtr<ID3D11RenderTargetView> FrameBufferRTV;
	TComPtr<ID3D11Texture2D> SelectionMaskBuffer;
	TComPtr<ID3D11RenderTargetView> SelectionMaskRTV;
	TComPtr<ID3D11ShaderResourceView> SelectionMaskSRV;
	TComPtr<ID3D11Texture2D> ViewportSceneColorTexture;
	TComPtr<ID3D11RenderTargetView> ViewportSceneColorRTV;
	TComPtr<ID3D11ShaderResourceView> ViewportSceneColorSRV;
	TComPtr<ID3D11Texture2D> ViewportSelectionMaskTexture;
	TComPtr<ID3D11RenderTargetView> ViewportSelectionMaskRTV;
	TComPtr<ID3D11ShaderResourceView> ViewportSelectionMaskSRV;

	TComPtr<ID3D11RasterizerState> RasterizerStateBackCull;
	TComPtr<ID3D11RasterizerState> RasterizerStateFrontCull;
	TComPtr<ID3D11RasterizerState> RasterizerStateNoCull;
	TComPtr<ID3D11RasterizerState> RasterizerStateWireFrame;

	TComPtr<ID3D11Texture2D> DepthStencilBuffer;
	TComPtr<ID3D11DepthStencilView> DepthStencilView;
	TComPtr<ID3D11Texture2D> ViewportDepthStencilTexture;
	TComPtr<ID3D11DepthStencilView> ViewportDepthStencilView;

	// Depth Prepass (R32_TYPELESS — DSV·SRV 겸용, Occlusion Culling용)
	TComPtr<ID3D11Texture2D>          DepthPrepassTexture;
	TComPtr<ID3D11DepthStencilView>   DepthPrepassDSV;          // 쓰기용 (D32_FLOAT)
	TComPtr<ID3D11DepthStencilView>   DepthPrepassDSV_ReadOnly; // 읽기전용 (READ_ONLY_DEPTH)
	TComPtr<ID3D11ShaderResourceView> DepthPrepassSRV;           // CS 읽기 (R32_FLOAT)

	// HiZ (Hierarchical Z) — mip chain, Occlusion Culling CS용
	static constexpr uint32 MaxHiZMips = 13;
	uint32 HiZMipCount = 0;
	TComPtr<ID3D11Texture2D>            HiZTexture;
	TComPtr<ID3D11ShaderResourceView>   HiZFullSRV;                      // 전체 mip chain (Step 5 occlusion test용)
	TComPtr<ID3D11ShaderResourceView>   HiZMipSRVs[MaxHiZMips];          // CS input (단일 mip)
	TComPtr<ID3D11UnorderedAccessView>  HiZMipUAVs[MaxHiZMips];          // CS output (단일 mip)

	TComPtr<ID3D11DepthStencilState> DepthStencilStateDefault;
	TComPtr<ID3D11DepthStencilState> DepthStencilStateDepthReadOnly;
	TComPtr<ID3D11DepthStencilState> DepthStencilStateStencilWrite;
	TComPtr<ID3D11DepthStencilState> DepthStencilStateStencilMaskEqual;

	TComPtr<ID3D11DepthStencilState> DepthStencilStateGizmoInside;
	TComPtr<ID3D11DepthStencilState> DepthStencilStateGizmoOutside;
	TComPtr<ID3D11DepthStencilState> DepthStencilStateDepthPrepassWrite;

	TComPtr<ID3D11BlendState> BlendStateAlpha;
	TComPtr<ID3D11BlendState> BlendStateNoColorWrite;

	TComPtr<ID3D11Debug> DebugDevice;

	D3D11_VIEWPORT ViewportInfo = {};
	D3D11_VIEWPORT ActiveViewportInfo = {};

	const float ClearColor[4] = { 0.1f, 0.1f, 0.1f, 1.0f };

	ERasterizerState CurrentRasterizerState = ERasterizerState::SolidBackCull;
	EDepthStencilState CurrentDepthStencilState = EDepthStencilState::Default;
	EBlendState CurrentBlendState = EBlendState::Opaque;

	BOOL bTearingSupported = FALSE;
	UINT SwapChainFlags = 0;
	uint32 ViewportRenderTargetWidth = 0;
	uint32 ViewportRenderTargetHeight = 0;

public:


private:
	void CreateDeviceAndSwapChain(HWND InHWindow);
	void ReleaseDeviceAndSwapChain();

	void CreateFrameBuffer();
	void ReleaseFrameBuffer();
	void CreateViewportRenderTargets(uint32 Width, uint32 Height);
	void ReleaseViewportRenderTargets();

	void CreateRasterizerState();
	void ReleaseRasterizerState();

	void CreateDepthStencilBuffer();
	void ReleaseDepthStencilBuffer();

	void CreateDepthPrepassBuffer();
	void ReleaseDepthPrepassBuffer();

	void CreateHiZBuffer();
	void ReleaseHiZBuffer();

	void CreateBlendState();
	void ReleaseBlendState();

public:
	FD3DDevice() = default;

	void Create(HWND InHWindow);
	void Release();
	void ReportLiveObjects();

	void BeginFrame();
	void EndFrame();

	void OnResizeViewport(int width, int height);
	void EnsureViewportRenderTargets(int width, int height);

	/*
	 * 렌더링 대상 : 지정한 서브 영역으로 제한
	 * 다중 뷰포트 렌더링 시 각 뷰포트마다 호출.
	 * BeginFrame 이후, 각 뷰포트 렌더 직전에 호출해야 합니다.
	 */
	void SetSubViewport(int32 X, int32 Y, int32 Width, int32 Height);

	ID3D11Device* GetDevice() const;
	ID3D11DeviceContext* GetDeviceContext() const;
	ID3D11RenderTargetView* GetFrameBufferRTV() const { return FrameBufferRTV.Get(); }
	ID3D11RenderTargetView* GetSelectionMaskRTV() const { return SelectionMaskRTV.Get(); }
	ID3D11ShaderResourceView* GetSelectionMaskSRV() const { return SelectionMaskSRV.Get(); }
	ID3D11DepthStencilView* GetDepthStencilView() const { return DepthStencilView.Get(); }
	ID3D11ShaderResourceView* GetViewportSceneColorSRV() const { return ViewportSceneColorSRV.Get(); }
	ID3D11DepthStencilView*   GetDepthPrepassDSV()         const { return DepthPrepassDSV.Get(); }
	ID3D11DepthStencilView*   GetDepthPrepassDSV_ReadOnly() const { return DepthPrepassDSV_ReadOnly.Get(); }
	ID3D11ShaderResourceView* GetDepthPrepassSRV()          const { return DepthPrepassSRV.Get(); }
	ID3D11Texture2D*          GetDepthPrepassTexture()       const { return DepthPrepassTexture.Get(); }
	ID3D11Texture2D*          GetHiZTexture()                const { return HiZTexture.Get(); }

	uint32 GetHiZMipCount()                         const { return HiZMipCount; }
	ID3D11ShaderResourceView*  GetHiZFullSRV()          const { return HiZFullSRV.Get(); }
	ID3D11ShaderResourceView*  GetHiZMipSRV(uint32 Mip) const { return HiZMipSRVs[Mip].Get(); }
	ID3D11UnorderedAccessView* GetHiZMipUAV(uint32 Mip) const { return HiZMipUAVs[Mip].Get(); }
	float GetViewportX() const { return ActiveViewportInfo.TopLeftX; }
	float GetViewportY() const { return ActiveViewportInfo.TopLeftY; }
	float GetViewportWidth() const { return ActiveViewportInfo.Width; }
	float GetViewportHeight() const { return ActiveViewportInfo.Height; }
	float GetRenderTargetWidth() const { return ViewportInfo.Width; }
	float GetRenderTargetHeight() const { return ViewportInfo.Height; }
	FRenderTargetSet GetBackBufferRenderTargets() const;
	FRenderTargetSet GetViewportRenderTargets() const;

	void SetDepthStencilState(EDepthStencilState InState);
	void SetBlendState(EBlendState InState);
	void SetRasterizerState(ERasterizerState InState);
};

