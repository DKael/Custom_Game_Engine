#include "EditorRenderPipeline.h"

#include "Editor/EditorEngine.h"
#include "Camera/PlayerCameraManager.h"
#include "Camera/ViewportCamera.h"
#include "Render/Renderer/Renderer.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/World.h"
#include "Core/Logging/Stats.h"
#include "Core/Logging/GPUProfiler.h"
#include "Core/Logging/Log.h"
#include "Runtime/SceneView.h"
#include "Engine/Component/GizmoComponent.h"
#include "Engine/Component/SkeletalMeshComponent.h"
#include "GameFramework/PrimitiveActors.h"
#include "Asset/StaticMesh.h"
#include "Render/Resource/Buffer.h"
#include "Render/Resource/Material.h"
#include "Render/Scene/RenderCommand.h"
#include "Math/Utils.h"

#include <algorithm>
#include <chrono>
#include <cstring>

namespace
{
    void ApplyPIECameraViewEffectsToBus(APlayerController* PlayerController, FRenderBus& Bus)
    {
        APlayerCameraManager* CameraManager = PlayerController
            ? PlayerController->GetPlayerCameraManager()
            : nullptr;
        if (!CameraManager)
        {
            return;
        }

        const FMinimalViewInfo& ViewInfo = CameraManager->GetCameraView();
        const FCameraPostProcessSettings& PostProcess = ViewInfo.PostProcessSettings;
        if (PostProcess.bVignetteEnabled)
        {
            Bus.SetVignette(
                PostProcess.VignetteIntensity,
                PostProcess.VignetteRadius,
                PostProcess.VignetteSmoothness,
                PostProcess.VignetteColor);
        }
        if (CameraManager->HasLetterbox())
        {
            Bus.SetLetterbox(CameraManager->GetLetterboxTargetAspect(), CameraManager->GetLetterboxAmount());
        }
        if (CameraManager->HasVisibleFade())
        {
            Bus.SetCameraFade(CameraManager->GetFadeColor().ToVector4(), CameraManager->GetFadeAlpha());
        }
    }
}

FEditorRenderPipeline::FEditorRenderPipeline(UEditorEngine* InEditor, FRenderer& InRenderer) : Editor(InEditor)
{
    Collector.Initialize(InRenderer.GetFD3DDevice().GetDevice());
    ViewportCullingStats.resize(FEditorViewportLayout::MaxViewports);
	ViewportDecalStats.resize(FEditorViewportLayout::MaxViewports);
	ViewportLightStats.resize(FEditorViewportLayout::MaxViewports);
}

FEditorRenderPipeline::~FEditorRenderPipeline() { Collector.Release(); }

void FEditorRenderPipeline::Execute(float DeltaTime, FRenderer& Renderer)
{
    const auto PipelineStart = std::chrono::steady_clock::now();
#if STATS
    FStatManager::Get().TakeSnapshot();
    FGPUProfiler::Get().TakeSnapshot();
#endif
    for (FRenderCollector::FCullingStats& Stats : ViewportCullingStats)
    {
        Stats = {};
    }

    if (!Editor->GetFocusedWorld())
        return;

    // 1회: 전체 백버퍼 클리어 (색상 + 깊이/스텐실)
    Renderer.BeginFrame();

    {
        GPU_SCOPE_STAT("EditorFrameGPU");

        // 4개 뷰포트를 순서대로 렌더링
        for (int32 i = 0; i < FEditorViewportLayout::MaxViewports; ++i)
        {
            RenderViewport(Renderer, i);
        }

		RenderViewerViewport(Renderer);
        RenderActiveDocumentViewport(Renderer, DeltaTime);
        Renderer.UseBackBufferRenderTargets();

        // ImGui UI 오버레이
        Editor->RenderUI(DeltaTime);
    }

    Renderer.EndFrame();
}

