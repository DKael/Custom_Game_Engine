#include "HitMapRenderPass.h"

#include "Core/ResourceManager.h"
#include "LightCullingPass.h"
#include "Render/Scene/RenderBus.h"

#include <cstring>

namespace
{
    struct FHitMapConstants
    {
        uint32 ScreenSize[2] = {};
        uint32 TileCount[2] = {};
        uint32 bEnable25DMask = 1u;
        float Padding[3] = { 0.0f, 0.0f, 0.0f };
    };
}

bool FHitMapRenderPass::Initialize()
{
    return true;
}

bool FHitMapRenderPass::Release()
{
    HitMapShader = nullptr;
    HitMapConstantBuffer.Reset();
    return true;
}

bool FHitMapRenderPass::Begin(const FRenderPassContext* Context)
{
    if (!Context || !Context->Device || !Context->DeviceContext || !Context->RenderTargets || !Context->RenderBus)
    {
        return false;
    }

    bSkipHitMapDraw = false;
    OutSRV = PrevPassSRV ? PrevPassSRV : Context->RenderTargets->SceneColorSRV;
    OutRTV = PrevPassRTV ? PrevPassRTV : Context->RenderTargets->SceneColorRTV;

    if (!Context->RenderBus->GetShowFlags().bShowLightHitmapOverlay)
    {
        bSkipHitMapDraw = true;
        return true;
    }

    if (!EnsureShader(Context->Device) || !EnsureConstantBuffer(Context->Device))
    {
        bSkipHitMapDraw = true;
        return true;
    }

    ID3D11RenderTargetView* RTV = Context->RenderTargets->SceneColorRTV;
    Context->DeviceContext->OMSetRenderTargets(1, &RTV, nullptr);
    Context->DeviceContext->OMSetDepthStencilState(nullptr, 0);

    ID3D11BlendState* AlphaBlend = FResourceManager::Get().GetOrCreateBlendState(EBlendType::AlphaBlend);
    Context->DeviceContext->OMSetBlendState(AlphaBlend, nullptr, 0xFFFFFFFF);

    ID3D11RasterizerState* RasterizerState =
        FResourceManager::Get().GetOrCreateRasterizerState(ERasterizerType::SolidNoCull);
    Context->DeviceContext->RSSetState(RasterizerState);

    Context->DeviceContext->IASetInputLayout(nullptr);
    Context->DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    return true;
}

bool FHitMapRenderPass::DrawCommand(const FRenderPassContext* Context)
{
    if (bSkipHitMapDraw)
    {
        return true;
    }

    const FLightCullingOutputs& Outputs = FLightCullingPass::GetOutputs();
    if (Outputs.TileCountX == 0 || Outputs.TileCountY == 0)
    {
        return true;
    }

    FHitMapConstants Constants = {};
    Constants.ScreenSize[0] = static_cast<uint32>(Context->RenderTargets->Width);
    Constants.ScreenSize[1] = static_cast<uint32>(Context->RenderTargets->Height);
    Constants.TileCount[0] = Outputs.TileCountX;
    Constants.TileCount[1] = Outputs.TileCountY;

    D3D11_MAPPED_SUBRESOURCE Mapped = {};
    if (SUCCEEDED(Context->DeviceContext->Map(HitMapConstantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &Mapped)))
    {
        std::memcpy(Mapped.pData, &Constants, sizeof(Constants));
        Context->DeviceContext->Unmap(HitMapConstantBuffer.Get(), 0);
    }

    ID3D11Buffer* ConstantBuffer = HitMapConstantBuffer.Get();
    Context->DeviceContext->PSSetConstantBuffers(11, 1, &ConstantBuffer);

    ID3D11ShaderResourceView* TilePointGrid = Outputs.TilePointLightGridSRV;
    ID3D11ShaderResourceView* TileSpotGrid = Outputs.TileSpotLightGridSRV;
    Context->DeviceContext->PSSetShaderResources(19, 1, &TilePointGrid);
    Context->DeviceContext->PSSetShaderResources(21, 1, &TileSpotGrid);

    HitMapShader->Bind(Context->DeviceContext);
    Context->DeviceContext->Draw(3, 0);
    return true;
}

bool FHitMapRenderPass::End(const FRenderPassContext* Context)
{
    if (bSkipHitMapDraw || !Context || !Context->DeviceContext)
    {
        return true;
    }

    ID3D11ShaderResourceView* NullSRV = nullptr;
    Context->DeviceContext->PSSetShaderResources(19, 1, &NullSRV);
    Context->DeviceContext->PSSetShaderResources(21, 1, &NullSRV);

    ID3D11Buffer* NullCB = nullptr;
    Context->DeviceContext->PSSetConstantBuffers(11, 1, &NullCB);
    return true;
}

bool FHitMapRenderPass::EnsureShader(ID3D11Device* Device)
{
    if (HitMapShader != nullptr)
    {
        return true;
    }

    HitMapShader = FResourceManager::Get().GetShader("Shaders/HitMap.hlsl");
    return HitMapShader != nullptr;
}

bool FHitMapRenderPass::EnsureConstantBuffer(ID3D11Device* Device)
{
    if (HitMapConstantBuffer)
    {
        return true;
    }

    D3D11_BUFFER_DESC Desc = {};
    Desc.ByteWidth = sizeof(FHitMapConstants);
    Desc.Usage = D3D11_USAGE_DYNAMIC;
    Desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    return SUCCEEDED(Device->CreateBuffer(&Desc, nullptr, HitMapConstantBuffer.GetAddressOf()));
}
