#include "Editor/Animation/AnimationSequencePreviewScene.h"

#include "Animation/AnimData/AnimSequence.h"
#include "Asset/SkeletalMesh.h"
#include "Component/ActorComponent.h"
#include "Component/GizmoComponent.h"
#include "Component/PostProcess/Light/AmbientLightComponent.h"
#include "Component/PrimitiveComponent.h"
#include "Component/SkeletalMeshComponent.h"
#include "Component/TransformProxy.h"
#include "Core/Paths.h"
#include "Core/ResourceManager.h"
#include "Editor/Animation/AnimationSequenceViewerUtils.h"
#include "Editor/EditorEngine.h"
#include "Editor/Selection/SelectionManager.h"
#include "GameFramework/AActor.h"
#include "GameFramework/PrimitiveActors.h"
#include "GameFramework/World.h"
#include "Math/Utils.h"
#include "Object/FName.h"
#include "Render/Renderer/Renderer.h"

#include <algorithm>
#include <filesystem>
#include <functional>
#include <string>

#ifdef GetCurrentTime
#undef GetCurrentTime
#endif

namespace
{
    void SortAndUniqueNames(TArray<FString>& Names)
    {
        std::sort(Names.begin(), Names.end());
        Names.erase(std::unique(Names.begin(), Names.end()), Names.end());
    }
}

FAnimationSequencePreviewScene::~FAnimationSequencePreviewScene()
{
    Shutdown();
}

bool FAnimationSequencePreviewScene::Initialize(
    UEditorEngine* InEditorEngine,
    const FString& InSequencePath,
    UAnimSequence* InSequence,
    const FString& InPreviewMeshPath)
{
    Shutdown();

    EditorEngine = InEditorEngine;
    SequencePath = InSequencePath;
    PreviewMeshPath = InPreviewMeshPath;
    PreviewMesh = FResourceManager::Get().LoadSkeletalMesh(PreviewMeshPath);
    if (!PreviewMesh || !PreviewMesh->HasValidMeshData())
    {
        Shutdown();
        return false;
    }

    if (!InitializePreviewWorld() || !InitializePreviewActor(InSequence))
    {
        Shutdown();
        return false;
    }

    SetViewportSize(ViewportWidth, ViewportHeight);
    return true;
}

void FAnimationSequencePreviewScene::Shutdown()
{
    ClearRecentNotifyHistory();
    PreviewComponent = nullptr;
    PreviewActor = nullptr;
    PreviewMesh = nullptr;
    PreviewMeshPath.clear();

    PreviewViewport.SetRenderTargetSet(nullptr);
    PreviewViewport.SetEditorIdPickActors({});
    PreviewViewport.SetClient(nullptr);
    PreviewViewportClient.SetBonePickHandler(nullptr);
    PreviewViewportClient.DestroyCamera();
    PreviewViewportClient.SetGizmo(nullptr);
    PreviewViewportClient.SetSelectionManager(nullptr);
    PreviewViewportClient.SetViewport(nullptr);
    PreviewViewportClient.SetState(nullptr);
    PreviewViewportClient.SetWorld(nullptr);
    PreviewViewportClient.ReleaseTransientEditorState();

    if (EditorEngine && PreviewResourceIndex != InvalidPreviewResourceIndex)
    {
        EditorEngine->GetRenderer().ReleaseViewerViewportResource(PreviewResourceIndex);
    }
    PreviewResourceIndex = InvalidPreviewResourceIndex;

    if (EditorEngine && PreviewWorldHandle.IsValid())
    {
        if (EditorEngine->GetWorldContextFromHandle(PreviewWorldHandle))
        {
            EditorEngine->DestroyWorldContext(PreviewWorldHandle);
        }
    }

    PreviewWorld = nullptr;
    PreviewWorldHandle = FName();
    SequencePath.clear();
    EditorEngine = nullptr;
}

bool FAnimationSequencePreviewScene::HasValidPreview() const
{
    return AnimationSequenceViewer::IsLiveObject(PreviewMesh) &&
        PreviewWorld != nullptr &&
        AnimationSequenceViewer::IsLiveObject(PreviewComponent);
}

