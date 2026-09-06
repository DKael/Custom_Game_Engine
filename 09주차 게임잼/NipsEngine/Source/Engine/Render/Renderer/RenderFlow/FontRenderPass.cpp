#include "FontRenderPass.h"
#include "Core/ResourceManager.h"
#include "Render/FontBatcher.h"
#include "Render/Scene/RenderBus.h"

bool FFontRenderPass::Initialize()
{
    return true;
}

bool FFontRenderPass::Release()
{
    return true;
}

bool FFontRenderPass::Begin(const FRenderPassContext* Context)
{
    ID3D11RenderTargetView* RTVs[3] = {
        PrevPassRTV ? PrevPassRTV : Context->RenderTargets->SceneColorRTV,
        Context->RenderTargets->SceneNormalRTV,
        Context->RenderTargets->SceneWorldPosRTV
    };
    ID3D11DepthStencilView* DSV = Context->RenderTargets->DepthStencilView;
    Context->DeviceContext->OMSetRenderTargets(ARRAYSIZE(RTVs), RTVs, DSV);

    OutSRV = PrevPassSRV;
    OutRTV = PrevPassRTV;
    return true;
}

bool FFontRenderPass::DrawCommand(const FRenderPassContext* Context)
{
    if (Context->FontBatcher == nullptr)
    {
        return true;
    }

    const FFontResource* FontRes = FResourceManager::Get().FindFont(FName("Default"));
    const bool bWireframe = (Context->RenderBus != nullptr) && (Context->RenderBus->GetViewMode() == EViewMode::Wireframe);
    Context->FontBatcher->Flush(Context->DeviceContext, Context->RenderBus, FontRes, bWireframe);
    return true;
}

bool FFontRenderPass::End(const FRenderPassContext* Context)
{
    return true;
}
