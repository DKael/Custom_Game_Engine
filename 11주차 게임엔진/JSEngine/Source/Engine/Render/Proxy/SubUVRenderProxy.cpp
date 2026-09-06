#include "SubUVRenderProxy.h"
#include "Component/SubUVComponent.h"

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

void FSubUVRenderProxy::CollectRenderCommands(const FRenderProxyContext& Context, FRenderBus& RenderBus)
{
    const FParticleResource* Particle = SubUVComp->GetParticle();
    if (!Particle || !Particle->IsLoaded())
        return;

    FRenderCommand Cmd = {};
    Cmd.PerObjectConstants = FPerObjectConstants{
        MakeViewBillboardMatrix(SubUVComp, RenderBus),
        FColor::White().ToVector4()
    };
    Cmd.SourcePrimitive = SubUVComp;
    Cmd.Type = ERenderCommandType::SubUV;
    Cmd.VertexFactoryType = EVertexFactoryType::SubUV;
    Cmd.MeshBuffer = &Context.MeshBufferManager.GetMeshBuffer(EPrimitiveType::EPT_SubUV);
    Cmd.SectionIndexStart = 0;
    Cmd.SectionIndexCount = Cmd.MeshBuffer->GetIndexBuffer().GetIndexCount();
    Cmd.Constants.SubUV.Particle = Particle;
    Cmd.Constants.SubUV.FrameIndex = SubUVComp->GetFrameIndex();
    Cmd.Constants.SubUV.Width = SubUVComp->GetWidth();
    Cmd.Constants.SubUV.Height = SubUVComp->GetHeight();

    RenderBus.AddCommand(ERenderPass::SubUV, Cmd);
}