void FAnimationSequencePreviewScene::TickViewportClient(float DeltaTime)
{
    if (!HasValidPreview())
    {
        return;
    }

    PreviewViewportClient.Tick(DeltaTime);
}

void FAnimationSequencePreviewScene::ApplyPlaybackSettings(bool bLooping, float PlayRate, bool bPlaying)
{
    if (!AnimationSequenceViewer::IsLiveObject(PreviewComponent))
    {
        return;
    }

    PreviewComponent->SetPreviewLooping(bLooping);
    PreviewComponent->SetPreviewPlayRate(PlayRate);
    PreviewComponent->SetPreviewPlaying(bPlaying);
}

void FAnimationSequencePreviewScene::SetCurrentTime(float InTime)
{
    if (!AnimationSequenceViewer::IsLiveObject(PreviewComponent))
    {
        return;
    }

    ClearRecentNotifyHistory();
    PreviewComponent->SetPreviewTime(InTime);
    if (PreviewWorld)
    {
        PreviewWorld->SyncSpatialIndex();
    }
}

float FAnimationSequencePreviewScene::GetCurrentTime() const
{
    return AnimationSequenceViewer::IsLiveObject(PreviewComponent) ? PreviewComponent->GetPreviewTime() : 0.0f;
}

float FAnimationSequencePreviewScene::GetPreviewLength() const
{
    return AnimationSequenceViewer::IsLiveObject(PreviewComponent) ? PreviewComponent->GetPreviewLength() : 0.0f;
}

const TArray<FAnimNotifyEvent>& FAnimationSequencePreviewScene::GetRecentFiredNotifyEvents() const
{
    return RecentFiredNotifyHistory;
}

bool FAnimationSequencePreviewScene::HasPreviewSocket(const FName& SocketName) const
{
    // Match runtime AnimNotify attach-target interpretation by delegating to
    // the preview component's HasSocket path, which already treats bone names
    // and socket names uniformly.
    return AnimationSequenceViewer::IsLiveObject(PreviewComponent) &&
        SocketName != FName::None &&
        PreviewComponent->HasSocket(SocketName);
}

TArray<FString> FAnimationSequencePreviewScene::GetPreviewSocketNames() const
{
    TArray<FString> SocketNames;
    if (!AnimationSequenceViewer::IsLiveObject(PreviewComponent))
    {
        return SocketNames;
    }

    USkeletalMesh* SkeletalMesh = PreviewComponent->GetSkeletalMesh();
    if (!(AnimationSequenceViewer::IsLiveObject(SkeletalMesh) && SkeletalMesh->HasValidMeshData()))
    {
        return SocketNames;
    }

    // Keep picker candidates aligned with runtime attach-target resolution:
    // sockets and bones are both valid when the preview component resolves
    // them through HasSocket/GetSocketTransform.
    for (const FSkeletalMeshSocket& Socket : SkeletalMesh->GetSockets())
    {
        if (!Socket.Name.IsValid())
        {
            continue;
        }

        const FString SocketName = Socket.Name.ToString();
        if (!SocketName.empty())
        {
            SocketNames.push_back(SocketName);
        }
    }

    for (const FBoneInfo& Bone : SkeletalMesh->GetBones())
    {
        if (!Bone.Name.IsValid())
        {
            continue;
        }

        const FString BoneName = Bone.Name.ToString();
        if (!BoneName.empty())
        {
            SocketNames.push_back(BoneName);
        }
    }

    SortAndUniqueNames(SocketNames);
    return SocketNames;
}

bool FAnimationSequencePreviewScene::HasPreviewPrimitiveComponent(const FString& ComponentName) const
{
    if (!AnimationSequenceViewer::IsLiveObject(PreviewActor) || ComponentName.empty())
    {
        return false;
    }

    for (UActorComponent* Component : PreviewActor->GetComponents())
    {
        UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(Component);
        if (AnimationSequenceViewer::IsLiveObject(PrimitiveComponent) &&
            PrimitiveComponent->GetName() == ComponentName)
        {
            return true;
        }
    }

    return false;
}

