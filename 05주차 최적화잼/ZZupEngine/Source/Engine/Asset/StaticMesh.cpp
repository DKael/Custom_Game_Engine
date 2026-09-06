#include "StaticMesh.h"

#include "UI/EditorConsoleWidget.h"
#include <algorithm>

DEFINE_CLASS(UStaticMesh, UObject)

UStaticMesh::~UStaticMesh()
{
	delete MeshData;
	MeshData = nullptr;

	for (int32 i = 1; i < LOD; ++i)
	{
		delete LODMeshData[i];
		LODMeshData[i] = nullptr;
	}
}

void UStaticMesh::SetMeshData(FStaticMesh* InMeshData, const TArray<FStaticMeshMaterialSlot>& MaterialSlot)
{
	if (MeshData == InMeshData)
	{
		return;
	}

	delete MeshData;

	// 기존 LOD 데이터 해제
	for (int32 i = 1; i < LOD; ++i)
	{
		delete LODMeshData[i];
		LODMeshData[i] = nullptr;
	}
	ValidLODCount = 1;

	MeshData = InMeshData;
	MaterialSlots = MaterialSlot;
	RebuildLocalBoundsFromMeshData();
	BuildMeshBVH();

	// 삼각형이 충분할 때만 LOD 생성 (최소 6개 삼각형 = 18개 인덱스)
	if (MeshData && MeshData->Indices.size() >= 18)
	{
		// LOD 슬롯 사전 할당
		for (int32 i = 1; i < LOD; ++i)
			LODMeshData[i] = new FStaticMesh();

		// 원본 메시 보존 (SimplifyMesh가 MeshData를 In-place 수정하므로)
		TArray<FNormalVertex> OriginalVertices = MeshData->Vertices;
		TArray<uint32>        OriginalIndices  = MeshData->Indices;

		BuildMeshQuadrics();
		SimplifyMesh(); // LODMeshData[1..N]을 채우고 MeshData를 최종 단순화 상태로 남김

		// LOD0 = 원본 메시 복원
		MeshData->Vertices = std::move(OriginalVertices);
		MeshData->Indices  = std::move(OriginalIndices);

		// 실제로 생성된 LOD 수 집계
		ValidLODCount = 1;
		for (int32 i = 1; i < LOD; ++i)
		{
			if (LODMeshData[i] && !LODMeshData[i]->Indices.empty())
				ValidLODCount = i + 1;
			else
				break;
		}

		// 빌드에 사용된 임시 데이터 해제
		Quadrics.clear();
		Edges.clear();
		EdgeUsage.clear();
		BoundaryVertices.clear();
		VertexToTriangleMap.clear();
	}
}

FStaticMesh* UStaticMesh::GetMeshData()
{
	return MeshData;
}

const FStaticMesh* UStaticMesh::GetMeshData() const
{
	return MeshData;
}

const FStaticMesh* UStaticMesh::GetLODMeshData(int32 LODLevel) const
{
	if (LODLevel <= 0 || LODLevel >= LOD)
		return nullptr;

	return LODMeshData[LODLevel];
}

const FString& UStaticMesh::GetAssetPathFileName() const
{
	static FString empty = {};
	return MeshData ? MeshData->PathFileName : empty;
}

const TArray<FNormalVertex>& UStaticMesh::GetVertices() const
{
	static const TArray<FNormalVertex> Empty = {};
	return MeshData ? MeshData->Vertices : Empty;
}

const TArray<uint32>& UStaticMesh::GetIndices() const
{
	static const TArray<uint32> Empty = {};
	return MeshData ? MeshData->Indices : Empty;
}

const TArray<FStaticMeshSection>& UStaticMesh::GetSections() const
{
	static const TArray<FStaticMeshSection> Empty = {};
	return MeshData ? MeshData->Sections : Empty;
}

const TArray<FStaticMeshMaterialSlot>& UStaticMesh::GetMaterialSlots() const
{
	static const TArray<FStaticMeshMaterialSlot> Empty = {};
	return MeshData ? MaterialSlots : Empty;
}

const FAABB& UStaticMesh::GetLocalBounds() const
{
	static const FAABB Empty = {};
	return MeshData ? MeshData->LocalBounds : Empty;
}

