#pragma once
#pragma once

#include "StaticMeshTypes.h"
#include "Object/Object.h"
#include "Engine/Spatial/BVH.h"
#include "Engine/Spatial/KDTree.h"
#include "Engine/Core/CollisionTypes.h"

// TopologicalVertex는 Vertex 배열에서 위치가 같지만 normal, uv 값이 다른 점들을 모두 저장한다.
struct FTopologicalVertex
{
	FVector Position;
	TArray<uint32> RenderVertices;
};

struct FCollapseCandidate
{
	FIndexEdge Edge;         // 병합할 대상 간선
	FVector4 OptimalPos; // 병합 후 새로운 정점이 위치할 최적의 좌표
	float Error = FLT_MAX;

	bool operator<(const FCollapseCandidate& Other) const
	{
		return Error > Other.Error; 
	}
};

class UStaticMesh : public UObject
{
public:
	DECLARE_CLASS(UStaticMesh, UObject)

	UStaticMesh() = default;
	~UStaticMesh() override;

	void SetMeshData(FStaticMesh* InMeshData, const TArray<FStaticMeshMaterialSlot>& MaterialSlot);

	/* Getters */
	FStaticMesh* GetMeshData();
	const FStaticMesh* GetMeshData() const;
	const FStaticMesh* GetLODMeshData(int32 LODLevel) const;

	const FString& GetAssetPathFileName() const;

	const TArray<FNormalVertex>& GetVertices() const;
	const TArray<uint32>& GetIndices() const;

	const TArray<FStaticMeshSection>& GetSections() const;
	const TArray<FStaticMeshMaterialSlot>& GetMaterialSlots() const;

	const FAABB& GetLocalBounds() const;
	const FBVH& GetMeshBVH() const { return MeshBVH; }
	bool HasValidMeshData() const;
	
	int32 GetValidLODCount() const { return ValidLODCount; }
	
private:
	void RebuildLocalBoundsFromMeshData();
	void BuildMeshBVH();
	void BuildMeshQuadrics();

	static float CalculateVertexError(const FMatrix& Q, const FVector& V);
	FCollapseCandidate CalculateEdgeError(uint32 ia, uint32 ib);
	void SimplifyMesh();
	void SaveCurrentStateAsLOD(int32 CurrentLOD, const TArray<uint32>& TopologicalIndices);

private:
	FStaticMesh* MeshData = nullptr;
	TArray<FStaticMeshMaterialSlot> MaterialSlots;
	FBVH MeshBVH;

	static constexpr int32 LOD = 5;
	FStaticMesh* LODMeshData[LOD] = {};  // LOD0=MeshData, LOD1~LOD9=간소화된 복사본
	int32        ValidLODCount    = 1;   // 실제로 빌드된 LOD 수 (LOD0 포함)

	TArray<FTopologicalVertex> TopologicalVertices; // 위치가 같은 정점을 저장하는 배열
	TMap<uint32, uint32> RenderToTopoMap;			// UV 좌표, Normal은 다르지만 위치가 같은 정점을 하나의 맵으로 관리한다.

	TArray<FMatrix> Quadrics;					    // 각 정점에 대한 오차 행렬 (2차 곡면을 표현하는 행렬)
	TSet<FIndexEdge> Edges;						    // 유효한 간선의 집합
	TMap<FIndexEdge, int32> EdgeUsage;				// 간선이 몇 개의 삼각형에서 사용되는지 기록
	TSet<uint32> BoundaryVertices;					// 꼭짓점에 포함되는 정점들의 배열
	TMap<uint32, TSet<uint32>> VertexToTriangleMap; // 각 정점이 포함된 삼각형 인덱스
};
