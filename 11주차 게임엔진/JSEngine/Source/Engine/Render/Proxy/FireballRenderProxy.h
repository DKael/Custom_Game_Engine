#pragma once
#include "PrimitiveRenderProxy.h"

class UFireballComponent;

class FFireballRenderProxy : public FPrimitiveRenderProxy
{
public:
    void CollectRenderCommands(const FRenderProxyContext& Context, FRenderBus& RenderBus) override;

	UFireballComponent* FireballComp = nullptr;
};