bool UStaticMesh::HasValidMeshData() const
{
	return MeshData != nullptr
		&& !MeshData->Vertices.empty()
		&& !MeshData->Indices.empty();
}

void UStaticMesh::RebuildLocalBoundsFromMeshData()
{
	if (!MeshData)
	{
		return;
	}

	MeshData->LocalBounds.Reset();
	for (const FNormalVertex& Vertex : MeshData->Vertices)
	{
		MeshData->LocalBounds.Expand(Vertex.Position);
	}
}

void UStaticMesh::BuildMeshBVH()
{
	if (!MeshData)
	{
		return;
	}

	const TArray<FNormalVertex>& Vertices = MeshData->Vertices;
	const TArray<uint32>& Indices = MeshData->Indices;

	const int32 NumTriangles = static_cast<int32>(Indices.size()) / 3;
	if (NumTriangles <= 0)
	{
		return;
	}

	TArray<FAABB> TriangleBounds;
	TriangleBounds.reserve(NumTriangles);

	for (int32 i = 0; i < NumTriangles; ++i)
	{
		const FVector& V0 = Vertices[Indices[i * 3 + 0]].Position;
		const FVector& V1 = Vertices[Indices[i * 3 + 1]].Position;
		const FVector& V2 = Vertices[Indices[i * 3 + 2]].Position;

		FAABB TriAABB;
		TriAABB.Expand(V0);
		TriAABB.Expand(V1);
		TriAABB.Expand(V2);

		TriangleBounds.push_back(TriAABB);
	}

	MeshBVH.BuildBLAS(TriangleBounds);
}

