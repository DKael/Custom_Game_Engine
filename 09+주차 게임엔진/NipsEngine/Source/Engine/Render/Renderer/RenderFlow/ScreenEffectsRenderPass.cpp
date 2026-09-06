#include "ScreenEffectsRenderPass.h"

#include "Core/ResourceManager.h"
#include "Render/Scene/RenderBus.h"
#include "Render/Scene/RenderCommand.h"

bool FScreenEffectsRenderPass::Initialize()
{
    return true;
}

bool FScreenEffectsRenderPass::Release()
{
    ShaderBinding.reset();
    return true;
}

bool FScreenEffectsRenderPass::Begin(const FRenderPassContext* Context)
{
    bSkipCameraEffects = false;
    const FScreenEffectSettings& Effects = Context->RenderBus->GetScreenEffectSettings();

    if (!ExistScreenCameraEffect(Effects))
    {
        OutSRV = PrevPassSRV;
        OutRTV = PrevPassRTV;
        bSkipCameraEffects = true;
        return true;
    }

    UShader* ScreenEffectsShader =  FResourceManager::Get().GetShader("Shaders/Multipass/ScreenEffectsRenderPass.hlsl");
    if (ScreenEffectsShader == nullptr)
    {
        bSkipCameraEffects = true;
        return true;
    }

    if (!ShaderBinding || ShaderBinding->GetShader() != ScreenEffectsShader)
    {
        ShaderBinding = ScreenEffectsShader->CreateBindingInstance(Context->Device);
    }

    if (!ShaderBinding)
    {
        bSkipCameraEffects = true;
        return true;
    }

    ShaderBinding->SetSRV("SceneColor", PrevPassSRV);
    ShaderBinding->SetAllSamplers(FResourceManager::Get().GetOrCreateSamplerState(ESamplerType::EST_Linear));

    ShaderBinding->SetFloat("FadeAmount", Effects.FadeAmount);
    ShaderBinding->SetVector3("FadeColor", Effects.FadeColor);
  
    ShaderBinding->SetFloat("LetterBoxAmount", Effects.LetterBoxAmount);

    ShaderBinding->SetFloat("bGammaCorrectionEnabled", Effects.bGammaCorrectionEnabled ? 1.0f : 0.0f);
    ShaderBinding->SetFloat("Gamma", Effects.Gamma);

    ShaderBinding->SetFloat("VignetteIntensity", Effects.VignetteIntensity);
    const FVector2 ViewportSize = Context->RenderBus->GetViewportSize();
    ShaderBinding->SetFloat("CurrentAspectRatio", ViewportSize.Y > 0.f ? ViewportSize.X / ViewportSize.Y : 1.f);

    ShaderBinding->SetFloat("VignetteRadius", Effects.VignetteRadius);
    ShaderBinding->SetFloat("VignetteSoftness", Effects.VignetteSoftness);
    ShaderBinding->SetVector3("VignetteColor", Effects.VignetteColor);

    ID3D11RenderTargetView* RTVs[1] = { Context->RenderTargets->SceneColorRTV };
    Context->DeviceContext->OMSetRenderTargets(1, RTVs, nullptr);

    Context->DeviceContext->IASetInputLayout(nullptr);
    Context->DeviceContext->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
    Context->DeviceContext->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
    Context->DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    OutSRV = Context->RenderTargets->SceneColorSRV;
    OutRTV = Context->RenderTargets->SceneColorRTV;

    return true;
}

bool FScreenEffectsRenderPass::DrawCommand(const FRenderPassContext* Context)
{
    if (bSkipCameraEffects || !ShaderBinding)
    {
        return true;
    }
    // Effect는 프레임당 한번이니까 Begin에서 값을 채워넣게 했음

    ShaderBinding->Bind(Context->DeviceContext);
    Context->DeviceContext->Draw(3, 0);
    return true;
}

bool FScreenEffectsRenderPass::End(const FRenderPassContext* Context)
{
    if (bSkipCameraEffects)
    {
        return false;
    }

    ID3D11ShaderResourceView* NullSRVs[] = { nullptr };
    Context->DeviceContext->PSSetShaderResources(0, 1, NullSRVs);
    return true;
}

bool FScreenEffectsRenderPass::ExistScreenCameraEffect(const FScreenEffectSettings& EffectSetting)
{
    bool bAnyEffect =
        EffectSetting.VignetteIntensity > 0.f ||
        EffectSetting.FadeAmount > 0.f ||
        EffectSetting.LetterBoxAmount > 0.f ||
        EffectSetting.bGammaCorrectionEnabled;

    return bAnyEffect;
}