void FEditorRenderPipeline::RenderViewport(FRenderer& Renderer, int32 ViewportIndex)
{
    const auto ViewportRenderStart = std::chrono::steady_clock::now();
    if (Editor->GetEditorState() != EViewportPlayState::Editing)
    {
        const int32 ActivePIEViewportIndex = Editor->GetPIESession().GetActiveViewportIndex();
        if (ActivePIEViewportIndex >= 0 && ViewportIndex != ActivePIEViewportIndex)
        {
            return;
        }
    }

    FEditorViewportClient* VC = Editor->GetViewportLayout().GetViewportClient(ViewportIndex);

    // 1. 이 뷰포트의 SceneView 빌드
    //    - ViewRect : 화면 내 서브 영역 (BuildSceneView가 State->Rect에서 채움)
    //    - ViewMode : 뷰포트별 독립 모드 (기본값 EViewMode::Lit)
    FSceneView SceneView;
    VC->BuildSceneView(SceneView);

    // 2. 렌더링 대상을 서브 영역으로 제한
    const FViewportRect& Rect = SceneView.ViewRect;
    if (Rect.Width <= 0 || Rect.Height <= 0)
        return;

    // Avoid recreating the full viewport render-target stack every frame while
    // layout animation is resizing panes. The UI can scale the last SRV until
    // the transition lands, then we render once at the final size.
    if (Editor->GetViewportLayout().IsLayoutTransitionActive())
        return;

    FSceneViewport& SceneViewport = Editor->GetViewportLayout().GetSceneViewport(ViewportIndex);
    
	// Width, Height 변경 여부에 따라 Resource 버퍼 재생성
	// 만약 최소화 등의 상황으로 (H, W) == (0, 0) 일 경우 Render 안함
	FViewportRenderResource& ViewportResource = Editor->GetRenderer().AcquireViewportResource(Rect.Width, Rect.Height, ViewportIndex);
    SceneViewport.SetRenderTargetSet(&ViewportResource.GetView());

    // Viewport 별 버퍼 클리어 및 Renderer 버퍼 세팅
    Renderer.BeginViewportFrame(SceneViewport.GetViewportRenderTargets());

    // 3. 이 뷰포트용 렌더 데이터 수집
    Bus.Clear();

    // 각 뷰포트는 자신이 참조하는 월드를 렌더링합니다.
    UWorld*                World = VC->GetFocusedWorld();
    const FEditorSettings& Settings = Editor->GetSettings();
    const FShowFlags&      ShowFlags = Settings.ShowFlags;
    const EViewMode        ViewMode = SceneView.ViewMode;

    const FViewportCamera* Camera = VC->GetRenderCamera();
    if (Camera == nullptr)
        return;

    Bus.SetViewProjection(SceneView.ViewMatrix, SceneView.ProjectionMatrix, Camera->GetNearPlane(), Camera->GetFarPlane());
    Bus.SetRenderSettings(ViewMode, ShowFlags);
    Bus.SetLightCullMode(SceneView.LightCullMode);
    Bus.SetShadowFilterMode(Settings.ShadowFilterMode);
	Bus.SetViewportSize(FVector2(static_cast<float>(Rect.Width), static_cast<float>(Rect.Height)));
    Bus.SetViewportOrigin(FVector2(0.0f, 0.0f));
    Bus.SetFXAAEnabled(Settings.bEnableFXAA && !SceneView.bOrthographic);
    Bus.SetCascadeVis(VC->GetViewportState()->bShowCascadeVis);
    if (Editor->GetEditorState() != EViewportPlayState::Editing)
    {
        ApplyPIECameraViewEffectsToBus(VC->GetPIEPlayerController(), Bus);
    }

	if (World->IsSandervistanActivated())
	{
        Bus.bSandevistanEnabled = true;
        Bus.SandevistanIntensity = 1.0f;
	}
	else
	{
        Bus.bSandevistanEnabled = false;
        Bus.SandevistanIntensity = 0.0f;
	}
	
	const FFrustum& ViewFrustum = SceneView.CameraFrustum;
	const bool bDrawEditorViewportHelpers = VC->AllowsEditorWorldControl();
	Collector.CollectWorld(World, ShowFlags, ViewMode, Bus, &ViewFrustum, bDrawEditorViewportHelpers);
    ViewportCullingStats[ViewportIndex] = Collector.GetLastCullingStats();
    ViewportDecalStats[ViewportIndex] = Collector.GetLastDecalStats();
    ViewportLightStats[ViewportIndex] = Collector.GetLastLightStats();

	// 순수 편집 뷰포트와 PIE Eject는 모두 Editor viewport setting을 따릅니다.
	if (bDrawEditorViewportHelpers)
    {
        Collector.CollectGrid(Settings.GridSpacing, Settings.GridHalfLineCount, Bus, SceneView.bOrthographic);
        const FWorldContext* Ctx = Editor->GetWorldContextFromWorld(World);
        if (UGizmoComponent* Gizmo = Ctx->SelectionManager->GetGizmo())
        {
            if (SceneView.bOrthographic)
                Gizmo->ApplyScreenSpaceScalingOrtho(SceneView.CameraOrthoHeight);
            else
                Gizmo->ApplyScreenSpaceScaling(SceneView.CameraPosition);
        }

        Collector.CollectGizmo(Ctx->SelectionManager->GetGizmo(), ShowFlags, Bus, VC->GetViewportState()->bHovered);
        Collector.CollectSelection(Ctx->SelectionManager->GetSelectedActors(), ShowFlags, ViewMode, Bus, bDrawEditorViewportHelpers);
    }

    // 4. CPU 배처 데이터 준비 → GPU 드로우 (SetSubViewport 영역에만 출력됨)
    Renderer.PrepareBatchers(Bus);
    Renderer.Render(Bus);
    Renderer.RenderScreenOverlays(Bus, false);

    TArray<AActor*> IdPickActors;
    Renderer.RenderEditorIdPickBuffer(Bus, ViewportResource, IdPickActors);
    SceneViewport.SetEditorIdPickActors(std::move(IdPickActors));
}

