#include "Misc/ObjViewer/ObjViewerRenderPipeline.h"
#include "Misc/ObjViewer/ObjViewerEngine.h"

#include "Render/Renderer/Renderer.h"
#include "Component/CameraComponent.h"
#include "Component/GizmoComponent.h"
#include "GameFramework/World.h"
#include "Core/Logging/Stats.h"
#include "Core/Logging/GPUProfiler.h"
#include "Viewport/ViewportCamera.h"
#include "Component/PrimitiveComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Asset/StaticMesh.h"
#include "Render/Scene/RenderCommand.h"
#include "Render/Scene/RenderBus.h"
#include "Engine/Runtime/WindowsWindow.h"

#include <algorithm>
#include <cfloat>

namespace
{
    // 깊이별 색상 팔레트 (내부 노드용)
    static const FColor DepthColors[] = {
        FColor(255, 0,   0),   // depth 0: 빨강 (루트)
        FColor(255, 128, 0),   // depth 1: 주황
        FColor(255, 255, 0),   // depth 2: 노랑
        FColor(128, 255, 0),   // depth 3: 연두
    };
    static constexpr int32 NumDepthColors = 4;
    static const FColor    LeafColor      = FColor(0, 255, 0); // 리프: 초록

    bool RaycastStaticMeshCandidate(UPrimitiveComponent& Primitive, const FRay& Ray,
                                    const FMatrix& WorldMatrix,
                                    const FMatrix& InverseWorldMatrix,
                                    const TArray<FNormalVertex>& Vertices,
                                    const TArray<uint32>& Indices, const FBVH& MeshBVH,
                                    FHitResult& OutHitResult)
    {
        const TArray<FBVH::FNode>& Nodes = MeshBVH.GetNodes();
        const TArray<int32>&       ObjectIndices = MeshBVH.GetObjectIndices();
        const int32                RootIdx = MeshBVH.GetRootNodeIndex();

        if (Vertices.empty() || Indices.empty() || Nodes.empty() || RootIdx < 0)
        {
            return false;
        }

        const FVector LocalDirection =
            InverseWorldMatrix.TransformVector(Ray.Direction).GetSafeNormal();
        if (LocalDirection.IsNearlyZero())
        {
            return false;
        }

        FRay LocalRay;
        LocalRay.Origin = InverseWorldMatrix.TransformPosition(Ray.Origin);
        LocalRay.SetDirection(LocalDirection);

        struct FEntry
        {
            int32 NodeIndex;
            float tEnter;
        };

        TArray<FEntry> Stack;
        Stack.reserve(64);

        float tRoot = 0.0f;
        if (!Nodes[RootIdx].Bounds.IntersectRay(LocalRay, tRoot))
        {
            return false;
        }

        Stack.push_back({RootIdx, tRoot});

        float   ClosestT = FLT_MAX;
        int32   BestFaceIndex = -1;
        FVector BestLocalNormal;

        while (!Stack.empty())
        {
            const FEntry Entry = Stack.back();
            Stack.pop_back();

            if (Entry.tEnter >= ClosestT)
            {
                continue;
            }

            const FBVH::FNode& Node = Nodes[Entry.NodeIndex];
            if (Node.IsLeaf())
            {
                for (int32 k = 0; k < Node.ObjectCount; ++k)
                {
                    const int32  TriangleIndex = ObjectIndices[Node.FirstObject + k];
                    const uint32 IndexOffset = static_cast<uint32>(TriangleIndex) * 3;
                    if (IndexOffset + 2 >= Indices.size())
                    {
                        continue;
                    }

                    const FVector& V0 = Vertices[Indices[IndexOffset]].Position;
                    const FVector& V1 = Vertices[Indices[IndexOffset + 1]].Position;
                    const FVector& V2 = Vertices[Indices[IndexOffset + 2]].Position;

                    float HitT = 0.0f;
                    if (Primitive.IntersectTriangle(LocalRay.Origin, LocalRay.Direction, V0, V1,
                                                   V2, HitT)
                        && HitT >= 0.0f && HitT < ClosestT)
                    {
                        ClosestT = HitT;
                        BestFaceIndex = TriangleIndex;
                        BestLocalNormal =
                            FVector::CrossProduct(V1 - V0, V2 - V0).GetSafeNormal();
                    }
                }
                continue;
            }

            float tLeft = FLT_MAX;
            float tRight = FLT_MAX;
            const bool bHitLeft = (Node.Left >= 0)
                && Nodes[Node.Left].Bounds.IntersectRay(LocalRay, tLeft)
                && tLeft < ClosestT;
            const bool bHitRight = (Node.Right >= 0)
                && Nodes[Node.Right].Bounds.IntersectRay(LocalRay, tRight)
                && tRight < ClosestT;

            if (bHitLeft && bHitRight)
            {
                if (tLeft < tRight)
                {
                    Stack.push_back({Node.Right, tRight});
                    Stack.push_back({Node.Left, tLeft});
                }
                else
                {
                    Stack.push_back({Node.Left, tLeft});
                    Stack.push_back({Node.Right, tRight});
                }
            }
            else if (bHitLeft)
            {
                Stack.push_back({Node.Left, tLeft});
            }
            else if (bHitRight)
            {
                Stack.push_back({Node.Right, tRight});
            }
        }

        if (BestFaceIndex < 0)
        {
            return false;
        }

        const FVector LocalHitLocation = LocalRay.Origin + LocalRay.Direction * ClosestT;
        const FVector WorldHitLocation = WorldMatrix.TransformPosition(LocalHitLocation);
        FVector WorldNormal = WorldMatrix.TransformVector(BestLocalNormal);
        WorldNormal.NormalizeSafe();

        OutHitResult.bHit = true;
        OutHitResult.HitComponent = &Primitive;
        OutHitResult.Distance = (WorldHitLocation - Ray.Origin).Size();
        OutHitResult.Location = WorldHitLocation;
        OutHitResult.Normal = WorldNormal;
        OutHitResult.FaceIndex = BestFaceIndex;
        return true;
    }

