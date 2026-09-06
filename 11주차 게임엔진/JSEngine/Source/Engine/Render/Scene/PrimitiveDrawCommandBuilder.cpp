#include "PrimitiveDrawCommandBuilder.h"

#include "Component/BillboardComponent.h"
#include "Component/FireballComponent.h"
#include "Component/HeightFogComponent.h"
#include "Component/ProceduralMeshComponent.h"
#include "Component/SkeletalMeshComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Component/SubUVComponent.h"
#include "Component/TextRenderComponent.h"
#include "Engine/Asset/StaticMesh.h"
#include "Render/Resource/MeshBufferManager.h"
#include "Render/Scene/RenderBus.h"
#include "Render/Proxy/PrimitiveRenderProxy.h"

#include <cmath>

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

    int32 SelectLODLevel(const FVector& CameraPos, const FAABB& Bounds, const FMatrix& ProjMatrix, int32 ValidLODCount)
    {
        bool IsOrthoGraphic = (std::abs(ProjMatrix.M[3][3] - 1.0f) < 1e-4f);
        if (ValidLODCount <= 1 || IsOrthoGraphic) return 0;

        const FVector Center = (Bounds.Min + Bounds.Max) * 0.5f;
        const FVector Extent = (Bounds.Max - Bounds.Min) * 0.5f;
        const float SphereRadius = std::sqrt(Extent.X * Extent.X + Extent.Y * Extent.Y + Extent.Z * Extent.Z);

        const FVector Diff = Center - CameraPos;
        const float Dist = std::sqrt(Diff.X * Diff.X + Diff.Y * Diff.Y + Diff.Z * Diff.Z);

        if (Dist <= 1e-4f) return 0;

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
}

bool FPrimitiveDrawCommandBuilder::CollectPrimitive(UPrimitiveComponent* Primitive, const FShowFlags& ShowFlags,
                                                    EViewMode ViewMode, FRenderBus& RenderBus,
                                                    FMeshBufferManager& MeshBufferManager) const
{
    if (Primitive == nullptr || !Primitive->IsVisible()) return true;

	FPrimitiveRenderProxy* Proxy = Primitive->GetOrCreateRenderProxy();
	if (Proxy)
	{
        FRenderProxyContext Context{ ShowFlags, ViewMode, MeshBufferManager };
        Proxy->CollectRenderCommands(Context, RenderBus);
	}
    return true; 
}
