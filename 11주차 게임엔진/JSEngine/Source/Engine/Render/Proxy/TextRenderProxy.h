#pragma once
#include "PrimitiveRenderProxy.h"

class UTextRenderComponent;

class FTextRenderProxy : public FPrimitiveRenderProxy
{
public:
    void CollectRenderCommands(const FRenderProxyContext& Context, FRenderBus& RenderBus) override;

	UTextRenderComponent* TextComp = nullptr;
};
