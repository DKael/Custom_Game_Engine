#pragma once

#include "Engine/Core/CoreMinimal.h"
#include "Engine/Render/Resource/Buffer.h"
#include "Engine/Render/Resource/Material.h"
#include "Engine/Render/Resource/MeshBufferManager.h"
#include "Engine/Asset/StaticMeshTypes.h"
#include "Spatial/BVH.h"
#include "Render/Scene/RenderCommand.h"

class UWorld;
class UPrimitiveComponent;
class UStaticMesh;

class FRenderDataManager
{
  public:
    FRenderDataManager() = default;
    ~FRenderDataManager() = default;

    void BuildInitialRenderData(UWorld* World, FMeshBufferManager& InMeshBufferMgr,
                                ID3D11Device* InDevice, ID3D11DeviceContext* InContext);

    // 카메라 위치 기반으로 각 컴포넌트의 LOD를 선택하고 MeshBuffer를 교체
    void UpdateLODForCamera(const FVector& CameraPos, float VerticalFOVRad, FMeshBufferManager& MeshBufferMgr);

    void                          UpdateRenderData(int Index, FRenderCommand&& RenderData);
    void                          SetVisibleIndices(const TArray<uint32>& InVisibleIndices);
    const TArray<FRenderCommand>& GetRenderDataArray() const { return RenderDataArray; }

    const TArray<FAABB>& GetAABBBoundsArray() const { return AABBBoundsArray; }
    UPrimitiveComponent* GetComponentByIndex(int32 ComponentIndex) const;
    int32 GetComponentCount() const { return static_cast<int32>(ComponentArray.size()); }

    const TArray<FMatrix>& GetWorldMatrixArray() const { return WorldMatrixArray; }
    const TArray<FMatrix>& GetInverseWorldMatrixArray() const { return InverseWorldMatrixArray; }
    const TArray<UPrimitiveComponent*>& GetComponentArray() const { return ComponentArray; }

    bool GetMeshCPUDataByIndex(int32 ComponentIndex, const TArray<FNormalVertex>*& OutVertices,
                               const TArray<uint32>*& OutIndices, const FBVH*& OutMeshBVH) const;

    ID3D11ShaderResourceView* GetWorldMatrixBufferSRV() const { return WorldMatrixBuffer.GetSRV(); }

    // 프러스텀 컬링 결과(매 프레임 갱신)
    void                          SetVisibleRenderCommandIndices(const TArray<uint32>& Indices);
    const TArray<FRenderCommand> GetVisibleRenderCommands() const;
    const TArray<uint32>&	GetVisibleRenderCommandIndices() const { return SortedVisibleIndices; }

  private:
    struct FMeshCPUDataRef
    {
        const TArray<FNormalVertex>* Vertices = nullptr;
        const TArray<uint32>*        Indices = nullptr;
        const FBVH*                  MeshBVH = nullptr;
    };

    TArray<FRenderCommand> RenderDataArray;  // SubMash RenderData
    TArray<FAABB>          AABBBoundsArray; //component's AABB
    TArray<FMatrix>        WorldMatrixArray;
    TArray<FMatrix>        InverseWorldMatrixArray;
    TArray<TArray<uint32>> ComponentToRenderDataIndices;
    TArray<UPrimitiveComponent*> ComponentArray;
    TArray<FMeshCPUDataRef>       ComponentMeshCPUData;

    // LOD 관리용 per-component 데이터
    TArray<int32>        ComponentCurrentLOD;      // 현재 LOD 레벨 (변경 감지용)

    // 각 RenderCommand의 원본 섹션 범위 (LOD 전환 후 복원용)
    // [compIdx][sectionIdx] = {OriginalStart, OriginalCount}
    TArray<TArray<uint32>> ComponentOriginalIndexStarts;
    TArray<TArray<uint32>> ComponentOriginalIndexCounts;

    FStructuredBuffer      WorldMatrixBuffer; // GPU StructuredBuffer — t1 (VS)

	// sorted Visible rendercommand's index
    TArray<uint32> SortedVisibleIndices;

	//TArray<uint32> VisibleRenderCommandIndices;
};