    void CollectBLASForComponent(const UStaticMeshComponent* Comp, FRenderBus& Bus)
    {
        const UStaticMesh* Mesh = Comp->GetStaticMesh();
        if (!Mesh || !Mesh->HasValidMeshData()) return;

        const FBVH&              BVH     = Mesh->GetMeshBVH();
        const TArray<FBVH::FNode>& Nodes = BVH.GetNodes();
        const int32              RootIdx = BVH.GetRootNodeIndex();

        if (RootIdx < 0 || Nodes.empty()) return;

        const FMatrix& World = Comp->GetWorldMatrix();

        struct FEntry { int32 Idx; int32 Depth; };
        TArray<FEntry> Stack;
        Stack.push_back({ RootIdx, 0 });

        while (!Stack.empty())
        {
            const FEntry           Entry = Stack.back(); Stack.pop_back();
            const FBVH::FNode&     Node  = Nodes[Entry.Idx];
            const FAABB            WorldAABB = FAABB::TransformAABB(Node.Bounds, World);

            FRenderCommand Cmd        = {};
            Cmd.Type                  = ERenderCommandType::DebugBox;
            Cmd.Constants.AABB.Min    = WorldAABB.Min;
            Cmd.Constants.AABB.Max    = WorldAABB.Max;
            Cmd.Constants.AABB.Color  = Node.IsLeaf()
                                        ? LeafColor
                                        : DepthColors[Entry.Depth % NumDepthColors];

            Bus.AddCommand(ERenderPass::Editor, Cmd);

            if (!Node.IsLeaf())
            {
                if (Node.Left  >= 0) Stack.push_back({ Node.Left,  Entry.Depth + 1 });
                if (Node.Right >= 0) Stack.push_back({ Node.Right, Entry.Depth + 1 });
            }
        }
    }
} // namespace

FObjViewerRenderPipeline::FObjViewerRenderPipeline(UObjViewerEngine* InEngine, FRenderer& InRenderer)
	: Engine(InEngine)
	, Device(InRenderer.GetFD3DDevice().GetDevice())
	, DeviceContext(InRenderer.GetFD3DDevice().GetDeviceContext())
{
	Collector.Initialize(Device);
	MeshBufferManager.Create(Device);
}

