#include "SkeletalMeshRenderProxy.h"
#include "Component/SkeletalMeshComponent.h"
#include "Core/Logging/Stats.h"

void FSkeletalMeshRenderProxy::CollectRenderCommands(const FRenderProxyContext& Context, FRenderBus& RenderBus)
{
    if (!Context.ShowFlags.bPrimitives || !Context.ShowFlags.bSkeletalMesh)
        return;

    USkeletalMesh* SkeletalMesh = SkeletalMeshComp->GetSkeletalMesh();

    if (!SkeletalMesh || !SkeletalMesh->HasValidMeshData())
        return;

    SkeletalMeshComp->EnsureSkinningUpdated();
    const bool bNeedsUpload = SkeletalMeshComp->ConsumeRenderStateDirty();

    const TArray<FSkeletalMeshVertex>* SkeletalVertices;

    if (SkeletalMeshComp->IsGPUSkinningEnabled())
    {
        SkeletalVertices = &SkeletalMesh->GetVertices();
    }
    else
    {
        SkeletalVertices = &SkeletalMeshComp->GetSkinnedVertices();
    }

    const TArray<uint32>& Indices = SkeletalMesh->GetIndices(); // 이건 immutable이라 걍 asset에서 들고와도 댐

    FMeshBuffer* MeshBuffer = Context.MeshBufferManager.GetSkeletalMeshBuffer(
        SkeletalMeshComp->GetUUID(),
        SkeletalMesh,
        *SkeletalVertices,
        Indices,
        bNeedsUpload);

    if (!MeshBuffer)
        return;

    const TArray<FStaticMeshSection>& Sections = SkeletalMesh->GetSections();
    if (Sections.empty()) // fallback
    {
        STAT_COUNTER_SKELMESH("Drawn Sections", 1);
        FRenderCommand Cmd = {};
        Cmd.PerObjectConstants = FPerObjectConstants{ SkeletalMeshComp->GetWorldMatrix(), FColor::White().ToVector4() };
        Cmd.SourcePrimitive = SkeletalMeshComp;
        Cmd.Type = ERenderCommandType::SkeletalMesh;
        Cmd.VertexFactoryType = EVertexFactoryType::SkeletalMesh;
        Cmd.MeshBuffer = MeshBuffer;
        Cmd.SectionIndexStart = 0;
        Cmd.SectionIndexCount = MeshBuffer->GetIndexBuffer().GetIndexCount();
        Cmd.Material = SkeletalMeshComp->GetMaterial(0);
        Cmd.WorldAABB = SkeletalMeshComp->GetWorldAABB();

		if (SkeletalMeshComp->IsGPUSkinningEnabled())
        {
            SkelVFData->SkinningMatrices = &SkeletalMeshComp->GetSkinningMatrices();
            Cmd.VertexFactoryData = SkelVFData;
		}

        RenderBus.AddCommand(ERenderPass::Opaque, Cmd);
        return;
    }

    for (int32 SectionIdx = 0; SectionIdx < static_cast<int32>(Sections.size()); ++SectionIdx)
    {
        const FStaticMeshSection& Section = Sections[SectionIdx];
        if (Section.IndexCount == 0)
        {
            continue;
        }

        STAT_COUNTER_SKELMESH("Drawn Sections", 1);
        UMaterialInterface* Material = Cast<UMaterialInterface>(SkeletalMeshComp->GetMaterial(SectionIdx));

        FRenderCommand Cmd = {};
        Cmd.PerObjectConstants = FPerObjectConstants{ SkeletalMeshComp->GetWorldMatrix(), FColor::White().ToVector4() };
        Cmd.SourcePrimitive = SkeletalMeshComp;
        Cmd.Type = ERenderCommandType::SkeletalMesh;
        Cmd.VertexFactoryType = EVertexFactoryType::SkeletalMesh;
        Cmd.MeshBuffer = MeshBuffer;

        Cmd.SectionIndexStart = Section.StartIndex;
        Cmd.SectionIndexCount = Section.IndexCount;
        Cmd.Material = Material;

        Cmd.WorldAABB = SkeletalMeshComp->GetWorldAABB();

		if (SkeletalMeshComp->IsGPUSkinningEnabled())
		{
            SkelVFData->SkinningMatrices = &SkeletalMeshComp->GetSkinningMatrices();
            Cmd.VertexFactoryData = SkelVFData;
		}

        RenderBus.AddCommand(ERenderPass::Opaque, Cmd);
    }
}

void FSkeletalMeshRenderProxy::Release()
{
	if (SkelVFData)
	{
        delete SkelVFData;
	}
}
