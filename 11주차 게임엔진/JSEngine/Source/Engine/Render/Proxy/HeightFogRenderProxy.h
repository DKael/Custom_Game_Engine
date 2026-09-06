#pragma once
#include "PrimitiveRenderProxy.h"

class UHeightFogComponent;

class FHeightFogRenderProxy : public FPrimitiveRenderProxy
{
public:
    void CollectRenderCommands(const FRenderProxyContext& Context, FRenderBus& RenderBus) override;

	UHeightFogComponent* HeightFogComp = nullptr;
};