FObjViewerRenderPipeline::~FObjViewerRenderPipeline() { Collector.Release(); }

void FObjViewerRenderPipeline::Execute(float DeltaTime, FRenderer& Renderer)
{
    Bus.Clear();

    UWorld*          World = Engine->GetWorld();
    FViewportCamera* Camera = Engine->GetCamera();
    if (Camera)
    {
        const auto&       Settings = Engine->GetSettings();
        const FShowFlags& ShowFlags = Settings.ShowFlags;
        EViewMode         ViewMode = Settings.ViewMode;

        Bus.SetViewProjection(Camera->GetViewMatrix(), Camera->GetProjectionMatrix());
        Bus.SetRenderSettings(ViewMode, ShowFlags);
        Renderer.SetOcclusionCullingEnabled(Settings.bOcclusionCulling);

        Collector.CollectWorld(World, ShowFlags, ViewMode, Bus);
        Collector.CollectGrid(Settings.GridSpacing, Settings.GridHalfLineCount, Bus);
        Collector.CollectGizmo(Engine->GetGizmo(), ShowFlags, Bus, true);

#if _DEBUG
        if (ShowFlags.bBLAS)
        {
            const int32 CompCount = RenderDataManager.GetComponentCount();
            for (int32 i = 0; i < CompCount; ++i)
            {
                if (const UStaticMeshComponent* SMComp =
                        dynamic_cast<const UStaticMeshComponent*>(RenderDataManager.GetComponentByIndex(i)))
                {
                    CollectBLASForComponent(SMComp, Bus);
                }
            }
        }
#endif

		//이것도 매 프레임 생성 오버헤드 있음.
        TArray<uint32> VisibleIndices;

		// 카메라 위치가 그대로이면 이전 VisibleIndices 결과 사용하기
        BVH.FrustumQuery(Camera->GetFrustum(), VisibleIndices);
        RenderDataManager.SetVisibleIndices(VisibleIndices);

        // 화면 점유 비율 기반 LOD 선택 및 Mesh Buffer 교체
        const FVector CameraPos = Bus.GetCameraPosition();
        RenderDataManager.UpdateLODForCamera(CameraPos, Camera->GetFOV(), MeshBufferManager);
    }

    // ObjViewer static meshes are rendered through RenderDataManager so the
    // generic Opaque pass should not draw the same mesh commands again.
    TArray<FRenderCommand>& OpaqueCommands = Bus.GetMutableCommands(ERenderPass::Opaque);
    OpaqueCommands.erase(std::remove_if(OpaqueCommands.begin(), OpaqueCommands.end(),
                                        [](const FRenderCommand& Command)
                                        { return Command.Type == ERenderCommandType::StaticMesh; }),
                         OpaqueCommands.end());

    Renderer.PrepareBatchers(Bus);
    Renderer.BeginFrame();

    TransferViewportData(Renderer);

    // Depth Prepass / Occlusion Cull이 현재 프레임 VP를 사용하도록 미리 설정
    if (Camera)
    {
        FMatrix CurrentVP = Camera->GetViewMatrix() * Camera->GetProjectionMatrix();
        Renderer.SetCurrentViewProjection(CurrentVP);
        Renderer.SetCurrentOcclusionCameraPosition(Camera->GetLocation());
        Renderer.SetCurrentOcclusionCameraBasis(Camera->GetEffectiveForward(),
                                                Camera->GetEffectiveRight(),
                                                Camera->GetEffectiveUp());
    }

    Renderer.Render(RenderDataManager, Bus);
    Renderer.Render(Bus);
    Engine->RenderUI(DeltaTime);
    Renderer.EndFrame();
}

void FObjViewerRenderPipeline::BuildInitialRenderData(UWorld* World)
{
	RenderDataManager.BuildInitialRenderData(World, MeshBufferManager, Device, DeviceContext);
	BVH.BuildBVH(RenderDataManager.GetAABBBoundsArray());
}

