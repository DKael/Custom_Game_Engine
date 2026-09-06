#pragma once
#include "VertexFactory.h"

class FSkeletalVertexFactory : public IVertexFactory
{
public:
    void Bind(const FRenderCommand& Cmd, ID3D11DeviceContext* Context, FRenderResources* RenderResources) override;
};
