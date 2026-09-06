#pragma once
#include "PrimitiveRenderProxy.h"

class USubUVComponent;

class FSubUVRenderProxy : public FPrimitiveRenderProxy
{
public:
    void CollectRenderCommands(const FRenderProxyContext& Context, FRenderBus& RenderBus) override;
	
	USubUVComponent* SubUVComp = nullptr;
};
