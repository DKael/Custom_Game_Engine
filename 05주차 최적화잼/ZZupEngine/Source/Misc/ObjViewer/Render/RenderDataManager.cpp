#include "RenderDataManager.h"

#include "Engine/GameFramework/World.h"
#include "Engine/GameFramework/AActor.h"
#include "Engine/Component/StaticMeshComponent.h"
#include "Engine/Asset/StaticMesh.h"
#include "Engine/Core/ResourceTypes.h"
#include "Engine/Core/ResourceManager.h"
#include "Misc/ObjViewer/Render/RenderDataManager.h"
#include "Render/Scene/RenderCommand.h"
#include <algorithm>

namespace
{
    // ScreenCoverage: AABB 외접구 반지름 / (카메라 거리 * tan(FOV/2))
    // 값이 클수록 화면을 많이 차지 → 낮은 LOD 번호(고화질) 선택
    static int32 SelectLODLevel(float ScreenCoverage, int32 ValidLODCount)
    {
        if (ValidLODCount <= 1) return 0;

        // coverage 임계값: 내림차순 — 커버리지가 이 값 이상이면 해당 LOD 이하 사용
        static constexpr float Thresholds[] = { 0.05f, 0.03f, 0.01f, 0.008f };
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

void FRenderDataManager::BuildInitialRenderData(UWorld* World, FMeshBufferManager& InMeshBufferMgr,
                                                ID3D11Device* InDevice, ID3D11DeviceContext* InContext)
{
    RenderDataArray.clear();
    AABBBoundsArray.clear();
    WorldMatrixArray.clear();
    InverseWorldMatrixArray.clear();
    ComponentToRenderDataIndices.clear();
    ComponentArray.clear();
    ComponentMeshCPUData.clear();
    // StaticMeshArray.clear();
    ComponentCurrentLOD.clear();
    ComponentOriginalIndexStarts.clear();
    ComponentOriginalIndexCounts.clear();
    WorldMatrixBuffer.Release();

    if (!World)
    {
        return;
    }

    ID3D11ShaderResourceView* DefaultSRV = FResourceManager::Get().GetDefaultWhiteSRV();
    uint32                    ComponentIndex = 0;

    for (AActor* Actor : World->GetActors())
    {
        for (UPrimitiveComponent* Primitive : Actor->GetPrimitiveComponents())
        {
            if (Primitive->IsA<UStaticMeshComponent>() == false)
            {
                continue;
            }

            AABBBoundsArray.push_back(Primitive->GetWorldAABB());
            const FMatrix WorldMatrix = Primitive->GetWorldMatrix();
            WorldMatrixArray.push_back(WorldMatrix);
            InverseWorldMatrixArray.push_back(WorldMatrix.GetInverse());
            ComponentToRenderDataIndices.push_back({});
            ComponentOriginalIndexStarts.push_back({});
            ComponentOriginalIndexCounts.push_back({});
            ComponentArray.push_back(Primitive);
            ComponentCurrentLOD.push_back(0);
            ComponentMeshCPUData.push_back({});

            UStaticMeshComponent* StaticMeshComp = static_cast<UStaticMeshComponent*>(Primitive);
            if (StaticMeshComp->HasValidMesh() == false)
            {
                // StaticMeshArray.push_back(nullptr);
                ++ComponentIndex;
                continue;
            }

            UStaticMesh* StaticMesh = StaticMeshComp->GetStaticMesh();
            ComponentMeshCPUData[ComponentIndex].Vertices = &StaticMesh->GetVertices();
            ComponentMeshCPUData[ComponentIndex].Indices = &StaticMesh->GetIndices();
            ComponentMeshCPUData[ComponentIndex].MeshBVH = &StaticMesh->GetMeshBVH();
            // StaticMeshArray.push_back(StaticMesh);

            for (const FStaticMeshSection& Section : StaticMesh->GetSections())
            {
                FRenderCommand RenderCommand = {};
                RenderCommand.PerObjectConstants.Model = StaticMeshComp->GetWorldMatrix();
                RenderCommand.PerObjectPerformanceConstants.ComponentIndex = ComponentIndex;
                RenderCommand.MeshBuffer = InMeshBufferMgr.GetPerformanceMeshBuffer(StaticMesh);
                RenderCommand.SectionIndexStart = Section.StartIndex;
                RenderCommand.SectionIndexCount = Section.IndexCount;
                RenderCommand.Type = ERenderCommandType::StaticMesh;

                uint32      SlotIndex = Section.MaterialSlotIndex;
                const auto& MaterialSlots = StaticMesh->GetMaterialSlots();

                const FMaterial* MtlData = (SlotIndex < MaterialSlots.size())
                    ? StaticMeshComp->GetMaterial(SlotIndex)
                    : nullptr;

                RenderCommand.Constants.PerformanceStaticMesh.DiffuseSRV =
                    (MtlData && MtlData->bHasDiffuseTexture && MtlData->CachedDiffuseSRV)
                    ? MtlData->CachedDiffuseSRV
                    : DefaultSRV;

                RenderCommand.Constants.PerformanceStaticMesh.DiffuseColor =
                    MtlData ? MtlData->DiffuseColor : FVector{ 0.5f, 0.f, 0.5f };

                RenderCommand.Constants.PerformanceStaticMesh.bHasDiffuseMap =
                    (MtlData && MtlData->bHasDiffuseTexture) ? 1.0f : 0.0f;

                int32 RenderDataIndex = static_cast<int32>(RenderDataArray.size());

                // 원본 섹션 범위 저장 (LOD 전환 복원용)
                ComponentOriginalIndexStarts[ComponentIndex].push_back(Section.StartIndex);
                ComponentOriginalIndexCounts[ComponentIndex].push_back(Section.IndexCount);

                RenderDataArray.push_back(std::move(RenderCommand));
                ComponentToRenderDataIndices[ComponentIndex].push_back(RenderDataIndex);
            }
            ++ComponentIndex;
        }
    }

    // WorldMatrixArray를 GPU StructuredBuffer(t1)에 한꺼번에 업로드
    if (InDevice && InContext && !WorldMatrixArray.empty())
    {
        const uint32 ElementCount  = static_cast<uint32>(WorldMatrixArray.size());
        const uint32 ElementStride = sizeof(FMatrix);

        WorldMatrixBuffer.Create(InDevice, ElementCount, ElementStride);
        WorldMatrixBuffer.Update(InContext, WorldMatrixArray.data(),
                                 ElementCount * ElementStride);
    }
}

UPrimitiveComponent* FRenderDataManager::GetComponentByIndex(int32 ComponentIndex) const
{
    if (ComponentIndex < 0 || ComponentIndex >= static_cast<int32>(ComponentArray.size()))
    {
        return nullptr;
    }
    return ComponentArray[ComponentIndex];
}

bool FRenderDataManager::GetMeshCPUDataByIndex(int32 ComponentIndex,
                                               const TArray<FNormalVertex>*& OutVertices,
                                               const TArray<uint32>*& OutIndices,
                                               const FBVH*& OutMeshBVH) const
{
    OutVertices = nullptr;
    OutIndices = nullptr;
    OutMeshBVH = nullptr;

    if (ComponentIndex < 0 || ComponentIndex >= static_cast<int32>(ComponentMeshCPUData.size()))
    {
        return false;
    }

    const FMeshCPUDataRef& Ref = ComponentMeshCPUData[ComponentIndex];
    if (!Ref.Vertices || !Ref.Indices || Ref.Indices->empty())
    {
        return false;
    }

    OutVertices = Ref.Vertices;
    OutIndices = Ref.Indices;
    OutMeshBVH = Ref.MeshBVH;
    return true;
}

void FRenderDataManager::SetVisibleRenderCommandIndices(const TArray<uint32>& Indices) {

	SetVisibleIndices(Indices);
}

const TArray<FRenderCommand> FRenderDataManager::GetVisibleRenderCommands() const
{

    TArray<FRenderCommand> ReturnVisibleRenderCommands;
    ReturnVisibleRenderCommands.reserve(SortedVisibleIndices.size());
	for (const auto& idx : SortedVisibleIndices)
	{
            ReturnVisibleRenderCommands.emplace_back(RenderDataArray[idx]);
	}
	
	return ReturnVisibleRenderCommands;
}

void FRenderDataManager::UpdateRenderData(int Index, FRenderCommand&& RenderData)
{
    if (Index < 0 || Index > RenderDataArray.size())
    {
        return;
    }

    RenderDataArray[Index] = std::move(RenderData);
}

void FRenderDataManager::SetVisibleIndices(const TArray<uint32>& InVisibleIndices)
{

    SortedVisibleIndices.clear();
    SortedVisibleIndices.reserve(InVisibleIndices.size());
    for (uint32 idx : InVisibleIndices)
    {
        if (idx < static_cast<uint32>(RenderDataArray.size()))
            SortedVisibleIndices.push_back(idx);
    }

    std::sort(SortedVisibleIndices.begin(), SortedVisibleIndices.end(),
              [this](uint32 IdxA, uint32 IdxB)
              {
                  const FRenderCommand& A = RenderDataArray[IdxA];
                  const FRenderCommand& B = RenderDataArray[IdxB];

                  if (A.MeshBuffer != B.MeshBuffer)
                      return A.MeshBuffer < B.MeshBuffer;

                  if (A.Type == ERenderCommandType::StaticMesh &&
                      B.Type == ERenderCommandType::StaticMesh)
                  {
                      const ID3D11ShaderResourceView* SrvA =
                          A.Constants.PerformanceStaticMesh.DiffuseSRV;
                      const ID3D11ShaderResourceView* SrvB =
                          B.Constants.PerformanceStaticMesh.DiffuseSRV;

                      if (SrvA != SrvB)
                          return SrvA < SrvB;
                  }

                  return false;
              });
}

void FRenderDataManager::UpdateLODForCamera(const FVector& CameraPos, float VerticalFOVRad, FMeshBufferManager& MeshBufferMgr)
{
    const int32 CompCount = static_cast<int32>(ComponentArray.size());

    for (int32 CompIdx = 0; CompIdx < CompCount; ++CompIdx)
    {
        UStaticMeshComponent* StaticMeshComp = static_cast<UStaticMeshComponent*>(ComponentArray[CompIdx]);
        UStaticMesh* StaticMesh = StaticMeshComp ? StaticMeshComp->GetStaticMesh() : nullptr;
        if (!StaticMesh)
            continue;

        const int32 ValidLODCount = StaticMesh->GetValidLODCount();
        if (ValidLODCount <= 1)
            continue; // LOD 데이터 없음 → 스킵

        // AABB 외접구 반지름 및 중심점까지의 거리 계산
        const FAABB& Bounds = AABBBoundsArray[CompIdx];
        const FVector Center = (Bounds.Min + Bounds.Max) * 0.5f;
        const FVector HalfExtent = (Bounds.Max - Bounds.Min) * 0.5f;
        const float SphereRadius = std::sqrt(HalfExtent.X * HalfExtent.X +
                                             HalfExtent.Y * HalfExtent.Y +
                                             HalfExtent.Z * HalfExtent.Z);
        const FVector Diff = Center - CameraPos;
        const float Dist = std::sqrt(Diff.X * Diff.X + Diff.Y * Diff.Y + Diff.Z * Diff.Z);

        // 화면 점유 비율 (0~1): 구 반지름 / (거리 * tan(FOV/2))
        const float HalfFOV = VerticalFOVRad * 0.5f;
        const float TanHalfFOV = (HalfFOV > 0.0001f) ? std::tan(HalfFOV) : 1.0f;
        const float ScreenCoverage = (Dist > 0.0001f) ? (SphereRadius / (Dist * TanHalfFOV)) : 1.0f;

        const int32 NewLOD = SelectLODLevel(ScreenCoverage, ValidLODCount);

        // LOD 레벨 변경 없으면 스킵
        if (NewLOD == ComponentCurrentLOD[CompIdx])
            continue;

        ComponentCurrentLOD[CompIdx] = NewLOD;

        const TArray<uint32>& RenderIndices = ComponentToRenderDataIndices[CompIdx];
        if (RenderIndices.empty())
            continue;

        if (NewLOD == 0)
        {
            // LOD0 복원: 원래 MeshBuffer + 원래 섹션 범위
            FMeshBuffer* LOD0Buffer = MeshBufferMgr.GetPerformanceMeshBuffer(StaticMesh);
            for (int32 s = 0; s < static_cast<int32>(RenderIndices.size()); ++s)
            {
                FRenderCommand& Cmd = RenderDataArray[RenderIndices[s]];
                Cmd.MeshBuffer        = LOD0Buffer;
                Cmd.SectionIndexStart = ComponentOriginalIndexStarts[CompIdx][s];
                Cmd.SectionIndexCount = ComponentOriginalIndexCounts[CompIdx][s];
            }
        }
        else
        {
            // LOD1+: 단일 섹션으로 합쳐진 LOD 버퍼 사용
            // 첫 번째 RenderCommand만 전체 LOD 인덱스를 그리고, 나머지는 비활성화
            FMeshBuffer* LODBuffer = MeshBufferMgr.GetPerformanceMeshBufferForLOD(StaticMesh, NewLOD);
            if (!LODBuffer)
                continue;

            const FStaticMesh* LODData = StaticMesh->GetLODMeshData(NewLOD);
            const uint32 LODIndexCount = LODData ? static_cast<uint32>(LODData->Indices.size()) : 0;

            for (int32 s = 0; s < static_cast<int32>(RenderIndices.size()); ++s)
            {
                FRenderCommand& Cmd = RenderDataArray[RenderIndices[s]];
                Cmd.MeshBuffer = LODBuffer;

                if (s == 0)
                {
                    // 첫 번째 섹션: 전체 LOD 인덱스 범위
                    Cmd.SectionIndexStart = 0;
                    Cmd.SectionIndexCount = LODIndexCount;
                }
                else
                {
                    // 나머지 섹션: 드로우 콜 스킵 (IndexCount=0)
                    Cmd.SectionIndexStart = 0;
                    Cmd.SectionIndexCount = 0;
                }
            }
        }
    }
}
