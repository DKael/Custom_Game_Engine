#pragma once
#include "PrimitiveRenderProxy.h"
#include "Render/Mesh/VertexFactory/SkeletalVertexFactoryData.h"

class USkeletalMeshComponent;
class FSkeletalVertexFactoryData;

class FSkeletalMeshRenderProxy : public FPrimitiveRenderProxy
{
public:
    void CollectRenderCommands(const FRenderProxyContext& Context, FRenderBus& RenderBus) override;
    void Release() override;

    FVertexFactoryData* GetVertexFactoryData() const override { return SkelVFData; }

	USkeletalMeshComponent* SkeletalMeshComp = nullptr;
    FSkeletalVertexFactoryData* SkelVFData = nullptr;
};
