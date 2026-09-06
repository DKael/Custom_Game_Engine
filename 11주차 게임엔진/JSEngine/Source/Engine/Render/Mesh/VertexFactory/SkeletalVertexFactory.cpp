#include "SkeletalVertexFactory.h"
#include "Render/Scene/RenderCommand.h"
#include "Render/Mesh/VertexFactory/SkeletalVertexFactoryData.h"
#include "Render/Resource/RenderResources.h"
#include <d3d11.h>

void FSkeletalVertexFactory::Bind(const FRenderCommand& Cmd, ID3D11DeviceContext* Context, FRenderResources* RenderResources)
{
	if (Cmd.VertexFactoryData)
	{
		// GPU Skinning 시에 Data 존재
        FSkeletalVertexFactoryData* VFData = static_cast<FSkeletalVertexFactoryData*>(Cmd.VertexFactoryData);

        if (VFData)
        {
            RenderResources->SkinningBuffer.Update(Context, VFData->SkinningMatrices->data(), static_cast<uint32>(VFData->SkinningMatrices->size()));
            ID3D11ShaderResourceView* SkinningSRV = RenderResources->SkinningBuffer.GetSRV();
            Context->VSSetShaderResources(16, 1, &SkinningSRV);
        }
	}

	ID3D11Buffer* VertexBuffer = Cmd.MeshBuffer->GetVertexBuffer().GetBuffer();
    UINT Stride = Cmd.MeshBuffer->GetVertexBuffer().GetStride();
    UINT Offset = 0;
	
    Context->IASetVertexBuffers(0, 1, &VertexBuffer, &Stride, &Offset);
}

