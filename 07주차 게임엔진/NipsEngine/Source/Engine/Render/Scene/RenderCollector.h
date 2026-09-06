#pragma once
#include "RenderBus.h"
#include "Render/Resource/MeshBufferManager.h"
#include "Spatial/WorldSpatialIndex.h"
#include <unordered_set>

class UWorld;
class AActor;
class UPrimitiveComponent;
class UGizmoComponent;
class ULightComponent;
struct FFrustum;

class FRenderCollector {
public:
	struct FCullingStats
	{
		int32 TotalVisiblePrimitiveCount{0};
		int32 BVHPassedPrimitiveCount{0};
		int32 FallbackPassedPrimitiveCount{0};
	};

	struct FDecalStats
	{
	    int32 TotalDecalCount{ 0 };
            int32 TotalVisibleDecalCount{0};
	};

private:
	FMeshBufferManager MeshBufferManager;
	FWorldSpatialIndex::FPrimitiveFrustumQueryScratch FrustumQueryScratch;
	TArray<UPrimitiveComponent*> VisiblePrimitiveScratch;
        FCullingStats LastCullingStats;
        FDecalStats LastDecalStats;

      public:
	void Initialize(ID3D11Device* InDevice) { MeshBufferManager.Create(InDevice); }
	void Release() { MeshBufferManager.Release(); }

	void CollectWorld(UWorld* World, const FShowFlags& ShowFlags, EViewMode ViewMode, FRenderBus& RenderBus,
	                  const FFrustum* ViewFrustum = nullptr);
	void CollectSelection(const TArray<AActor*>& SelectedActors, const FShowFlags& ShowFlags, EViewMode ViewMode, FRenderBus& RenderBus);
	void CollectGizmo(UGizmoComponent* Gizmo, const FShowFlags& ShowFlags, FRenderBus& RenderBus, bool bIsActiveOperation);
	void CollectGrid(const FGridConstants& GridConstants, FRenderBus& RenderBus);
        const FCullingStats& GetLastCullingStats() const { return LastCullingStats; }
        const FDecalStats&   GetLastDecalStats() const { return LastDecalStats; }

	void CollectFog(UWorld* World, FRenderBus& RenderBus);
    void CollectLight(ULightComponent* LightComponent, const FShowFlags& ShowFlags, FRenderBus& RenderBus);

private:
	void ResetStats();
	void CollectWorldWithFrustum(UWorld* World, const FFrustum& ViewFrustum, const FShowFlags& ShowFlags, EViewMode ViewMode,
	                             FRenderBus& RenderBus);
	void CollectFromActor(AActor* Actor, const FShowFlags& ShowFlags, EViewMode ViewMode, FRenderBus& RenderBus);
	bool CollectFromSelectedActor(AActor* Actor, const FShowFlags& ShowFlags, EViewMode ViewMode, FRenderBus& RenderBus);
	void CollectFromComponent(UPrimitiveComponent* Primitive, const FShowFlags& ShowFlags, EViewMode ViewMode, FRenderBus& RenderBus);
	void CollectBVHInternalNodeAABBs(UPrimitiveComponent* PrimitiveComponent, const FShowFlags& ShowFlags, FRenderBus& RenderBus,
	                                 std::unordered_set<int32>& SeenNodeIndices);
	void CollectAABBCommand(const FAABB& Box, const FColor& Color, FRenderBus& RenderBus);
	void CollectAABBCommand(UPrimitiveComponent* PrimitiveComponent, const FShowFlags& ShowFlags, FRenderBus& RenderBus);
};