TArray<FString> FAnimationSequencePreviewScene::GetPreviewPrimitiveComponentNames() const
{
    TArray<FString> ComponentNames;
    if (!AnimationSequenceViewer::IsLiveObject(PreviewActor))
    {
        return ComponentNames;
    }

    for (UActorComponent* Component : PreviewActor->GetComponents())
    {
        UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(Component);
        if (!AnimationSequenceViewer::IsLiveObject(PrimitiveComponent))
        {
            continue;
        }

        const FString ComponentName = PrimitiveComponent->GetName();
        if (!ComponentName.empty())
        {
            ComponentNames.push_back(ComponentName);
        }
    }

    SortAndUniqueNames(ComponentNames);
    return ComponentNames;
}

bool FAnimationSequencePreviewScene::SelectPreviewBone(int32 BoneIndex)
{
    if (!(AnimationSequenceViewer::IsLiveObject(PreviewActor) &&
        AnimationSequenceViewer::IsLiveObject(PreviewComponent) &&
        AnimationSequenceViewer::IsLiveObject(PreviewMesh)))
    {
        return false;
    }

    const FSkeletalMesh* MeshData = PreviewMesh->GetMeshData();
    if (!MeshData || BoneIndex < 0 || BoneIndex >= static_cast<int32>(MeshData->GetBones().size()))
    {
        return false;
    }

    PreviewComponent->EnsureSkinningUpdated();

    if (FSelectionManager* SelectionManager = PreviewViewportClient.GetSelectionManager())
    {
        SelectionManager->Select(PreviewActor);
    }

    if (UGizmoComponent* Gizmo = PreviewViewportClient.GetGizmo())
    {
        Gizmo->SetProxy(std::make_shared<FBoneTransformProxy>(PreviewComponent, BoneIndex));
        Gizmo->SetSelectedActors(nullptr);
    }

    return true;
}

void FAnimationSequencePreviewScene::ClearPreviewSelection()
{
    if (FSelectionManager* SelectionManager = PreviewViewportClient.GetSelectionManager())
    {
        SelectionManager->ClearSelection();
        return;
    }

    if (UGizmoComponent* Gizmo = PreviewViewportClient.GetGizmo())
    {
        Gizmo->Deactivate();
    }
}

void FAnimationSequencePreviewScene::RefreshPreviewPose(float DeltaTime)
{
    if (!AnimationSequenceViewer::IsLiveObject(PreviewComponent))
    {
        return;
    }

    PreviewComponent->TickPreviewAnimation(DeltaTime);
    CaptureRecentNotifyEvents();
    if (PreviewWorld)
    {
        PreviewWorld->SyncSpatialIndex();
    }
}

void FAnimationSequencePreviewScene::AdvancePreviewPlayback(
    float InTime,
    float DeltaTime,
    bool bWrapped,
    float RangeStart,
    float RangeEnd)
{
    if (!AnimationSequenceViewer::IsLiveObject(PreviewComponent))
    {
        return;
    }

    PreviewComponent->AdvancePreviewAnimation(InTime, DeltaTime, bWrapped, RangeStart, RangeEnd);
    CaptureRecentNotifyEvents();
    if (PreviewWorld)
    {
        PreviewWorld->SyncSpatialIndex();
    }
}

