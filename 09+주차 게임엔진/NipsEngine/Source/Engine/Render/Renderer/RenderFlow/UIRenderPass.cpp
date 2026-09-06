#include "UIRenderPass.h"
#include "RenderPassContext.h"
#include "Engine/UI/UIManager.h"
#include "Render/Scene/RenderBus.h"

bool FUIRenderPass::Initialize()
{
    return true;
}

bool FUIRenderPass::Release()
{
    return true;
}

bool FUIRenderPass::Begin(const FRenderPassContext* Context)
{
    // 이전 패스의 RTV를 그대로 사용 — UI는 씬 위에 덧그림
    ID3D11RenderTargetView* RTV = PrevPassRTV
        ? PrevPassRTV
        : Context->RenderTargets->SceneColorRTV;

    // UI는 깊이 판정 없이 항상 위에 그려지므로 DSV를 nullptr로 바인딩
    Context->DeviceContext->OMSetRenderTargets(1, &RTV, nullptr);

    OutSRV = PrevPassSRV;
    OutRTV = PrevPassRTV;
    return true;
}

bool FUIRenderPass::DrawCommand(const FRenderPassContext* Context)
{
    if (!Context->UIManager)
        return true;

    Context->UIManager->Flush(Context->DeviceContext, Context->RenderBus);
    return true;
}

bool FUIRenderPass::End(const FRenderPassContext* Context)
{
    return true;
}