void UStaticMesh::BuildMeshQuadrics()
{
	if (!MeshData)
	{
		return;
	}
	
	// SIMD 최적화된 람다 함수 (Normal과 d값을 통해 Quadric 행렬 계산)
	auto AddPlane = [&](uint32 VertIdx, const FVector& InNormal, float InD)
    {
        // 1. 평면 벡터 p = [a, b, c, d] 생성 (SIMD 레지스터 로드)
        DirectX::XMVECTOR P = DirectX::XMVectorSet(InNormal.X, InNormal.Y, InNormal.Z, InD);

        // 2. 외적(Outer Product)을 SIMD로 계산 (p * p^T)
        // P 벡터에 각각 a, b, c, d를 복제(Replicate)하여 곱하면 4x4 행렬의 각 행(Row)이 완성됩니다.
        DirectX::XMVECTOR R0 = DirectX::XMVectorMultiply(P, DirectX::XMVectorReplicate(InNormal.X));
        DirectX::XMVECTOR R1 = DirectX::XMVectorMultiply(P, DirectX::XMVectorReplicate(InNormal.Y));
        DirectX::XMVECTOR R2 = DirectX::XMVectorMultiply(P, DirectX::XMVectorReplicate(InNormal.Z));
        DirectX::XMVECTOR R3 = DirectX::XMVectorMultiply(P, DirectX::XMVectorReplicate(InD));

        // 3. 기존 Quadric 행렬에 누적 합산 (SIMD 덧셈)
        // FMatrix 구조를 참조하여 기존 데이터를 로드, 더한 후 다시 저장합니다.
        float* MatrixPtr = Quadrics[VertIdx].M[0]; // FMatrix의 시작 포인터 가정

        DirectX::XMVECTOR Q0 = DirectX::XMVectorAdd(DirectX::XMLoadFloat4(reinterpret_cast<const DirectX::XMFLOAT4*>(&MatrixPtr[0])), R0);
        DirectX::XMVECTOR Q1 = DirectX::XMVectorAdd(DirectX::XMLoadFloat4(reinterpret_cast<const DirectX::XMFLOAT4*>(&MatrixPtr[4])), R1);
        DirectX::XMVECTOR Q2 = DirectX::XMVectorAdd(DirectX::XMLoadFloat4(reinterpret_cast<const DirectX::XMFLOAT4*>(&MatrixPtr[8])), R2);
        DirectX::XMVECTOR Q3 = DirectX::XMVectorAdd(DirectX::XMLoadFloat4(reinterpret_cast<const DirectX::XMFLOAT4*>(&MatrixPtr[12])), R3);

        DirectX::XMStoreFloat4(reinterpret_cast<DirectX::XMFLOAT4*>(&MatrixPtr[0]), Q0);
        DirectX::XMStoreFloat4(reinterpret_cast<DirectX::XMFLOAT4*>(&MatrixPtr[4]), Q1);
        DirectX::XMStoreFloat4(reinterpret_cast<DirectX::XMFLOAT4*>(&MatrixPtr[8]), Q2);
        DirectX::XMStoreFloat4(reinterpret_cast<DirectX::XMFLOAT4*>(&MatrixPtr[12]), Q3);
    };

	const TArray<FNormalVertex>& Vertices = MeshData->Vertices;
	const TArray<uint32>& Indices = MeshData->Indices;

	// Topological Vertices 초기화 (위치가 같지만 uv, normal 값이 같은 중복 정점을 제거)
	TopologicalVertices.clear();
	RenderToTopoMap.clear();

	for (int i = 0; i < Vertices.size(); i++)
	{
		const FVector& Pos = MeshData->Vertices[i].Position;
		int32 idx = -1;

		for (int32 t = 0; t < TopologicalVertices.size(); t++)
		{
			if ((TopologicalVertices[t].Position - Pos).SizeSquared() < 1e-6f)
			{
				idx = static_cast<int32>(t);
				break;
			}
		}

		if (idx == -1)
		{
			FTopologicalVertex NewTopologicalVertex;
			NewTopologicalVertex.Position = Pos;
			idx = static_cast<int32>(TopologicalVertices.size());
			TopologicalVertices.push_back(NewTopologicalVertex);
		}
		
		// 서로 다르지만 위치가 같은 정점들을 Render To Topological Vertex 맵에 연결
		TopologicalVertices[idx].RenderVertices.push_back(i);
		RenderToTopoMap[i] = idx;
	}

	const TArray<uint32> PrimeTable = 
	{ 
		53, 97, 193, 389, 769, 1543, 3079, 6151, 12289, 24593, 
		49157, 98317, 196613, 393241, 786433, 1572869, 3145739, 6291469, 
		12582917, 25165843, 50331653, 100663319, 201326611, 402653189, 805306457,
	};

	Quadrics.resize(MeshData->Vertices.size());
	uint32 NextPrime = *std::lower_bound(PrimeTable.begin(), PrimeTable.end(), 2u * MeshData->Vertices.size() + 1u);

	VertexToTriangleMap.reserve(NextPrime);
	uint32 NumTriangles = static_cast<uint32>(MeshData->Indices.size()) / 3u;

	for (uint32 tidx = 0; tidx < NumTriangles; tidx++)
	{
		const uint32 I0 = Indices[tidx * 3 + 0];
		const uint32 I1 = Indices[tidx * 3 + 1];
		const uint32 I2 = Indices[tidx * 3 + 2];

		const uint32& V0 = RenderToTopoMap[I0];
		const uint32& V1 = RenderToTopoMap[I1];
		const uint32& V2 = RenderToTopoMap[I2];

		const FIndexEdge E0 = FIndexEdge(V0, V1);
		const FIndexEdge E1 = FIndexEdge(V0, V2);
		const FIndexEdge E2 = FIndexEdge(V1, V2);

		Edges.emplace(E0); Edges.emplace(E1); Edges.emplace(E2);
		EdgeUsage[E0]++; EdgeUsage[E1]++; EdgeUsage[E2]++;

		// Quadric 계산: 평면 방정식 Dot(N, V) + d = 0
		FVector V01 = Vertices[V1].Position - Vertices[V0].Position;
		FVector V02 = Vertices[V2].Position - Vertices[V0].Position;
		FVector Normal = FVector::CrossProduct(V01, V02).GetSafeNormal();
		float d = -FVector::DotProduct(Normal, Vertices[V0].Position);

		AddPlane(V0, Normal, d);
		AddPlane(V1, Normal, d);
		AddPlane(V2, Normal, d);

		VertexToTriangleMap[V0].emplace(tidx);
		VertexToTriangleMap[V1].emplace(tidx);
		VertexToTriangleMap[V2].emplace(tidx);
	}

	// Nanite는 전체 메쉬를 한번에 간소화하지 않고, 삼각형 128개-512개로 구성된 클러스터 단위로 간소화함
	// 따라서 클러스터의 가장자리에 해당되는 정점을 간소화하지 않고 넘기기 위해 따로 저장한다.
	for (const auto& [E, cnt] : EdgeUsage)
	{
		if (cnt == 1)
		{
			BoundaryVertices.emplace(E.A);
			BoundaryVertices.emplace(E.B);
		}
	}

	return;
}

