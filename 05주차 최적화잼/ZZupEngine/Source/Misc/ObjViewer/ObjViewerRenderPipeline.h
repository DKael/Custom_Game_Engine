#pragma once
#include "Misc/ObjViewer/ObjViewerEngine.h"
#include "Misc/ObjViewer/Render/RenderDataManager.h"
#include "Engine/Core/CollisionTypes.h"
#include "Engine/Render/Renderer/IRenderPipeline.h"
#include "Render/Scene/RenderCollector.h"
#include "Render/Scene/RenderBus.h"
#include "Engine/Render/Resource/MeshBufferManager.h"
#include "Spatial/BVH.h"

class UObjViewerEngine;
class UPrimitiveComponent;

class FObjViewerRenderPipeline : public IRenderPipeline
{
  public:
    FObjViewerRenderPipeline(UObjViewerEngine* InEngine, FRenderer& InRenderer);
    ~FObjViewerRenderPipeline() override;

    void Execute(float DeltaTime, FRenderer& Renderer) override;
    void BuildInitialRenderData(UWorld* World);
    void TransferViewportData(FRenderer& Renderer);

    // Picking support
    void QueryRayCandidates(const FRay& Ray, TArray<int32>& OutComponentIndices,
                            TArray<float>& OutTs) const;
    bool RaycastCandidateComponent(int32 ComponentIndex, const FRay& Ray,
                                   FHitResult& OutHitResult) const;
    UPrimitiveComponent* GetComponentByIndex(int32 ComponentIndex) const;
    const TArray<UPrimitiveComponent*>& GetComponentArray() const;
    const TArray<FMatrix>& GetWorldMatrixArray() const;
    const TArray<FMatrix>& GetInverseWorldMatrixArray() const;
    bool GetMeshCPUDataByIndex(int32 ComponentIndex, const TArray<FNormalVertex>*& OutVertices,
                               const TArray<uint32>*& OutIndices, const FBVH*& OutMeshBVH) const;

  private:
    UObjViewerEngine*    Engine = nullptr;
    ID3D11Device*        Device = nullptr;
    ID3D11DeviceContext* DeviceContext = nullptr;
    FRenderCollector     Collector;
    FRenderBus           Bus;
    FRenderDataManager   RenderDataManager;
    FMeshBufferManager   MeshBufferManager;
    FBVH                 BVH;
};