void FEditorRenderPipeline::RenderViewerViewport(FRenderer& Renderer)
{
    TArray<std::unique_ptr<FEditorViewer>>& Viewers = Editor->GetViewers();

	for (size_t i = 0; i < Viewers.size(); i++)
	{
        const auto ViewportRenderStart = std::chrono::steady_clock::now();

        FSceneViewport& SceneViewport = Viewers[i]->GetViewport();
        FEditorViewportClient* VC = SceneViewport.GetClient();

        if (!VC)
            continue;

        // 1. SceneView 생성
        FSceneView SceneView;
        VC->BuildSceneView(SceneView);

        const FViewportRect& Rect = SceneViewport.GetRect();
        if (Rect.Width <= 0 || Rect.Height <= 0)
            continue;

        // 2. RenderTarget 확보
        FViewportRenderResource& ViewportResource =
            Editor->GetRenderer().AcquireViewerViewportResource(
                (uint32)i,
                Rect.Width,
                Rect.Height);

        SceneViewport.SetRenderTargetSet(&ViewportResource.GetView());

        // 3. Begin
        Renderer.BeginViewportFrame(SceneViewport.GetViewportRenderTargets());

        // 4. Bus 세팅
        Bus.Clear();

        UWorld* World = VC->GetFocusedWorld();
        if (!World)
            continue;

        const FEditorSettings& Settings = Editor->GetSettings();
        const FSkeletalViewerShowFlags& VFlags = Viewers[i]->GetClient().GetShowFlags();

        // ViewerPreview는 Level Editor의 전역 표시 상태를 상속하지 않는다.
        // Asset 검사 도구로서 필요한 표면/오버레이만 명시적으로 켠다.
        FShowFlags ShowFlags = {};
        ShowFlags.bPrimitives = true;
        ShowFlags.bSkeletalMesh = VFlags.bShowSkeletalMesh;
        ShowFlags.bGrid = true;
        ShowFlags.bAxis = true;
        ShowFlags.bGizmo = true;
        ShowFlags.bBillboardText = false;
        ShowFlags.bBoundingVolume = VFlags.bShowBoundingBox;
        ShowFlags.bBVHBoundingVolume = false;
        ShowFlags.bEnableLOD = false;
        ShowFlags.bDecals = false;
        ShowFlags.bFog = false;
        ShowFlags.bShadow = false;
        ShowFlags.bGammaCorrection = false;
        ShowFlags.GammaValue = Settings.ShowFlags.GammaValue;
        ShowFlags.bShowSelectedBoneWeight = VFlags.bShowSelectedBoneWeight;
        ShowFlags.SelectedBoneIndex = Viewers[i]->GetSelectedBoneIndex();
        const FEditorViewportState* ViewportState = VC->GetViewportState();
        const EViewMode ViewMode = ViewportState ? ViewportState->ViewMode : EViewMode::Lit_BlinnPhong;

        const FViewportCamera* Camera = VC->GetRenderCamera();
        if (!Camera)
            continue;

        Bus.SetViewProjection(
            SceneView.ViewMatrix,
            SceneView.ProjectionMatrix,
            Camera->GetNearPlane(),
            Camera->GetFarPlane());

        Bus.SetRenderSettings(ViewMode, ShowFlags);
        Bus.SetLightCullMode(ViewportState ? ViewportState->LightCullMode : ELightCullMode::None);
        Bus.SetShadowFilterMode(Settings.ShadowFilterMode);
        Bus.SetViewportSize(FVector2((float)Rect.Width, (float)Rect.Height));
        Bus.SetViewportOrigin(FVector2(0.0f, 0.0f));
        Bus.SetFXAAEnabled(Settings.bEnableFXAA && !SceneView.bOrthographic);
        Bus.SetCascadeVis(ViewportState ? ViewportState->bShowCascadeVis : false);

        // Sandevistan 유지
        if (World->IsSandervistanActivated())
        {
            Bus.bSandevistanEnabled = true;
            Bus.SandevistanIntensity = 1.0f;
        }
        else
        {
            Bus.bSandevistanEnabled = false;
            Bus.SandevistanIntensity = 0.0f;
        }

        // 5. Collect
        const FFrustum& ViewFrustum = SceneView.CameraFrustum;
        const bool bDrawEditorViewportHelpers =
            VC->AllowsEditorWorldControl();

        Collector.CollectWorld(
            World,
            ShowFlags,
            ViewMode,
            Bus,
            &ViewFrustum,
            bDrawEditorViewportHelpers);

        // 🔹 Editor helper 그대로 유지
        if (bDrawEditorViewportHelpers)
        {
            Collector.CollectGrid(
                Settings.GridSpacing,
                Settings.GridHalfLineCount,
                Bus,
                SceneView.bOrthographic);

            const FWorldContext* Ctx = Editor->GetWorldContextFromWorld(World);
            if (UGizmoComponent* Gizmo = Ctx->SelectionManager->GetGizmo())
            {
                if (SceneView.bOrthographic)
                    Gizmo->ApplyScreenSpaceScalingOrtho(SceneView.CameraOrthoHeight);
                else
                    Gizmo->ApplyScreenSpaceScaling(SceneView.CameraPosition);
            }

            Collector.CollectGizmo(
                Ctx->SelectionManager->GetGizmo(),
                ShowFlags,
                Bus,
                VC->GetViewportState()->bHovered);

            if (VFlags.bShowOutline)
            {
                Collector.CollectSelection(
                    Ctx->SelectionManager->GetSelectedActors(),
                    ShowFlags,
                    ViewMode,
                    Bus,
                    bDrawEditorViewportHelpers);
            }
        }

        // Viewer 전용: 본 와이어 드로우 (skeleton mesh viewer 토글)
        {
            if (VFlags.bShowBones)
            {
                if (ASkeletalMeshActor* ViewTarget = Viewers[i]->GetViewTarget())
                {
                    if (USkeletalMeshComponent* SkComp = ViewTarget->GetSkeletalMeshComponent())
                    {
                        SkComp->EnsureSkinningUpdated();   // 본 자세 최신화 보장
                        if (VFlags.bShowOnlySelectedBone)
                        {
                            const int32 BoneIdx = Viewers[i]->GetSelectedBoneIndex();
                            if (BoneIdx >= 0)
                            {
                                Collector.CollectSingleBone(SkComp, BoneIdx, Bus);
                            }
                        }
                        else
                        {
                            Collector.CollectSkeletonBones(SkComp, Bus);
                        }
                    }
                }
            }
        }

        if (VFlags.bShowBoundingBox)
        {
            if (ASkeletalMeshActor* ViewTarget = Viewers[i]->GetViewTarget())
            {
                if (USkeletalMeshComponent* SkComp = ViewTarget->GetSkeletalMeshComponent())
                {
                    FRenderCommand BoundsCmd = {};
                    const FAABB Bounds = SkComp->GetWorldAABB();
                    BoundsCmd.Type = ERenderCommandType::DebugBox;
                    BoundsCmd.Constants.AABB.Min = Bounds.Min;
                    BoundsCmd.Constants.AABB.Max = Bounds.Max;
                    BoundsCmd.Constants.AABB.Color = FColor::White();
                    Bus.AddCommand(ERenderPass::Editor, BoundsCmd);
                }
            }
        }

        // 6. Draw
        Renderer.PrepareBatchers(Bus);
        Renderer.Render(Bus);
        Renderer.RenderScreenOverlays(Bus, false);

        // 7. ID Pick
        TArray<AActor*> IdPickActors;
        Renderer.RenderEditorIdPickBuffer(
            Bus,
            ViewportResource,
            IdPickActors);

        SceneViewport.SetEditorIdPickActors(std::move(IdPickActors));
	}
}

