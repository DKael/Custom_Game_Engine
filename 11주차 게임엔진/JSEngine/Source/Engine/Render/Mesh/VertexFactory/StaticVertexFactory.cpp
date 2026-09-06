#include "StaticVertexFactory.h"
#include "Render/Scene/RenderCommand.h"
#include <d3d11.h>

void FStaticVertexFactory::Bind(const FRenderCommand& Cmd, ID3D11DeviceContext* Context, FRenderResources* RenderResources)
{
    // 스태틱 메시(StaticMesh) 에셋에 종속된 로직이 아닙니다.
    // 스킨닝 연산이 없고 정적인 정점 레이아웃을 사용하는 모든 객체(예: Billboard, UI Quad 등)는
    // 이 Factory를 공유하여 파이프라인 상태 변화(State Change)를 최소화할 수 있습니다.
    ID3D11Buffer* VertexBuffer = Cmd.MeshBuffer->GetVertexBuffer().GetBuffer();
    UINT Stride = Cmd.MeshBuffer->GetVertexBuffer().GetStride();
    UINT Offset = 0;

    Context->IASetVertexBuffers(0, 1, &VertexBuffer, &Stride, &Offset);
}