// 정점의 위치벡터를 v = [x, y, z, 1]이라고 할 때, 오차값 E(v) = v^TQv를 구한다.
float UStaticMesh::CalculateVertexError(const FMatrix& Q, const FVector& Pos)
{
    const DirectX::XMVECTOR V = DirectX::XMVectorSet(Pos.X, Pos.Y, Pos.Z, 1.0f);

    // Q는 대칭 행렬이므로 v^T*Q*v == dot(v, Q*v) == dot(v*Q, v)
    // XMVector4Transform은 행 벡터 규약(v * M)으로 계산하며, Q가 대칭이면 v*Q == Q*v이므로 결과 동일
    const DirectX::XMMATRIX* QMat = reinterpret_cast<const DirectX::XMMATRIX*>(&Q.M[0][0]);
    const DirectX::XMVECTOR VtQ = DirectX::XMVector4Transform(V, *QMat);

    // v^T * Q * v = dot(v, Q*v)
    return DirectX::XMVectorGetX(DirectX::XMVector4Dot(V, VtQ));
}

FCollapseCandidate UStaticMesh::CalculateEdgeError(uint32 ia, uint32 ib)
{
	FCollapseCandidate Candidate;
	Candidate.Edge = FIndexEdge(ia, ib);

	FMatrix Q = Quadrics[ia] + Quadrics[ib];

	FVector PosA = TopologicalVertices[ia].Position;
    FVector PosB = TopologicalVertices[ib].Position;
    FVector PosMid = (PosA + PosB) * 0.5f;

    // [중요] 이전 코드에서 빠져있었던 에러 사전 계산 (FLT_MAX 방지)
    float ErrorA = CalculateVertexError(Q, PosA);
    float ErrorB = CalculateVertexError(Q, PosB);
    float ErrorMid = CalculateVertexError(Q, PosMid);

    // ==========================================================
    // 모드 1: Half-Edge Collapse (추천)
    // 텍스쳐 찢어짐(UV 왜곡)을 최소화하고 가장 안정적으로 메쉬를 줄입니다.
    // ==========================================================
    // 1. 성게(Spike) 버그를 막기 위한 3x3 수동 행렬식 검사
    float det3x3 = 
        Q.M[0][0] * (Q.M[1][1] * Q.M[2][2] - Q.M[1][2] * Q.M[2][1]) -
        Q.M[0][1] * (Q.M[1][0] * Q.M[2][2] - Q.M[1][2] * Q.M[2][0]) +
        Q.M[0][2] * (Q.M[1][0] * Q.M[2][1] - Q.M[1][1] * Q.M[2][0]);

    FMatrix Q_Opt = Q;
    Q_Opt.M[3][0] = 0.0f; Q_Opt.M[3][1] = 0.0f; Q_Opt.M[3][2] = 0.0f; Q_Opt.M[3][3] = 1.0f;

    if (std::abs(det3x3) > 1e-3f && Q_Opt.IsInvertible())
    {
        FMatrix Q_Inv = Q_Opt.GetInverse();
        Candidate.OptimalPos = FVector(Q_Inv.M[0][3], Q_Inv.M[1][3], Q_Inv.M[2][3]);
        Candidate.Error = CalculateVertexError(Q, FVector(Q_Inv.M[0][3], Q_Inv.M[1][3], Q_Inv.M[2][3]));
    }
    else
    {
        Candidate.Error = std::min({ErrorA, ErrorB, ErrorMid});
            
        if (Candidate.Error == ErrorMid)    Candidate.OptimalPos = PosMid;
        else if (Candidate.Error == ErrorA) Candidate.OptimalPos = PosA;
        else                                Candidate.OptimalPos = PosB;
    }

	// UV 찢어짐 방지: 두 위상 정점의 렌더 정점 UV 차이가 클수록 패널티 부여
	// UV 좌표가 크게 다른 간선(UV seam 경계)은 collapse 우선순위를 낮춘다.
	float MaxUVDistSq = 0.0f;
	for (uint32 RA : TopologicalVertices[ia].RenderVertices)
	{
		for (uint32 RB : TopologicalVertices[ib].RenderVertices)
		{
			float du = MeshData->Vertices[RA].UVs.X - MeshData->Vertices[RB].UVs.X;
			float dv = MeshData->Vertices[RA].UVs.Y - MeshData->Vertices[RB].UVs.Y;
			MaxUVDistSq = std::max(MaxUVDistSq, du * du + dv * dv);
		}
	}
	constexpr float UVSeamPenaltyScale = 1000.0f;
	Candidate.Error += UVSeamPenaltyScale * MaxUVDistSq;

	// Nanite 엣지 보존 로직, BoundaryVertex인 경우 무한대 오차 부여
	if (BoundaryVertices.contains(ia) || BoundaryVertices.contains(ib))
		Candidate.Error = FLT_MAX;

	return Candidate;
}