void FAnimationSequencePreviewScene::SetViewportSize(int32 InWidth, int32 InHeight)
{
    ViewportWidth = std::max(InWidth, 1);
    ViewportHeight = std::max(InHeight, 1);

    FViewportRect ViewportRect = {};
    ViewportRect.X = 0;
    ViewportRect.Y = 0;
    ViewportRect.Width = ViewportWidth;
    ViewportRect.Height = ViewportHeight;

    PreviewViewport.SetRect(ViewportRect);
    PreviewViewportClient.SetViewportSize(static_cast<float>(ViewportWidth), static_cast<float>(ViewportHeight));

    if (!EditorEngine || !HasValidPreview())
    {
        PreviewViewport.SetRenderTargetSet(nullptr);
        return;
    }

    // The preview scene owns the offscreen viewer resource. The embedded ImGui
    // panel only mirrors this rect; it does not own the render target lifecycle.
    // Use a separate index range from standalone skeletal mesh viewers so
    // opening a viewer for the same mesh cannot steal this preview render target.
    if (PreviewResourceIndex == InvalidPreviewResourceIndex)
    {
        PreviewResourceIndex = FirstEmbeddedPreviewResourceIndex + NextPreviewResourceIndex++;
    }

    FViewportRenderResource& Resource =
        EditorEngine->GetRenderer().AcquireViewerViewportResource(
            PreviewResourceIndex,
            static_cast<uint32>(ViewportWidth),
            static_cast<uint32>(ViewportHeight));
    PreviewViewport.SetRenderTargetSet(&Resource.GetView());
}

ID3D11ShaderResourceView* FAnimationSequencePreviewScene::GetPreviewSRV() const
{
    return PreviewViewport.GetOutSRV();
}

bool FAnimationSequencePreviewScene::InitializePreviewWorld()
{
    if (!EditorEngine)
    {
        return false;
    }

    const size_t PathHash = std::hash<FString>{}(SequencePath);
    PreviewWorldHandle = FName(("__AnimSequencePreview_" + std::to_string(PathHash)).c_str());
    FWorldContext& PreviewContext =
        EditorEngine->CreateWorldContext(EWorldType::ViewerPreview, PreviewWorldHandle, "Animation Sequence Preview");
    PreviewContext.bPaused = true;
    EditorEngine->ApplySpatialIndexMaintenanceSettings(PreviewContext.World);
    PreviewWorld = PreviewContext.World;
    if (!PreviewWorld)
    {
        return false;
    }

    InitializeViewportClient();
    if (PreviewContext.SelectionManager)
    {
        PreviewViewportClient.SetGizmo(PreviewContext.SelectionManager->GetGizmo());
        PreviewViewportClient.SetSelectionManager(PreviewContext.SelectionManager);
    }

    ADirectionalLightActor* DirectionalLight = PreviewWorld->SpawnActor<ADirectionalLightActor>();
    if (DirectionalLight)
    {
        DirectionalLight->InitDefaultComponents();
        DirectionalLight->SetFName(FName("Anim Preview Directional Light"));
        DirectionalLight->SetActorLocation(FVector(100000.0f, 100000.0f, 100000.0f));
        DirectionalLight->SetActorRotation(FVector(0.0f, 44.0f, 0.0f));
    }

    AAmbientLightActor* AmbientLight = PreviewWorld->SpawnActor<AAmbientLightActor>();
    if (AmbientLight)
    {
        AmbientLight->InitDefaultComponents();
        AmbientLight->SetFName(FName("Anim Preview Ambient Light"));
        AmbientLight->SetActorLocation(FVector(100000.0f, 100000.0f, 100000.0f));

        if (UAmbientLightComponent* AmbientLightComponent = AmbientLight->FindComponent<UAmbientLightComponent>())
        {
            AmbientLightComponent->Intensity = 0.7f;
        }
    }

    return true;
}

bool FAnimationSequencePreviewScene::InitializePreviewActor(UAnimSequence* Sequence)
{
    if (!PreviewWorld || !PreviewMesh)
    {
        return false;
    }

    PreviewActor = PreviewWorld->SpawnActor<ASkeletalMeshActor>();
    if (!PreviewActor)
    {
        return false;
    }

    PreviewActor->InitDefaultComponents();
    PreviewActor->SetFName(FName("AnimationSequencePreviewActor"));
    PreviewActor->SetActorLocation(FVector::ZeroVector);

    PreviewComponent = PreviewActor->GetSkeletalMeshComponent();
    if (!PreviewComponent)
    {
        return false;
    }

    PreviewComponent->SetSkeletalMesh(PreviewMesh);
    PreviewComponent->SetPreviewSequence(Sequence);
    PreviewComponent->SetPreviewLooping(true);
    PreviewComponent->SetPreviewPlayRate(1.0f);
    PreviewComponent->SetPreviewPlaying(false);
    PreviewComponent->EnsureSkinningUpdated();
    ClearRecentNotifyHistory();

    PreviewWorld->SyncSpatialIndex();
    ConfigurePreviewCamera();
    return true;
}

