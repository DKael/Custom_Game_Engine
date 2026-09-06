#pragma once
#include "RenderProxy.h"

class FPrimitiveRenderProxy : public IRenderProxy
{
public:
    virtual ~FPrimitiveRenderProxy() = default;
    void CollectRenderCommands(const FRenderProxyContext& Context, FRenderBus& RenderBus) override;

    virtual class FVertexFactoryData* GetVertexFactoryData() const { return nullptr; }
};
