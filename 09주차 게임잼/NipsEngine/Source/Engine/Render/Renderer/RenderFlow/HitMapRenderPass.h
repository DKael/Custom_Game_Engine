#pragma once

#include "Render/Common/ComPtr.h"
#include "RenderPass.h"

class UShader;

class FHitMapRenderPass : public FBaseRenderPass
{
public:
    bool Initialize() override;
    bool Release() override;

private:
    bool Begin(const FRenderPassContext* Context) override;
    bool DrawCommand(const FRenderPassContext* Context) override;
    bool End(const FRenderPassContext* Context) override;

    bool EnsureShader(ID3D11Device* Device);
    bool EnsureConstantBuffer(ID3D11Device* Device);

private:
    UShader* HitMapShader = nullptr;
    TComPtr<ID3D11Buffer> HitMapConstantBuffer;
    bool bSkipHitMapDraw = false;
};
