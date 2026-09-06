#include "BillboardRenderProxy.h"
#include "Component/BillboardComponent.h"

namespace
{
	FMatrix MakeViewBillboardMatrix(const UPrimitiveComponent* Primitive, const FRenderBus& RenderBus)
	{
		const FMatrix WorldMatrix = Primitive->GetWorldMatrix();
		const UBillboardComponent* Billboard = static_cast<const UBillboardComponent*>(Primitive);
		return UBillboardComponent::MakeBillboardWorldMatrix(
			WorldMatrix.GetOrigin(),
			Billboard->GetBillboardWorldScale(),
			RenderBus.GetCameraForward(),
			RenderBus.GetCameraRight(),
			RenderBus.GetCameraUp());
	}
}

void FBillboardRenderProxy::CollectRenderCommands(const FRenderProxyContext& Context, FRenderBus& RenderBus)
{
    UTexture* Texture = BillboardComp->GetTexture();

    FRenderCommand Cmd = {};
    Cmd.Type = ERenderCommandType::Billboard;
    Cmd.VertexFactoryType = EVertexFactoryType::Billboard;
    Cmd.SourcePrimitive = BillboardComp;
    Cmd.MeshBuffer = &Context.MeshBufferManager.GetMeshBuffer(EPrimitiveType::EPT_Billboard);
    Cmd.SectionIndexStart = 0;
    Cmd.SectionIndexCount = Cmd.MeshBuffer->GetIndexBuffer().GetIndexCount();
    Cmd.PerObjectConstants = FPerObjectConstants{
        MakeViewBillboardMatrix(BillboardComp, RenderBus),
        FColor::White().ToVector4()
    };
    Cmd.Constants.Billboard.Texture = Texture;
    Cmd.Constants.Billboard.Width = BillboardComp->GetWidth();
    Cmd.Constants.Billboard.Height = BillboardComp->GetHeight();
    Cmd.Constants.Billboard.Color = BillboardComp->GetColor();

    RenderBus.AddCommand(ERenderPass::SubUV, Cmd);
}