void FAnimationSequencePreviewScene::ClearRecentNotifyHistory()
{
    RecentFiredNotifyHistory.clear();
}

void FAnimationSequencePreviewScene::CaptureRecentNotifyEvents()
{
    if (!AnimationSequenceViewer::IsLiveObject(PreviewComponent))
    {
        return;
    }

    const TArray<FAnimNotifyEvent>& FrameEvents = PreviewComponent->GetRecentFiredNotifyEvents();
    if (FrameEvents.empty())
    {
        return;
    }

    for (const FAnimNotifyEvent& NotifyEvent : FrameEvents)
    {
        FAnimNotifyEvent RuntimeEvent = NotifyEvent;
        if (RuntimeEvent.SourceSequencePath.empty() && !SequencePath.empty())
        {
            RuntimeEvent.SourceSequencePath = FPaths::ToProjectRelativePath(SequencePath);
        }

        if (RuntimeEvent.SourceSequenceName.empty() && !SequencePath.empty())
        {
            RuntimeEvent.SourceSequenceName =
                FPaths::ToString(std::filesystem::path(FPaths::ToWide(SequencePath)).stem().wstring());
        }

        RecentFiredNotifyHistory.push_back(RuntimeEvent);
    }

    if (static_cast<int32>(RecentFiredNotifyHistory.size()) <= MaxRecentNotifyHistoryCount)
    {
        return;
    }

    const int32 OverflowCount =
        static_cast<int32>(RecentFiredNotifyHistory.size()) - MaxRecentNotifyHistoryCount;
    RecentFiredNotifyHistory.erase(
        RecentFiredNotifyHistory.begin(),
        RecentFiredNotifyHistory.begin() + OverflowCount);
}

void FAnimationSequencePreviewScene::InitializeViewportClient()
{
    PreviewViewport.SetClient(&PreviewViewportClient);

    PreviewViewportClient.Initialize(EditorEngine->GetWindow(), EditorEngine);
    PreviewViewportClient.SetWorld(PreviewWorld);
    PreviewViewportClient.SetViewport(&PreviewViewport);
    PreviewViewportClient.SetState(&PreviewViewport.GetState());
    PreviewViewportClient.SetSceneEditingShortcutsEnabled(false);
    PreviewViewportClient.SetViewportType(EEditorViewportType::EVT_Perspective);
    PreviewViewportClient.CreateCamera();
    PreviewViewportClient.ApplyCameraMode();

    PreviewViewport.GetState().ViewMode = EViewMode::Lit_BlinnPhong;
    PreviewViewport.GetState().LightCullMode = ELightCullMode::None;
}

void FAnimationSequencePreviewScene::ConfigurePreviewCamera()
{
    if (!PreviewMesh)
    {
        return;
    }

    FViewportCamera* Camera = PreviewViewportClient.GetCamera();
    if (!Camera)
    {
        return;
    }

    const FAABB& MeshBounds = PreviewMesh->GetLocalBounds();
    const FVector Center = MeshBounds.GetCenter();
    const float Radius = std::max(MeshBounds.GetExtent().Size(), 40.0f);
    const float Distance = std::max(Radius * 2.25f, 160.0f);
    constexpr float DegreesToRadians = 3.14159265358979323846f / 180.0f;

    Camera->SetLookAt(Center);
    Camera->SetLocation(Center + FVector(Distance, Distance * 0.35f, Radius * 0.75f));
    // Keep the embedded animation preview near clip consistent with the
    // regular skeletal mesh viewer so close-up inspection does not clip away
    // the front surface and reveal occluded bones/meshes behind it.
    Camera->SetNearPlane(0.1f);
    Camera->SetFarPlane(std::max(5000.0f, Distance * 8.0f));
    Camera->SetFOV(60.0f * DegreesToRadians);
}