void UStaticMesh::SimplifyMesh()
{
	// [렌더링 정점, 위상 정점]을 추적하는 배열 생성 (런타임에 정보가 바뀌는 배열)
	TArray<uint32> TopologicalIndices(MeshData->Indices.size());
	for (int i = 0; i < TopologicalIndices.size(); i++)
	{
		TopologicalIndices[i] = RenderToTopoMap[MeshData->Indices[i]];
	}

	std::priority_queue<FCollapseCandidate> pq;

	// 모든 유효 간선의 오차를 계산하여 큐에 삽입한다.
	for (auto e : Edges)
	{
		FCollapseCandidate CandidateEdge = CalculateEdgeError(e.A, e.B);
		pq.push(CandidateEdge);
	}

	float TargetRatio = 0.9f;
	int32 CurrentTriangles = static_cast<int32>(MeshData->Indices.size()) / 3;
	int32 TargetTriangles = static_cast<int32>(CurrentTriangles * TargetRatio);
	int32 CurrentLOD = 1;

	TArray<bool> IsVertexDeleted(MeshData->Vertices.size(), false);

	// priority queue에서 계산된 오차가 작은 순서대로 간선이 pop된다.
	while (pq.size() && CurrentLOD < LOD)
	{
		FCollapseCandidate Victim = pq.top();
		pq.pop();

		uint32 ia = Victim.Edge.A;
		uint32 ib = Victim.Edge.B;

		if (IsVertexDeleted[ia] || IsVertexDeleted[ib] || ia == ib)
		{
			continue;
		}

		FCollapseCandidate CurrentState = CalculateEdgeError(ia, ib);
        // [이전 데이터 검증] 계산된 오차가 큐에 있던 오차보다 크다면(쿼드릭이 그동안 누적되었다면) 과거 데이터임
        if (CurrentState.Error > Victim.Error + 0.0001f)
        {
            pq.push(CurrentState); // 최신 데이터로 다시 큐에 넣고 스킵
            continue;
        }
        Victim = CurrentState;
		
        // [경계선 파괴 방지] 더 이상 안전하게 간소화할 수 있는 간선이 없으므로 남은 LOD를 건너뛰거나 종료
		if (Victim.Error >= FLT_MAX)
        {
			break;
        }

		Quadrics[ia] = Quadrics[ia] + Quadrics[ib];

		// 최적 위치의 ia/ib 기여 가중치 계산 (UV 보간용)
		// OptimalPos가 ia에 가까울수록 WeightA가 크고, ib에 가까울수록 WeightB가 크다.
		const FVector PosA = TopologicalVertices[ia].Position;
		const FVector PosB = TopologicalVertices[ib].Position;
		const FVector OptPos = FVector(Victim.OptimalPos.X, Victim.OptimalPos.Y, Victim.OptimalPos.Z);
		const float DistAB_sq = (PosB - PosA).SizeSquared();
		float WeightB = 0.5f;
		if (DistAB_sq > 1e-10f)
		{
			const FVector AB = PosB - PosA;
			const float t = FVector::DotProduct(OptPos - PosA, AB) / DistAB_sq;
			WeightB = std::max(0.0f, std::min(1.0f, t));
		}
		const float WeightA = 1.0f - WeightB;
		TopologicalVertices[ia].Position = OptPos;

		// ia의 렌더 정점: 위치만 업데이트 (UV는 원래 UV island 유지)
		for (uint32 RenderIndex : TopologicalVertices[ia].RenderVertices)
		{
			MeshData->Vertices[RenderIndex].Position = OptPos;
		}

		// ib의 렌더 정점: 위치 업데이트 + UV를 ia/ib 사이에서 가중치 보간
		// ia 참조 UV: ia 렌더 정점 중 첫 번째를 기준으로 사용
		const uint32 RefIaRenderIndex = TopologicalVertices[ia].RenderVertices.empty()
			? ia : TopologicalVertices[ia].RenderVertices[0];
		for (uint32 RenderIndex : TopologicalVertices[ib].RenderVertices)
		{
			MeshData->Vertices[RenderIndex].Position = OptPos;

			int32 TextureMode = 0;

			if (TextureMode == 0)
			{
				MeshData->Vertices[RenderIndex].UVs.X =
					WeightA * MeshData->Vertices[RefIaRenderIndex].UVs.X +
					WeightB * MeshData->Vertices[RenderIndex].UVs.X;
				MeshData->Vertices[RenderIndex].UVs.Y =
					WeightA * MeshData->Vertices[RefIaRenderIndex].UVs.Y +
					WeightB * MeshData->Vertices[RenderIndex].UVs.Y;
			}
			else if (TextureMode == 1)
			{
				MeshData->Vertices[RenderIndex].UVs.X = MeshData->Vertices[RefIaRenderIndex].UVs.X;
				MeshData->Vertices[RenderIndex].UVs.Y = MeshData->Vertices[RefIaRenderIndex].UVs.Y;
			}
			else
			{
				MeshData->Vertices[RenderIndex].UVs.X = MeshData->Vertices[RenderIndex].UVs.X;
				MeshData->Vertices[RenderIndex].UVs.Y = MeshData->Vertices[RenderIndex].UVs.Y;
			}

			TopologicalVertices[ia].RenderVertices.push_back(RenderIndex);
		}
		TopologicalVertices[ib].RenderVertices.clear();

		// 3. ib를 포함하는 삼각형을 순회하며 ib -> ia로 교체, 파괴할 간선을 밑변으로 공유하는 삼각형을 삭제한다.
		TSet<uint32> NewNeighbors;
		for (uint32 TriangleIndex : VertexToTriangleMap[ib])
		{
			uint32& I0 = TopologicalIndices[TriangleIndex * 3 + 0];
			uint32& I1 = TopologicalIndices[TriangleIndex * 3 + 1];
			uint32& I2 = TopologicalIndices[TriangleIndex * 3 + 2];
			
            // [퇴화 삼각형 이중 카운팅 방지] 변경 전 원래 퇴화되어 있던 상태인지 기억
            bool bWasDegenerated = (I0 == I1 || I1 == I2 || I0 == I2);
			
			if (I0 == ib) I0 = ia;
			if (I1 == ib) I1 = ia;
			if (I2 == ib) I2 = ia;

			// 퇴화 삼각형 검사: 두 꼭짓점이 같으면 삭제 (ia-ib를 밑변으로 공유하던 삼각형)
			if (I0 == I1 || I1 == I2 || I0 == I2)
			{
				// 퇴화된 삼각형을 인덱스 버퍼에서 제거하기 위해 세 인덱스를 동일하게 처리
				I0 = I1 = I2 = ia;
				if (!bWasDegenerated) 
				{
					CurrentTriangles--;
				}
			}
			else
			{
				VertexToTriangleMap[ia].emplace(TriangleIndex);

				if (I0 != ia) NewNeighbors.emplace(I0);
				if (I1 != ia) NewNeighbors.emplace(I1);
				if (I2 != ia) NewNeighbors.emplace(I2);
			}
		}

		// 4. ib 정점 삭제 표시 및 VertexToTriangleMap에서 제거
		IsVertexDeleted[ib] = true;
		VertexToTriangleMap.erase(ib);

		// 5. ia의 새 이웃 정점들과의 엣지 오차를 재계산하여 큐에 삽입
		// priority queue에 저장된 원래 엣지들은 초반의 IsVertexDeleted 조건문에서 삭제
		for (uint32 vn : NewNeighbors)
		{
			if (!IsVertexDeleted[vn])
			{
				pq.push(CalculateEdgeError(ia, vn));
			}
		}

		// 6. 계산한 현재 단계의 LOD 저장
		if (CurrentTriangles <= TargetTriangles)
		{
			SaveCurrentStateAsLOD(CurrentLOD, TopologicalIndices);

			CurrentLOD++;
			TargetRatio *= 0.6f;
			TargetTriangles = static_cast<int32>(CurrentTriangles * TargetRatio);
			if (TargetTriangles < 16)
				TargetTriangles = 16;

			if (CurrentTriangles < 16)
				break;
		}
	}

	// 7. 퇴화된 삼각형(세 인덱스가 모두 동일)을 인덱스 버퍼에서 제거하여 최종 메쉬를 정리
	TArray<uint32> NewIndices;
    NewIndices.reserve(static_cast<size_t>(CurrentTriangles) * 3);

    const int32 NumOriginalTriangles = static_cast<int32>(MeshData->Indices.size()) / 3;
    for (int32 tidx = 0; tidx < NumOriginalTriangles; tidx++)
    {
        // 최후의 위상 상태(TopoIndices)를 확인하여 퇴화되었는지 검사
        uint32 T0 = TopologicalIndices[tidx * 3 + 0];
        uint32 T1 = TopologicalIndices[tidx * 3 + 1];
        uint32 T2 = TopologicalIndices[tidx * 3 + 2];

        if (T0 != T1 && T1 != T2 && T0 != T2)
        {
            // 위상이 유효한 삼각형만, "원본 렌더링 인덱스"를 그대로 가져와 저장합니다.
            NewIndices.push_back(MeshData->Indices[tidx * 3 + 0]);
            NewIndices.push_back(MeshData->Indices[tidx * 3 + 1]);
            NewIndices.push_back(MeshData->Indices[tidx * 3 + 2]);
        }
    }

	MeshData->Indices = std::move(NewIndices);
}