void FEditorRenderPipeline::RenderActiveDocumentViewport(FRenderer& Renderer, float DeltaTime)
{
    FEditorDocument* ActiveDocument = Editor ? Editor->GetMainPanel().GetActiveEditorDocument() : nullptr;
    if (!ActiveDocument)
    {
        return;
    }

    FSceneViewport* SceneViewport = ActiveDocument->GetSceneViewport();
    if (!SceneViewport)
    {
        return;
    }

    ActiveDocument->Tick(DeltaTime);

    FEditorViewportClient* VC = SceneViewport->GetClient();
    if (!VC)
    {
        return;
    }

    FSceneView SceneView;
    VC->BuildSceneView(SceneView);

    const FViewportRect& Rect = SceneViewport->GetRect();
    if (Rect.Width <= 0 || Rect.Height <= 0)
    {
        return;
    }

    Renderer.BeginViewportFrame(SceneViewport->GetViewportRenderTargets());

    Bus.Clear();

    UWorld* World = VC->GetFocusedWorld();
    if (!World)
    {
        return;
    }

    FShowFlags ShowFlags = {};
    ShowFlags.bPrimitives = true;
    ShowFlags.bSkeletalMesh = true;
    ShowFlags.bGrid = false;
    ShowFlags.bAxis = false;
    ShowFlags.bGizmo = true;
    ShowFlags.bBillboardText = false;
    ShowFlags.bBoundingVolume = false;
    ShowFlags.bBVHBoundingVolume = false;
    ShowFlags.bEnableLOD = false;
    ShowFlags.bDecals = false;
    ShowFlags.bFog = false;
    ShowFlags.bShadow = false;
    ShowFlags.bGammaCorrection = false;

    FEditorSkeletalPreviewDebugSettings SkeletalPreviewDebugSettings;
    const bool bHasSkeletalPreviewDebugSettings =
        ActiveDocument->GetSkeletalPreviewDebugSettings(SkeletalPreviewDebugSettings);
    if (bHasSkeletalPreviewDebugSettings)
    {
        ShowFlags.bShowSelectedBoneWeight = SkeletalPreviewDebugSettings.bShowSelectedBoneWeight;
        ShowFlags.SelectedBoneIndex = SkeletalPreviewDebugSettings.SelectedBoneIndex;
    }

    const FViewportCamera* Camera = VC->GetRenderCamera();
    if (!Camera)
    {
        return;
    }

    Bus.SetViewProjection(
        SceneView.ViewMatrix,
        SceneView.ProjectionMatrix,
        Camera->GetNearPlane(),
        Camera->GetFarPlane());
    Bus.SetRenderSettings(EViewMode::Lit_BlinnPhong, ShowFlags);
    Bus.SetLightCullMode(ELightCullMode::None);
    Bus.SetShadowFilterMode(Editor->GetSettings().ShadowFilterMode);
    Bus.SetViewportSize(FVector2(static_cast<float>(Rect.Width), static_cast<float>(Rect.Height)));
    Bus.SetViewportOrigin(FVector2(0.0f, 0.0f));
    Bus.SetFXAAEnabled(Editor->GetSettings().bEnableFXAA && !SceneView.bOrthographic);
    Bus.SetCascadeVis(false);

    const FFrustum& ViewFrustum = SceneView.CameraFrustum;
    Collector.CollectWorld(World, ShowFlags, EViewMode::Lit_BlinnPhong, Bus, &ViewFrustum, false);

    if (UGizmoComponent* Gizmo = VC->GetGizmo())
    {
        if (SceneView.bOrthographic)
        {
            Gizmo->ApplyScreenSpaceScalingOrtho(SceneView.CameraOrthoHeight);
        }
        else
        {
            Gizmo->ApplyScreenSpaceScaling(SceneView.CameraPosition);
        }
    }

    Collector.CollectGizmo(
        VC->GetGizmo(),
        ShowFlags,
        Bus,
        VC->GetViewportState() ? VC->GetViewportState()->bHovered : false);

    if (bHasSkeletalPreviewDebugSettings &&
        SkeletalPreviewDebugSettings.bShowBones &&
        SkeletalPreviewDebugSettings.SkeletalMeshComponent)
    {
        SkeletalPreviewDebugSettings.SkeletalMeshComponent->EnsureSkinningUpdated();
        if (SkeletalPreviewDebugSettings.bShowOnlySelectedBone)
        {
            if (SkeletalPreviewDebugSettings.SelectedBoneIndex >= 0)
            {
                Collector.CollectSingleBone(
                    SkeletalPreviewDebugSettings.SkeletalMeshComponent,
                    SkeletalPreviewDebugSettings.SelectedBoneIndex,
                    Bus);
            }
        }
        else
        {
            Collector.CollectSkeletonBones(SkeletalPreviewDebugSettings.SkeletalMeshComponent, Bus);
        }
    }

    Renderer.PrepareBatchers(Bus);
    Renderer.Render(Bus);
}

