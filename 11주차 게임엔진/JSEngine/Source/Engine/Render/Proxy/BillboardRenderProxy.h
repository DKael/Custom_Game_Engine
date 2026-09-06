#pragma once
#include "PrimitiveRenderProxy.h"

class UBillboardComponent;

class FBillboardRenderProxy : public FPrimitiveRenderProxy
{
public:
    void CollectRenderCommands(const FRenderProxyContext& Context, FRenderBus& RenderBus) override;

	UBillboardComponent* BillboardComp = nullptr;
};
