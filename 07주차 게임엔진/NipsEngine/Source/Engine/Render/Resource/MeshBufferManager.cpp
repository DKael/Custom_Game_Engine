#include "MeshBufferManager.h"

#include "Asset/StaticMesh.h"

namespace
{
	FMeshData CreateBillboardQuadMeshData()
	{
		FMeshData QuadMeshData;
		FColor DefaultColor(1.0f, 1.0f, 1.0f, 1.0f);

		QuadMeshData.Vertices.push_back({ FVector(0.0f, -0.5f,  0.5f), DefaultColor, 0 });
		QuadMeshData.Vertices.push_back({ FVector(0.0f,  0.5f,  0.5f), DefaultColor, 0 });
		QuadMeshData.Vertices.push_back({ FVector(0.0f,  0.5f, -0.5f), DefaultColor, 0 });
		QuadMeshData.Vertices.push_back({ FVector(0.0f, -0.5f, -0.5f), DefaultColor, 0 });

		QuadMeshData.Indices = { 0, 1, 2, 0, 2, 3 };
		return QuadMeshData;
	}
        FMeshData CreateUnitCubeMeshData()
        {
            FMeshData CubeData;
            FColor    DefaultColor(1.0f, 1.0f, 1.0f, 1.0f);

            // 8개의 정점 (중심 0,0,0 / 크기 1x1x1)
            CubeData.Vertices.push_back({FVector(-0.5f, -0.5f, -0.5f), DefaultColor, 0}); // 0
            CubeData.Vertices.push_back({FVector(0.5f, -0.5f, -0.5f), DefaultColor, 0});  // 1
            CubeData.Vertices.push_back({FVector(-0.5f, 0.5f, -0.5f), DefaultColor, 0});  // 2
            CubeData.Vertices.push_back({FVector(0.5f, 0.5f, -0.5f), DefaultColor, 0});   // 3
            CubeData.Vertices.push_back({FVector(-0.5f, -0.5f, 0.5f), DefaultColor, 0});  // 4
            CubeData.Vertices.push_back({FVector(0.5f, -0.5f, 0.5f), DefaultColor, 0});   // 5
            CubeData.Vertices.push_back({FVector(-0.5f, 0.5f, 0.5f), DefaultColor, 0});   // 6
            CubeData.Vertices.push_back({FVector(0.5f, 0.5f, 0.5f), DefaultColor, 0});    // 7

            // 36개의 인덱스 (기존 데칼 셰이더에서 쓰시던 순서 그대로 가져왔습니다)
            CubeData.Indices = {0, 2, 1, 1, 2, 3, 4, 5, 6, 5, 7, 6, 2, 6, 3, 3, 6, 7,
                                0, 1, 4, 1, 5, 4, 4, 6, 0, 0, 6, 2, 1, 3, 5, 5, 3, 7};

            return CubeData;
        }
}

void FMeshBufferManager::Create(ID3D11Device* InDevice)
{
	Device = InDevice;
    const FMeshData QuadMeshData = CreateBillboardQuadMeshData();
    const FMeshData CubeData = CreateUnitCubeMeshData();

	MeshBufferMap[EPrimitiveType::EPT_TransGizmo].Create(InDevice, FEditorMeshLibrary::GetTranslationGizmo());
	MeshBufferMap[EPrimitiveType::EPT_RotGizmo].Create(InDevice, FEditorMeshLibrary::GetRotationGizmo()); 
	MeshBufferMap[EPrimitiveType::EPT_ScaleGizmo].Create(InDevice, FEditorMeshLibrary::GetScaleGizmo());
	MeshBufferMap[EPrimitiveType::EPT_Billboard].Create(InDevice, QuadMeshData);
	MeshBufferMap[EPrimitiveType::EPT_SubUV].Create(InDevice, QuadMeshData);
	MeshBufferMap[EPrimitiveType::EPT_Text].Create(InDevice, QuadMeshData);
        MeshBufferMap[EPrimitiveType::EPT_Decal].Create(InDevice, CubeData);
}


void FMeshBufferManager::Release()
{
	for (auto& pair : MeshBufferMap)
	{
		pair.second.Release();
	}
	MeshBufferMap.clear();

	for (auto& pair : StaticMeshBufferMap)
	{
		pair.second.Release();
	}
	StaticMeshBufferMap.clear();
	Device = nullptr;
}

//	MeshBuffer는 VB, IB를 모두 포함하고 있습니다.
FMeshBuffer& FMeshBufferManager::GetMeshBuffer(EPrimitiveType InPrimitiveType)
{
	auto it = MeshBufferMap.find(InPrimitiveType);
	if (it != MeshBufferMap.end())
	{
		return it->second;
	}
	
	//	존재하지 않는 PrimitiveType이 요청된 경우, Billboard Quad를 기본 반환합니다.
	return MeshBufferMap.at(EPrimitiveType::EPT_Billboard);
}

FMeshBuffer* FMeshBufferManager::GetStaticMeshBuffer(const UStaticMesh* StaticMeshAsset)
{
	if (!Device || !StaticMeshAsset || !StaticMeshAsset->HasValidMeshData())
	{
		return nullptr;
	}

	auto It = StaticMeshBufferMap.find(StaticMeshAsset);
	if (It != StaticMeshBufferMap.end())
	{
		return &It->second;
	}

	const TArray<FNormalVertex>& Vertices = StaticMeshAsset->GetVertices();
	const TArray<uint32>&        Indices  = StaticMeshAsset->GetIndices();
	if (Vertices.empty() || Indices.empty())
	{
		return nullptr;
	}

	FMeshBuffer& NewBuffer = StaticMeshBufferMap[StaticMeshAsset];
	NewBuffer.CreateForStaticMesh(Device, Vertices, Indices);
	return &NewBuffer;
}