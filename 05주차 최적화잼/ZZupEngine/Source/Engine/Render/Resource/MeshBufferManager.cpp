#include "MeshBufferManager.h"

#include "Asset/StaticMesh.h"
#include "Asset/StaticMeshTypes.h"
#include <cstring>

namespace
{
	// float → IEEE 754 half-float (uint16)
	// UV 범위가 [0.0, 1.0] 내라면 충분한 정밀도 (약 0.001 @ 값 1.0)
	static uint16_t FloatToHalf(float F)
	{
		uint32_t I;
		std::memcpy(&I, &F, sizeof(float));
		const uint32_t Sign     = (I >> 16) & 0x8000; // 부호
		const uint32_t Exp      =  (I >> 23) & 0xFF;  // 지수
		const uint32_t Mantissa =   I & 0x007FFFFF;   // 진수

		if (Exp == 0xFF)  // Inf / NaN → 그대로 유지
			return static_cast<uint16_t>(Sign | 0x7C00 | (Mantissa ? 0x0200u : 0u));
		if (Exp > 142)    // Overflow → Inf
			return static_cast<uint16_t>(Sign | 0x7C00);
		if (Exp < 113)    // Underflow → 0 (UV에서는 사실상 미발생)
			return static_cast<uint16_t>(Sign);
		return static_cast<uint16_t>(Sign | ((Exp - 112) << 10) | (Mantissa >> 13));
	}

	// U, V를 각각 half로 변환하여 uint32에 pack (lo=U, hi=V)
	static uint32_t PackHalfUV(float U, float V)
	{
		return static_cast<uint32_t>(FloatToHalf(U))
		     | (static_cast<uint32_t>(FloatToHalf(V)) << 16);
	}
}

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
}

void FMeshBufferManager::Create(ID3D11Device* InDevice)
{
	Device = InDevice;
	const FMeshData QuadMeshData = CreateBillboardQuadMeshData();

	MeshBufferMap[EPrimitiveType::EPT_TransGizmo].Create(InDevice, FEditorMeshLibrary::GetTranslationGizmo());
	MeshBufferMap[EPrimitiveType::EPT_RotGizmo].Create(InDevice, FEditorMeshLibrary::GetRotationGizmo()); 
	MeshBufferMap[EPrimitiveType::EPT_ScaleGizmo].Create(InDevice, FEditorMeshLibrary::GetScaleGizmo());
	MeshBufferMap[EPrimitiveType::EPT_Billboard].Create(InDevice, QuadMeshData);
	MeshBufferMap[EPrimitiveType::EPT_SubUV].Create(InDevice, QuadMeshData);
	MeshBufferMap[EPrimitiveType::EPT_Text].Create(InDevice, QuadMeshData);
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

	for (auto& pair : PerformanceMeshBufferMap)
	{
		pair.second.Release();
	}
	PerformanceMeshBufferMap.clear();

	for (int32 i = 0; i < MaxLODBuffers; ++i)
	{
		for (auto& pair : LODMeshBufferMaps[i])
		{
			pair.second.Release();
		}
		LODMeshBufferMaps[i].clear();
	}

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

// 처음 MeshBuffer에 저장된 Vertices, Indices를 읽어서 PerformanceMeshBuffer에 불러온다.
FMeshBuffer* FMeshBufferManager::GetPerformanceMeshBuffer(const UStaticMesh* StaticMeshAsset)
{
	if (!Device || !StaticMeshAsset || !StaticMeshAsset->HasValidMeshData())
	{
		return nullptr;
	}
	
	// MeshBufferMap에 이미 vertex buffer가 초기화되어 있다면 early return, 아니라면 값 번역
	auto It = PerformanceMeshBufferMap.find(StaticMeshAsset);
	if (It != PerformanceMeshBufferMap.end())
	{
		return &It->second;
	}

	const TArray<FNormalVertex>& Vertices = StaticMeshAsset->GetVertices();
	const TArray<uint32>&        Indices  = StaticMeshAsset->GetIndices();
	if (Vertices.empty() || Indices.empty())
	{
		return nullptr;
	}

	// FNormalVertex (36 bytes) → FPerformanceVertex (16 bytes)
	// Color 필드 제거, UV float2(8 bytes) → R16G16_FLOAT(4 bytes) 압축
	TArray<FPerformanceVertex> PerformanceVertices;
	PerformanceVertices.reserve(Vertices.size());
	for (const FNormalVertex& V : Vertices)
	{
		PerformanceVertices.push_back({ V.Position, PackHalfUV(V.UVs.X, V.UVs.Y) });
	}

	FMeshBuffer& NewBuffer = PerformanceMeshBufferMap[StaticMeshAsset];
	NewBuffer.CreateForPerformanceMesh(Device, PerformanceVertices, Indices);
	return &NewBuffer;
}

// LODLevel 0 → 기존 PerformanceMeshBuffer 반환
// LODLevel 1+ → LOD 전용 버퍼 생성/캐시 후 반환, 데이터 없으면 LOD0 반환
FMeshBuffer* FMeshBufferManager::GetPerformanceMeshBufferForLOD(const UStaticMesh* StaticMeshAsset, int32 LODLevel)
{
	if (!Device || !StaticMeshAsset)
		return nullptr;

	// LOD0 or 범위 초과 → 기존 경로
	if (LODLevel <= 0 || LODLevel > MaxLODBuffers)
		return GetPerformanceMeshBuffer(StaticMeshAsset);

	const FStaticMesh* LODData = StaticMeshAsset->GetLODMeshData(LODLevel);
	if (!LODData || LODData->Vertices.empty() || LODData->Indices.empty())
		return GetPerformanceMeshBuffer(StaticMeshAsset); // 해당 LOD 없으면 LOD0 사용

	// 캐시 조회
	int32 MapIdx = LODLevel - 1; // LOD1 → index 0, LOD2 → index 1, ...
	auto It = LODMeshBufferMaps[MapIdx].find(StaticMeshAsset);
	if (It != LODMeshBufferMaps[MapIdx].end())
		return &It->second;

	// 신규 생성: FNormalVertex → FPerformanceVertex 변환
	const TArray<FNormalVertex>& Vertices = LODData->Vertices;
	const TArray<uint32>&        Indices  = LODData->Indices;

	TArray<FPerformanceVertex> PerfVertices;
	PerfVertices.reserve(Vertices.size());
	for (const FNormalVertex& V : Vertices)
		PerfVertices.push_back({ V.Position, PackHalfUV(V.UVs.X, V.UVs.Y) });

	FMeshBuffer& NewBuffer = LODMeshBufferMaps[MapIdx][StaticMeshAsset];
	NewBuffer.CreateForPerformanceMesh(Device, PerfVertices, Indices);
	return &NewBuffer;
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