const FRenderCollector::FCullingStats& FEditorRenderPipeline::GetViewportCullingStats(int32 ViewportIndex) const
{
    static const FRenderCollector::FCullingStats EmptyStats{};

    if (ViewportIndex < 0 || ViewportIndex >= static_cast<int32>(ViewportCullingStats.size()))
    {
        return EmptyStats;
    }

    return ViewportCullingStats[ViewportIndex];
}

const FRenderCollector::FDecalStats& FEditorRenderPipeline::GetViewportDecalStats(int32 ViewportIndex) const
{
	static const FRenderCollector::FDecalStats EmptyStats{};

	if (ViewportIndex < 0 || ViewportIndex >= static_cast<int32>(ViewportDecalStats.size()))
	{
		return EmptyStats;
	}

	return ViewportDecalStats[ViewportIndex];
}

const FRenderCollector::FLightStats& FEditorRenderPipeline::GetViewportLightStats(int32 ViewportIndex) const
{
	static const FRenderCollector::FLightStats EmptyStats{};

	if (ViewportIndex < 0 || ViewportIndex >= static_cast<int32>(ViewportLightStats.size()))
	{
		return EmptyStats;
	}

	return ViewportLightStats[ViewportIndex];
}

ID3D11ShaderResourceView* FEditorRenderPipeline::RenderMaterialPreview(
	FRenderer& Renderer,
	UStaticMesh* Mesh,
	UMaterialInterface* Material,
	uint32 Width,
	uint32 Height,
	float YawRad,
	float PitchRad,
	float Distance)
{
	if (Mesh == nullptr || Material == nullptr || !Mesh->HasValidMeshData() || Width == 0 || Height == 0)
	{
		return nullptr;
	}

	FMeshBuffer* MeshBuffer = Collector.GetStaticMeshBuffer(Mesh, 0);
	const FStaticMesh* MeshData = Mesh->GetMeshData(0);
	if (MeshBuffer == nullptr || MeshData == nullptr || MeshData->Indices.empty())
	{
		return nullptr;
	}

	FViewportRenderResource& PreviewTarget = Renderer.AcquirePreviewResource(Width, Height);
	if (!PreviewTarget.GetView().IsValid())
	{
		return nullptr;
	}

	Renderer.BeginViewportFrame(PreviewTarget.GetView());

	Bus.Clear();
	Bus.SetRenderSettings(EViewMode::Lit_BlinnPhong, FShowFlags{});
	Bus.SetLightCullMode(ELightCullMode::None);
	Bus.SetViewportSize(FVector2(static_cast<float>(Width), static_cast<float>(Height)));
	Bus.SetViewportOrigin(FVector2(0.0f, 0.0f));
	Bus.SetFXAAEnabled(true);

	const float ClampedPitch = MathUtil::Clamp(PitchRad, -1.35f, 1.35f);
	const float SafeDistance = std::max(Distance, 0.8f);
	const FVector Eye(SafeDistance, -SafeDistance, SafeDistance * 0.45f);
	const FVector Target = FVector::ZeroVector;
	const FMatrix View = FMatrix::MakeViewLookAtLH(Eye, Target, FVector::UpVector);
	const FMatrix Proj = FMatrix::MakePerspectiveFovLH(MathUtil::DegreesToRadians(45.0f),
		static_cast<float>(Width) / static_cast<float>(Height), 0.05f, 100.0f);
	Bus.SetViewProjection(View, Proj, 0.05f, 100.0f);

	Bus.AmbientLightInfo.Color = FVector(1.0f, 1.0f, 1.0f);
	Bus.AmbientLightInfo.Intensity = 0.25f;
	Bus.DirectionalLightInfo.Color = FVector(1.0f, 0.96f, 0.90f);
	Bus.DirectionalLightInfo.Intensity = 1.5f;
	Bus.DirectionalLightInfo.Direction = FVector(-0.55f, -0.35f, -0.75f).GetSafeNormal();

	const FAABB& Bounds = Mesh->GetLocalBounds();
	const FVector Center = Bounds.GetCenter();
	const FVector Extent = Bounds.GetExtent();
	const float MaxExtent = std::max(0.01f, std::max(Extent.X, std::max(Extent.Y, Extent.Z)));
	const float Scale = 1.35f / MaxExtent;
	const FMatrix CenterToOrigin = FMatrix::MakeTranslationMatrix(FVector(-Center.X, -Center.Y, -Center.Z));
	const FMatrix ScaleMatrix = FMatrix::MakeScaleMatrix(FVector(Scale, Scale, Scale));
	const FMatrix Rotation = FMatrix::MakeRotationY(ClampedPitch) * FMatrix::MakeRotationZ(YawRad);
	const FMatrix World = CenterToOrigin * ScaleMatrix * Rotation;

	if (!MeshData->Sections.empty())
	{
		for (const FStaticMeshSection& Section : MeshData->Sections)
		{
			if (Section.IndexCount == 0)
			{
				continue;
			}

			FRenderCommand Cmd = {};
			Cmd.Type = ERenderCommandType::StaticMesh;
			Cmd.VertexFactoryType = EVertexFactoryType::StaticMesh;
			Cmd.MeshBuffer = MeshBuffer;
			Cmd.Material = Material;
			Cmd.SectionIndexStart = Section.StartIndex;
			Cmd.SectionIndexCount = Section.IndexCount;
			Cmd.PerObjectConstants = FPerObjectConstants(World, FColor::White().ToVector4());
			Cmd.WorldAABB = FAABB::TransformAABB(Bounds, World);
			Bus.AddCommand(ERenderPass::Opaque, Cmd);
		}
	}
	else
	{
		FRenderCommand Cmd = {};
		Cmd.Type = ERenderCommandType::StaticMesh;
		Cmd.VertexFactoryType = EVertexFactoryType::StaticMesh;
		Cmd.MeshBuffer = MeshBuffer;
		Cmd.Material = Material;
		Cmd.SectionIndexStart = 0;
		Cmd.SectionIndexCount = static_cast<uint32>(MeshData->Indices.size());
		Cmd.PerObjectConstants = FPerObjectConstants(World, FColor::White().ToVector4());
		Cmd.WorldAABB = FAABB::TransformAABB(Bounds, World);
		Bus.AddCommand(ERenderPass::Opaque, Cmd);
	}

	Renderer.PrepareBatchers(Bus);
	Renderer.Render(Bus);

	ID3D11ShaderResourceView* PreviewSRV =
		const_cast<ID3D11ShaderResourceView*>(Renderer.GetCurrentSceneSRV());
	Renderer.UseBackBufferRenderTargets();
	return PreviewSRV;
}