void UStaticMesh::SaveCurrentStateAsLOD(int32 CurrentLOD, const TArray<uint32>& TopologicalIndices)
{
    if (CurrentLOD < 0 || CurrentLOD >= LOD || !MeshData)
    {
        return;
    }

    // 현재 LOD 레벨의 메쉬 데이터 레퍼런스
    FStaticMesh* TargetLOD = LODMeshData[CurrentLOD];

    // 1. 현재까지의 정점(Vertices) 상태 복사 
    // (삭제된 정점의 위치 등도 포함되지만, 인덱스에서 참조하지 않으므로 렌더링에 영향 없음)
    TargetLOD->Vertices = MeshData->Vertices;

    // 2. 퇴화되지 않고 살아남은 삼각형(Indices)만 추출하여 복사
    TArray<uint32> ValidIndices;
    const int32 NumOriginalTriangles = static_cast<int32>(MeshData->Indices.size()) / 3;
    ValidIndices.reserve(NumOriginalTriangles * 3); 

    for (int32 tidx = 0; tidx < NumOriginalTriangles; tidx++)
    {
		// 위치에 대한 계산이므로 퇴화 여부는 위상 정점의 인덱스로 판별
        const uint32 I0 = TopologicalIndices[tidx * 3 + 0];
        const uint32 I1 = TopologicalIndices[tidx * 3 + 1];
        const uint32 I2 = TopologicalIndices[tidx * 3 + 2];

        // 퇴화된 삼각형(면적이 0인 삼각형)이 아닌 경우에만 추가
        if (I0 != I1 && I1 != I2 && I0 != I2)
        {
            ValidIndices.push_back(MeshData->Indices[tidx * 3 + 0]);
            ValidIndices.push_back(MeshData->Indices[tidx * 3 + 1]);
            ValidIndices.push_back(MeshData->Indices[tidx * 3 + 2]);
        }
    }

    TargetLOD->Indices = std::move(ValidIndices);

    // 3. LOD 메시는 간소화로 섹션 경계가 무너지므로 단일 섹션으로 통합
    FStaticMeshSection MergedSection;
    MergedSection.StartIndex      = 0;
    MergedSection.IndexCount      = static_cast<uint32>(TargetLOD->Indices.size());
    MergedSection.MaterialSlotIndex = 0;
    TargetLOD->Sections = { MergedSection };

    TargetLOD->PathFileName = MeshData->PathFileName;
}