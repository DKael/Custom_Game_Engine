#pragma once

struct FRenderCommand;
struct ID3D11DeviceContext;
struct FRenderResources;

class IVertexFactory
{
public:
    // VB, IB 세팅
	// VS 에서 쓰는 Buffer 세팅 (VSSetConstantBuffers, ...)
    virtual void Bind(const FRenderCommand& Cmd, ID3D11DeviceContext* Context, FRenderResources* RenderResources) = 0;
};