// 렌더러에 데이터를 전송해 뷰포트 크기에 맞는 서브 뷰포트 세팅을 요청한다.
void FObjViewerRenderPipeline::TransferViewportData(FRenderer& Renderer)
{
    auto& VC = Engine->GetViewportClient();

    int32 vx = static_cast<int32>(VC.GetViewportX());
    int32 vy = static_cast<int32>(VC.GetViewportY());
    int32 vw = static_cast<int32>(VC.GetViewportWidth());
    int32 vh = static_cast<int32>(VC.GetViewportHeight());

    if (vw <= 1 || vh <= 1)
    {
        if (const FWindowsWindow* Window = Engine ? Engine->GetWindow() : nullptr)
        {
            vw = static_cast<int32>(Window->GetWidth());
            vh = static_cast<int32>(Window->GetHeight());
        }

        vx = 0;
        vy = 0;
    }

    if (vw <= 0)
    {
        vw = 1;
    }

    if (vh <= 0)
    {
        vh = 1;
    }

    if (vx < 0)
    {
        vx = 0;
    }

    if (vy < 0)
    {
        vy = 0;
    }

    Renderer.GetFD3DDevice().SetSubViewport(vx, vy, vw, vh);
}

void FObjViewerRenderPipeline::QueryRayCandidates(const FRay& Ray,
                                                  TArray<int32>& OutComponentIndices,
                                                  TArray<float>& OutTs) const
{
    BVH.RayQuery(RenderDataManager.GetAABBBoundsArray(), Ray, OutComponentIndices, OutTs);
}

bool FObjViewerRenderPipeline::RaycastCandidateComponent(int32 ComponentIndex, const FRay& Ray,
                                                         FHitResult& OutHitResult) const
{
    OutHitResult.Reset();

    UPrimitiveComponent* Primitive = RenderDataManager.GetComponentByIndex(ComponentIndex);
    if (!Primitive || !Primitive->IsVisible())
    {
        return false;
    }

    const TArray<FMatrix>& WorldMatrices = RenderDataManager.GetWorldMatrixArray();
    const TArray<FMatrix>& InverseWorldMatrices = RenderDataManager.GetInverseWorldMatrixArray();
    if (ComponentIndex < 0 || ComponentIndex >= static_cast<int32>(WorldMatrices.size())
        || ComponentIndex >= static_cast<int32>(InverseWorldMatrices.size()))
    {
        return false;
    }

    const TArray<FNormalVertex>* Vertices = nullptr;
    const TArray<uint32>*        Indices = nullptr;
    const FBVH*                  MeshBVH = nullptr;
    if (!RenderDataManager.GetMeshCPUDataByIndex(ComponentIndex, Vertices, Indices, MeshBVH)
        || !Vertices || !Indices || !MeshBVH)
    {
        return false;
    }

    return RaycastStaticMeshCandidate(*Primitive, Ray, WorldMatrices[ComponentIndex],
                                      InverseWorldMatrices[ComponentIndex], *Vertices,
                                      *Indices, *MeshBVH, OutHitResult);
}

UPrimitiveComponent* FObjViewerRenderPipeline::GetComponentByIndex(int32 ComponentIndex) const
{
    return RenderDataManager.GetComponentByIndex(ComponentIndex);
}

const TArray<UPrimitiveComponent*>& FObjViewerRenderPipeline::GetComponentArray() const
{
    return RenderDataManager.GetComponentArray();
}

const TArray<FMatrix>& FObjViewerRenderPipeline::GetWorldMatrixArray() const
{
    return RenderDataManager.GetWorldMatrixArray();
}

const TArray<FMatrix>& FObjViewerRenderPipeline::GetInverseWorldMatrixArray() const
{
    return RenderDataManager.GetInverseWorldMatrixArray();
}

bool FObjViewerRenderPipeline::GetMeshCPUDataByIndex(int32 ComponentIndex,
                                                     const TArray<FNormalVertex>*& OutVertices,
                                                     const TArray<uint32>*& OutIndices,
                                                     const FBVH*& OutMeshBVH) const
{
    return RenderDataManager.GetMeshCPUDataByIndex(ComponentIndex, OutVertices, OutIndices,
                                                   OutMeshBVH);
}
