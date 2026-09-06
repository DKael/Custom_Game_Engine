#pragma once

#include "Core/CoreTypes.h"
#include "Render/Common/RenderTypes.h"

#include "Render/Resource/Buffer.h"

#include "Render/Mesh/MeshManager.h"

class UStaticMesh;

/*
	Mesh Manager에서 넘겨 받은 MeshData를 바탕으로 MeshBuffer를 생성하고 소유합니다.
*/

class FMeshBufferManager
{
	// LOD0 이외의 LOD 슬롯 수 (LOD1 ~ LOD9)
	static constexpr int32 MaxLODBuffers = 9;

private:
	ID3D11Device* Device = nullptr;
	TMap<EPrimitiveType, FMeshBuffer> MeshBufferMap;
	TMap<const UStaticMesh*, FMeshBuffer> StaticMeshBufferMap;
	TMap<const UStaticMesh*, FMeshBuffer> PerformanceMeshBufferMap;

	// LOD1~LOD9 전용 버퍼 맵 (인덱스 i → LOD(i+1) 버퍼)
	TMap<const UStaticMesh*, FMeshBuffer> LODMeshBufferMaps[MaxLODBuffers];

public:
	void Create(ID3D11Device* InDevice);
	void Release();

	FMeshBuffer& GetMeshBuffer(EPrimitiveType InPrimitiveType);
	FMeshBuffer* GetStaticMeshBuffer(const UStaticMesh* StaticMeshAsset);
	FMeshBuffer* GetPerformanceMeshBuffer(const UStaticMesh* StaticMeshAsset);

	// LODLevel 0 → LOD0 버퍼, 1 이상 → 해당 LOD 버퍼 (없으면 LOD0 반환)
	FMeshBuffer* GetPerformanceMeshBufferForLOD(const UStaticMesh* StaticMeshAsset, int32 LODLevel);
};