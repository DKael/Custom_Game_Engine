#pragma once

#include "RenderPass.h"
#include <memory>

class FShaderBindingInstance;
struct FScreenEffectSettings;

class FScreenEffectsRenderPass : public FBaseRenderPass
{
public:
    bool Initialize() override;
    bool Release() override;

private:
    bool Begin(const FRenderPassContext* Context) override;
    bool DrawCommand(const FRenderPassContext* Context) override;
    bool End(const FRenderPassContext* Context) override;

private:
   bool ExistScreenCameraEffect(const FScreenEffectSettings& EffectSetting);
private:
    bool bSkipCameraEffects = false;
    std::shared_ptr<FShaderBindingInstance> ShaderBinding;
};
