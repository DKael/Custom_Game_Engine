#include "DepthPrePass.h"
#include "Render/Scene/RenderBus.h"
#include "Render/Resource/RenderResources.h"
#include "Render/Resource/Material.h"
#include "Render/Resource/ShaderPaths.h"
#include "Render/Resource/VertexFactoryTypes.h"
#include "Render/Resource/ShaderHelper.h"
#include "Core/ResourceManager.h"
#include "Render/Mesh/VertexFactory/SkeletalVertexFactoryData.h"

namespace
{
	FShaderProgram* GetDepthPrepassProgram(EVertexFactoryType VertexFactoryType, bool bUseGPUSkinning, const FVertexLayoutDesc* PositionLayout)
	{
		const FVertexFactoryDesc& VertexFactory = FVertexFactoryRegistry::Get(VertexFactoryType);

		FShaderStageKey VSKey;
		VSKey.FilePath = VertexFactory.DepthPassVSPath.empty() ? FShaderPaths::DepthPrepass : VertexFactory.DepthPassVSPath;
		VSKey.EntryPoint = VertexFactory.DepthPassVSEntry;

		FShaderStageKey PSKey;
		PSKey.FilePath = FShaderPaths::DepthPrepass;
		PSKey.EntryPoint = "DepthPrepassPS";

		// Build macros so we can compile a skeletal variant of the depth VS that
		// performs GPU skinning when needed. Only enable SKELETAL_MESH when the
		// draw command intends to use GPU skinning.
		TArray<D3D_SHADER_MACRO> Macros;
		if (VertexFactoryType == EVertexFactoryType::SkeletalMesh && bUseGPUSkinning)
		{
			Macros.push_back({ "SKELETAL_MESH", "1" });
		}
		Macros.push_back({ nullptr, nullptr });

		const FVertexLayoutDesc* LayoutToUse = PositionLayout ? PositionLayout : &VertexFactory.PositionOnlyLayout;

		return FResourceManager::Get().GetOrCreateShaderProgram(
			VSKey,
			PSKey,
			Macros.data(),
			Macros.data(),
			LayoutToUse);
	}
}

bool FDepthPrePass::Initialize()
{
	return true;
}

bool FDepthPrePass::Release()
{
	return true;
}

bool FDepthPrePass::Begin(const FRenderPassContext* Context)
{
	const FRenderTargetSet* RenderTargets = Context->RenderTargets;
	ID3D11DepthStencilView* DSV = RenderTargets->DepthStencilView;
	Context->DeviceContext->ClearDepthStencilView(DSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
	Context->DeviceContext->OMSetRenderTargets(0, nullptr, DSV);
	OutSRV = RenderTargets->SceneDepthSRV;
	OutRTV = nullptr;

	ID3D11DepthStencilState* DepthState = FResourceManager::Get().GetOrCreateDepthStencilState(EDepthStencilType::Default);
	Context->DeviceContext->OMSetDepthStencilState(DepthState, 0);

	Context->DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	return true;
}

bool FDepthPrePass::DrawCommand(const FRenderPassContext* Context)
{
	const FRenderBus* RenderBus = Context->RenderBus;
	const TArray<FRenderCommand>& OpaqueCmds = RenderBus->GetCommands(ERenderPass::Opaque);

	for (const auto& Cmd : OpaqueCmds)
	{
		if (Cmd.Type == ERenderCommandType::PostProcessOutline)
		{
			continue;
		}

		if (Cmd.MeshBuffer == nullptr || !Cmd.MeshBuffer->IsValid())
		{
			continue;
		}

		Context->RenderResources->PerObjectConstantBuffer.Update(Context->DeviceContext, &Cmd.PerObjectConstants, sizeof(FPerObjectConstants));
		ID3D11Buffer* CB1 = Context->RenderResources->PerObjectConstantBuffer.GetBuffer();
		Context->DeviceContext->VSSetConstantBuffers(1, 1, &CB1);

		// Select input layout & whether to compile GPU-skinning variant per-command.
		const FVertexFactoryDesc& VFDesc = FVertexFactoryRegistry::Get(Cmd.VertexFactoryType);
		const FVertexLayoutDesc* PositionLayout = nullptr;

		bool bUseGPUSkinning = false;
		if (Cmd.VertexFactoryType == EVertexFactoryType::SkeletalMesh)
		{
			if (Cmd.VertexFactoryData)
            {
                FSkeletalVertexFactoryData* VFData = static_cast<FSkeletalVertexFactoryData*>(Cmd.VertexFactoryData);
                bUseGPUSkinning = (VFData->SkinningMatrices != nullptr);
			}

			if (bUseGPUSkinning)
			{
				PositionLayout = &FVertexFactoryRegistry::GetSkeletalPositionOnlyLayout();
			}
			else
			{
				// For CPU skinning, ensure depth prepass reads the same full skeletal
				// vertex layout that the mesh buffer provides so POSITION is sourced
				// correctly from the dynamic skeletal vertex buffer.
				const FVertexFactoryDesc& VF = FVertexFactoryRegistry::Get(Cmd.VertexFactoryType);
				PositionLayout = &VF.VertexLayout;
			}
		}

		FShaderProgram* Program = GetDepthPrepassProgram(Cmd.VertexFactoryType, bUseGPUSkinning, PositionLayout);
		if (!Program)
		{
			continue;
		}

		Program->Bind(Context->DeviceContext);
        Context->DeviceContext->PSSetShader(nullptr, nullptr, 0);
        IVertexFactory* VertexFactory = FVertexFactoryRegistry::GetVertexFactory(Cmd.VertexFactoryType);
        VertexFactory->Bind(Cmd, Context->DeviceContext, Context->RenderResources);
        CheckOverrideViewMode(Context);  

		ID3D11Buffer* IndexBuffer = Cmd.MeshBuffer->GetIndexBuffer().GetBuffer();
		if (IndexBuffer != nullptr)
		{
			Context->DeviceContext->IASetIndexBuffer(IndexBuffer, DXGI_FORMAT_R32_UINT, 0);
			Context->DeviceContext->DrawIndexed(Cmd.SectionIndexCount, Cmd.SectionIndexStart, 0);
		}
		else
		{
            Context->DeviceContext->Draw(Cmd.MeshBuffer->GetVertexBuffer().GetVertexCount(), 0);
		}
	}

	return true;
}

bool FDepthPrePass::End(const FRenderPassContext* Context)
{
	Context->DeviceContext->OMSetRenderTargets(0, nullptr, nullptr);
	return true;
}
