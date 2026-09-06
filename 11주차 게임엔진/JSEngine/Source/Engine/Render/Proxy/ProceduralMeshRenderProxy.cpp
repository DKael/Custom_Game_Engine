#include "ProceduralMeshRenderProxy.h"
#include "Component/ProceduralMeshComponent.h"

void FProceduralMeshRenderProxy::CollectRenderCommands(const FRenderProxyContext& Context, FRenderBus& RenderBus)
{
    if (!Context.ShowFlags.bPrimitives)
        return;

    const TArray<UProceduralMeshComponent::FMeshSection>& Sections = ProcMeshComp->GetSections();

    if (!ProcMeshComp || Sections.empty())
        return;

    for (int32 SectionIdx = 0; SectionIdx < static_cast<int32>(Sections.size()); ++SectionIdx)
    {
        const UProceduralMeshComponent::FMeshSection& Section = Sections[SectionIdx];
        FMeshBuffer* MeshBuffer = nullptr;
        MeshBuffer = Context.MeshBufferManager.GetProcMeshBuffer(ProcMeshComp->GetUUID(), Section.Vertices, Section.Indices);

        if (!MeshBuffer)
            break;

        UMaterialInterface* Material = Cast<UMaterialInterface>(ProcMeshComp->GetMaterial(SectionIdx));

        FRenderCommand Cmd = {};
        Cmd.PerObjectConstants = FPerObjectConstants{ ProcMeshComp->GetWorldMatrix(), FColor::White().ToVector4() };
        Cmd.SourcePrimitive = ProcMeshComp;
        Cmd.Type = ERenderCommandType::StaticMesh;
        Cmd.VertexFactoryType = EVertexFactoryType::ProceduralMesh;
        Cmd.MeshBuffer = MeshBuffer;

        Cmd.SectionIndexStart = 0;
        Cmd.SectionIndexCount = static_cast<uint32>(Section.Indices.size());
        Cmd.Material = Material;

        Cmd.WorldAABB = ProcMeshComp->GetWorldAABB();

        RenderBus.AddCommand(ERenderPass::Opaque, Cmd);
    }
}
