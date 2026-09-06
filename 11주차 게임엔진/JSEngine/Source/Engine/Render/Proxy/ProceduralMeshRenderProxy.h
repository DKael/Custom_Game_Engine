#pragma once
#include "PrimitiveRenderProxy.h"

class UProceduralMeshComponent;

class FProceduralMeshRenderProxy : public FPrimitiveRenderProxy
{
public:
    void CollectRenderCommands(const FRenderProxyContext& Context, FRenderBus& RenderBus) override;

	UProceduralMeshComponent* ProcMeshComp = nullptr;
};
