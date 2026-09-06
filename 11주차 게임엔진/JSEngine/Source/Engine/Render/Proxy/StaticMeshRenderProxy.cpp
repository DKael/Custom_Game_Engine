#include "StaticMeshRenderProxy.h"
#include "Component/StaticMeshComponent.h"
#include "Render/Resource/Buffer.h" 
#include "Asset/StaticMesh.h"

namespace
{
	int32 SelectLODLevel(const FVector& CameraPos, const FAABB& Bounds, const FMatrix& ProjMatrix, int32 ValidLODCount)
	{
		bool IsOrthoGraphic = (std::abs(ProjMatrix.M[3][3] - 1.0f) < 1e-4f);
		if (ValidLODCount <= 1 || IsOrthoGraphic)
			return 0;

		const FVector Center = (Bounds.Min + Bounds.Max) * 0.5f;
		const FVector Extent = (Bounds.Max - Bounds.Min) * 0.5f;
		const float SphereRadius = std::sqrt(Extent.X * Extent.X + Extent.Y * Extent.Y + Extent.Z * Extent.Z);

		const FVector Diff = Center - CameraPos;
		const float Dist = std::sqrt(Diff.X * Diff.X + Diff.Y * Diff.Y + Diff.Z * Diff.Z);

		if (Dist <= 1e-4f)
			return 0;

		const float ProjectedRadius = (SphereRadius / Dist) * ProjMatrix.M[2][1];
		const float ScreenCoverage = ProjectedRadius;

		static constexpr float Thresholds[] = { 0.15f, 0.08f, 0.05f, 0.02f };
		static constexpr int32 ThresholdCount = static_cast<int32>(sizeof(Thresholds) / sizeof(Thresholds[0]));

		const int32 MaxLOD = ValidLODCount - 1;
		for (int32 LOD = 0; LOD < MaxLOD; ++LOD)
		{
			float Threshold = (LOD < ThresholdCount) ? Thresholds[LOD] : 0.0f;
			if (ScreenCoverage >= Threshold)
				return LOD;
		}

		return MaxLOD;
	}
} // namespace

void FStaticMeshRenderProxy::CollectRenderCommands(const FRenderProxyContext& Context, FRenderBus& RenderBus)
{
    if (!Context.ShowFlags.bPrimitives)
        return;

    FVector CameraPos = RenderBus.GetCameraPosition();
    FMatrix ProjMatrix = RenderBus.GetProj();

    int32 SelectedLOD = 0;
    if (Context.ShowFlags.bEnableLOD)
    {
        SelectedLOD = SelectLODLevel(CameraPos, StaticMeshComp->GetWorldAABB(), ProjMatrix, StaticMeshComp->GetStaticMesh()->GetValidLODCount());
    }

    FMeshBuffer* MeshBuffer = Context.MeshBufferManager.GetStaticMeshBuffer(StaticMeshComp->GetStaticMesh(), SelectedLOD);
    if (!MeshBuffer)
        return;

    const FStaticMesh* MeshData = StaticMeshComp->GetStaticMesh()->GetMeshData(SelectedLOD);
    const TArray<FStaticMeshSection>& Sections = MeshData->Sections;

    for (int32 SectionIdx = 0; SectionIdx < static_cast<int32>(Sections.size()); ++SectionIdx)
    {
        const FStaticMeshSection& Section = Sections[SectionIdx];
        UMaterialInterface* Material = Cast<UMaterialInterface>(StaticMeshComp->GetMaterial(SectionIdx));

        FRenderCommand Cmd = {};
        Cmd.PerObjectConstants = FPerObjectConstants{ StaticMeshComp->GetWorldMatrix(), FColor::White().ToVector4() };
        Cmd.SourcePrimitive = StaticMeshComp;
        Cmd.Type = ERenderCommandType::StaticMesh;
        Cmd.VertexFactoryType = EVertexFactoryType::StaticMesh;
        Cmd.MeshBuffer = MeshBuffer;
        Cmd.VertexFactoryData = StaticVFData;

        Cmd.SectionIndexStart = Section.StartIndex;
        Cmd.SectionIndexCount = Section.IndexCount;
        Cmd.Material = Material;

        Cmd.WorldAABB = StaticMeshComp->GetWorldAABB();

        RenderBus.AddCommand(ERenderPass::Opaque, Cmd);
    }
}

void FStaticMeshRenderProxy::Release()
{
	if (StaticVFData)
	{
        delete StaticVFData;
	}
}
