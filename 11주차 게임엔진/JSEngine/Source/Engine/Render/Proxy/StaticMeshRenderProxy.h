#pragma once
#include "PrimitiveRenderProxy.h"
#include "Render/Mesh/VertexFactory/StaticVertexFactoryData.h"

class UStaticMeshComponent;
class FStaticVertexFactoryData;

class FStaticMeshRenderProxy : public FPrimitiveRenderProxy
{
public:
    void CollectRenderCommands(const FRenderProxyContext& Context, FRenderBus& RenderBus) override;
    void Release() override;

    FVertexFactoryData* GetVertexFactoryData() const override { return StaticVFData; }

    UStaticMeshComponent* StaticMeshComp = nullptr;
    FStaticVertexFactoryData* StaticVFData = nullptr;
};
