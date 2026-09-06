#include "RenderCollector.h"

#include <algorithm>

#include "GameFramework/World.h"
#include "GameFramework/AActor.h"
#include "Object/ActorIterator.h"
#include "Component/BillboardComponent.h"
#include "Component/PrimitiveComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Component/GizmoComponent.h"
#include "Component/TextRenderComponent.h"
#include "Component/SubUVComponent.h"
#include "Core/ResourceManager.h"
#include "Engine/Geometry/Frustum.h"
#include "Render/Resource/Material.h"
#include <unordered_set>

#include "Component/DecalComponent.h"
#include "Component/HeightFogComponent.h"
#include "Component/FireBallComponent.h"
#include "Component/Light/AmbientLightComponent.h"
#include "Component/Light/DirectionalLightComponent.h"

#include "Render/Scene/LightInfo.h"
#include "Component/Light/LightComponent.h"
#include "Component/Light/SpotLightComponent.h"
#include "Component/Light/PointLightComponent.h"

namespace
{
    FColor MakeBVHInternalNodeColor(int32 PathIndexFromLeaf, int32 PathLength)
    {
        if (PathLength <= 1)
        {
            return FColor::Yellow();
        }

        const float T = static_cast<float>(PathIndexFromLeaf) / static_cast<float>(PathLength - 1);
        return FColor::Lerp(FColor::Cyan(), FColor::Yellow(), T);
    }

    bool UsesCameraDependentRenderBounds(const UPrimitiveComponent* PrimitiveComponent)
    {
        if (PrimitiveComponent == nullptr)
        {
            return false;
        }

        switch (PrimitiveComponent->GetPrimitiveType())
        {
        case EPrimitiveType::EPT_Billboard:
        case EPrimitiveType::EPT_Text:
        case EPrimitiveType::EPT_SubUV:
            return true;
        default:
            return false;
        }
    }

    FMatrix MakeViewBillboardMatrix(const UPrimitiveComponent* Primitive, const FRenderBus& RenderBus)
    {
        const UBillboardComponent* BillComp = static_cast<const UBillboardComponent*>(Primitive);

        // 2. 객체를 통해 직접 호출합니다. 파라미터 3개만 넘기면 자기 상태를 알아서 다 계산합니다!
        return BillComp->MakeBillboardWorldMatrix(RenderBus.GetCameraForward(), RenderBus.GetCameraRight(),
                                                  RenderBus.GetCameraUp());
    }

    FMatrix MakeViewSubUVSelectionMatrix(const USubUVComponent* SubUVComp, const FRenderBus& RenderBus)
    {
        FMatrix SelectionMat = SubUVComp->MakeBillboardWorldMatrix(RenderBus.GetCameraForward(),
                                                                   RenderBus.GetCameraRight(), RenderBus.GetCameraUp());

        // 2. SelectionBox(AABB) 크기에 맞게 Y축(Right)과 Z축(Up)에 Width와 Height를 추가로 곱해줍니다.
        // (DirectX의 Row-Major 행렬 기준: M[1]은 Y축, M[2]는 Z축을 의미합니다)
        const float W = SubUVComp->GetWidth();
        const float H = SubUVComp->GetHeight();

        SelectionMat.M[1][0] *= W;
        SelectionMat.M[1][1] *= W;
        SelectionMat.M[1][2] *= W;
        SelectionMat.M[2][0] *= H;
        SelectionMat.M[2][1] *= H;
        SelectionMat.M[2][2] *= H;

        // 3. 기존 코드에 있던 X축 스케일 방어 로직 (Scale.X가 0.01 이하일 때 보정)
        const float ScaleX = SubUVComp->GetWorldScale().X;
        if (std::abs(ScaleX) < 0.01f)
        {
            // 0으로 나누기 방지용 안전 장치 포함
            float Ratio = (std::abs(ScaleX) > 0.0001f) ? (0.01f / ScaleX) : 1.0f;
            SelectionMat.M[0][0] *= Ratio;
            SelectionMat.M[0][1] *= Ratio;
            SelectionMat.M[0][2] *= Ratio;
        }

        return SelectionMat;
    }
    /*
     * BillBoardComponent를 상속받은 text, SubUV가 사용하는 AABB 계산함수(의존성 분리)
     */
    FAABB BuildQuadAABB(const FMatrix& WorldMatrix)
    {
        static constexpr FVector LocalQuadCorners[4] = {FVector(0.0f, -0.5f, 0.5f), FVector(0.0f, 0.5f, 0.5f),
                                                        FVector(0.0f, 0.5f, -0.5f), FVector(0.0f, -0.5f, -0.5f)};

        FAABB Box;
        Box.Reset();

        for (const FVector& Corner : LocalQuadCorners)
        {
            Box.Expand(WorldMatrix.TransformPosition(Corner));
        }

        return Box;
    }

    FAABB BuildRenderAABB(const UPrimitiveComponent* PrimitiveComponent, const FRenderBus& RenderBus)
    {
        switch (PrimitiveComponent->GetPrimitiveType())
        {
        case EPrimitiveType::EPT_Billboard:
            return BuildQuadAABB(MakeViewBillboardMatrix(PrimitiveComponent, RenderBus));
        case EPrimitiveType::EPT_Text:
        {
            const UTextRenderComponent* TextComp = static_cast<const UTextRenderComponent*>(PrimitiveComponent);
            return BuildQuadAABB(TextComp->GetTextMatrix());
        }
        case EPrimitiveType::EPT_SubUV:
        {
            const USubUVComponent* SubUVComp = static_cast<const USubUVComponent*>(PrimitiveComponent);
            return BuildQuadAABB(MakeViewSubUVSelectionMatrix(SubUVComp, RenderBus));
        }

        default:
            return PrimitiveComponent->GetWorldAABB();
        }
    }
} // namespace

void FRenderCollector::CollectWorld(UWorld* World, const FShowFlags& ShowFlags, EViewMode ViewMode,
                                    FRenderBus& RenderBus, const FFrustum* ViewFrustum)
{
    ResetStats();

    if (!World)
        return;

    if (ViewMode == EViewMode::Fog)
    {
        CollectFog(World, RenderBus);
    }

    if (ViewFrustum != nullptr)
    {
        CollectWorldWithFrustum(World, *ViewFrustum, ShowFlags, ViewMode, RenderBus);
        return;
    }

    for (TActorIterator<AActor> Iter(World); Iter; ++Iter)
    {
        AActor* Actor = *Iter;
        if (!Actor || !Actor->IsVisible())
            continue;

        for (UPrimitiveComponent* Primitive : Actor->GetPrimitiveComponents())
        {
            if (Primitive != nullptr && Primitive->IsVisible())
            {
                ++LastCullingStats.TotalVisiblePrimitiveCount;
            }
        }

        CollectFromActor(Actor, ShowFlags, ViewMode, RenderBus);
    }
}

void FRenderCollector::ResetStats()
{
    LastCullingStats = {};
    LastDecalStats = {};
}

void FRenderCollector::CollectWorldWithFrustum(UWorld* World, const FFrustum& ViewFrustum, const FShowFlags& ShowFlags,
                                               EViewMode ViewMode, FRenderBus& RenderBus)
{
    VisiblePrimitiveScratch.clear();
    World->GetSpatialIndex().FrustumQueryPrimitives(ViewFrustum, VisiblePrimitiveScratch, FrustumQueryScratch);

    for (UPrimitiveComponent* Primitive : VisiblePrimitiveScratch)
    {
        if (Primitive == nullptr || UsesCameraDependentRenderBounds(Primitive))
        {
            continue;
        }

        ++LastCullingStats.BVHPassedPrimitiveCount;
        CollectFromComponent(Primitive, ShowFlags, ViewMode, RenderBus);
    }

    std::unordered_set<UPrimitiveComponent*> CollectedCameraDependentPrimitives;
    CollectedCameraDependentPrimitives.reserve(32);

    for (TActorIterator<AActor> Iter(World); Iter; ++Iter)
    {
        AActor* Actor = *Iter;
        if (Actor == nullptr || !Actor->IsVisible())
        {
            continue;
        }

        // Primitive
        for (UPrimitiveComponent* Primitive : Actor->GetPrimitiveComponents())
        {
            if (Primitive == nullptr || !Primitive->IsVisible())
            {
                continue;
            }

            ++LastCullingStats.TotalVisiblePrimitiveCount;
            if (Primitive->IsA<UDecalComponent>())
            {
                ++LastDecalStats.TotalDecalCount;
            }

            if (!UsesCameraDependentRenderBounds(Primitive))
            {
                continue;
            }

            if (!CollectedCameraDependentPrimitives.insert(Primitive).second)
            {
                continue;
            }

            if (ViewFrustum.Intersects(BuildRenderAABB(Primitive, RenderBus)) ==
                FFrustum::EFrustumIntersectResult::Outside)
            {
                continue;
            }

            ++LastCullingStats.FallbackPassedPrimitiveCount;
            CollectFromComponent(Primitive, ShowFlags, ViewMode, RenderBus);
        }

        // Light
        for (ULightComponent* Light : Actor->GetLightComponents())
        {
            if (Light == nullptr)
            {
                continue;
            }

            CollectLight(Light, ShowFlags, RenderBus);
        }
    }
}

void FRenderCollector::CollectSelection(const TArray<AActor*>& SelectedActors, const FShowFlags& ShowFlags,
                                        EViewMode ViewMode, FRenderBus& RenderBus)
{
    bool bHasSelectionMask = false;
    for (AActor* Actor : SelectedActors)
    {
        bHasSelectionMask |= CollectFromSelectedActor(Actor, ShowFlags, ViewMode, RenderBus);
    }

    if (bHasSelectionMask)
    {
        FRenderCommand PostProcessCmd = {};
        PostProcessCmd.Type = ERenderCommandType::PostProcessOutline;
        PostProcessCmd.Constants.Outline.OutlineColor = FVector4(1.0f, 0.5f, 0.0f, 1.0f);
        PostProcessCmd.Constants.Outline.OutlineThicknessPixels = 5.0f;
        RenderBus.AddCommand(ERenderPass::PostProcessOutline, PostProcessCmd);
    }
}

void FRenderCollector::CollectGrid(const FGridConstants& GridConstants, FRenderBus& RenderBus)
{
    FRenderCommand Cmd = {};
    Cmd.Type = ERenderCommandType::Grid;
    Cmd.Constants.Grid = GridConstants;
    RenderBus.AddCommand(ERenderPass::Grid, Cmd);
}

void FRenderCollector::CollectFog(UWorld* World, FRenderBus& RenderBus)
{
    for (TActorIterator<AActor> Iter(World); Iter; ++Iter)
    {
        AActor* Actor = *Iter;
        if (!Actor || !Actor->IsActive())
        {
            continue;
        }

        for (UPrimitiveComponent* PrimitiveComponent : Actor->GetPrimitiveComponents())
        {
            if (!PrimitiveComponent || !PrimitiveComponent->IsActive())
            {
                continue;
            }

            EPrimitiveType PrimitiveType = PrimitiveComponent->GetPrimitiveType();
            if (PrimitiveType == EPrimitiveType::EPT_Fog)
            {
                UHeightFogComponent* FogCompoent = static_cast<UHeightFogComponent*>(PrimitiveComponent);

                FFogConstants Fog = {};
                Fog.bEnabled = true;
                Fog.InscatteringColor = FogCompoent->GetInscatteringColor();
                Fog.Density = FogCompoent->GetDensity();
                Fog.HeightFalloff = FogCompoent->GetHeightFalloff();
                Fog.StartDistance = FogCompoent->GetStartDistance();
                Fog.CutoffDistance = FogCompoent->GetCutoffDistance();
                Fog.MaxOpacity = FogCompoent->GetMaxOpacity();
                Fog.FogHeight = FogCompoent->GetWorldLocation().Z;

                RenderBus.SetFogConstants(Fog);

                FRenderCommand FogCmd = {};
                FogCmd.Type = ERenderCommandType::Fog;
                RenderBus.AddCommand(ERenderPass::Fog, FogCmd);
                return;
            }
        }
    }
}

void FRenderCollector::CollectLight(ULightComponent* LightComponent, const FShowFlags& ShowFlags, FRenderBus& RenderBus)
{
    if (LightComponent == nullptr || !LightComponent->IsActive())
    {
        return;
    }

    FLightingConstants Lighting = RenderBus.GetLightingConstants();
    const FColor&      LightColor = LightComponent->GetColor();
    const FVector      ColorVec = FVector(LightColor.r, LightColor.g, LightColor.b);

    switch (LightComponent->GetLightType())
    {

    case ELightType::Ambient:
    {
        Lighting.AmbientLight.Color = ColorVec;
        Lighting.AmbientLight.Intensity = LightComponent->GetIntensity();
        break;
    }

    case ELightType::Directional:
    {
        const UDirectionalLightComponent* DirLight = static_cast<const UDirectionalLightComponent*>(LightComponent);
        const FVector&                    Direction = DirLight->GetLightDirection().GetSafeNormal();

        FDirectionalLightConstants DirConst;
        DirConst.Direction = Direction;
        DirConst.Color = ColorVec;
        DirConst.Intensity = LightComponent->GetIntensity();
        DirConst.Padding = 0.0f;
        RenderBus.AddDirectionalLight(DirConst);

        if (ShowFlags.bDirectionalLightDebug)
        {
            TArray<FDebugRenderCommand> Temp;
            DebugCmd::MakeArrow(Temp, DirLight->GetWorldLocation(), Direction, 3.0f, 0.3f, FColor::Cyan().ToVector4());
            for (auto& DebugCmd : Temp)
            {
                RenderBus.AddDebugCommand(ERenderPass::Editor, DebugCmd);
            }
        }
        break;
    }

    case ELightType::Point:
    {
        UPointLightComponent* PointLight = static_cast<UPointLightComponent*>(LightComponent);
        FPointLightConstatns  PointLightConst;

        const FColor ColorRef = PointLight->GetColor();

        PointLightConst.Position = PointLight->GetWorldLocation();
        PointLightConst.Radius = PointLight->GetRadius();
        PointLightConst.Color = FVector(ColorRef.R, ColorRef.G, ColorRef.B);
        PointLightConst.Intensity = PointLight->GetIntensity();

        RenderBus.AddPointLight(PointLightConst);

        if (ShowFlags.bPointLightDebug)
        {
            RenderBus.AddDebugCommand(
                ERenderPass::Editor,
                DebugCmd::MakeSphere(PointLightConst.Position, PointLightConst.Radius, FColor::Yellow().ToVector4()));
        }
        break;
    }

    case ELightType::Spot:
    {
        USpotLightComponent* SpotLight = static_cast<USpotLightComponent*>(LightComponent);
        FSpotLightInfo       SpotLightInfo;

        FVector4 LightColor = SpotLight->GetColor().ToVector4();
        float    InnerRadian = MathUtil::DegreesToRadians(SpotLight->GetInnerConeAngle());
        float    OuterRadian = MathUtil::DegreesToRadians(SpotLight->GetOuterConeAngle());

        SpotLightInfo.Color = {LightColor.X, LightColor.Y, LightColor.Z};
        SpotLightInfo.Direction = SpotLight->GetDirection();
        SpotLightInfo.InnerConeCos = cos(InnerRadian);
        SpotLightInfo.OuterConeCos = cos(OuterRadian);
        SpotLightInfo.Position = SpotLight->GetWorldLocation();
        SpotLightInfo.Intensity = SpotLight->GetIntensity();
        SpotLightInfo.Radius = SpotLight->GetRadius();
        RenderBus.AddSpotLightInfo(SpotLightInfo);

        // Add Debug Shape Command
        // Outer
        if (ShowFlags.bSpotLightDebug)
        {
            RenderBus.AddDebugCommand(ERenderPass::Editor,
                                      DebugCmd::MakeCone(SpotLightInfo.Position, SpotLightInfo.Direction,
                                                         SpotLightInfo.Radius, OuterRadian, FColor::Green().ToVector4()));

            // Iner
            RenderBus.AddDebugCommand(ERenderPass::Editor,
                                      DebugCmd::MakeCone(SpotLightInfo.Position, SpotLightInfo.Direction,
                                                         SpotLightInfo.Radius, InnerRadian, FColor::Yellow().ToVector4()));
        }
        break;
    }

    default:
        break;
    }

    RenderBus.SetLightingConstants(Lighting);
}

void FRenderCollector::CollectGizmo(UGizmoComponent* Gizmo, const FShowFlags& ShowFlags, FRenderBus& RenderBus,
                                    bool bIsActiveOperation)
{
    if (ShowFlags.bGizmo == false)
        return;
    if (!Gizmo || !Gizmo->IsVisible())
        return;

    FMeshBuffer* GizmoMesh = &MeshBufferManager.GetMeshBuffer(Gizmo->GetPrimitiveType());
    FMatrix      WorldMatrix = Gizmo->GetWorldMatrix();
    bool         bHolding = Gizmo->IsHolding();
    int32        SelectedAxis = Gizmo->GetSelectedAxis();

    auto CreateGizmoCmd = [&](bool bInner)
    {
        FRenderCommand Cmd = {};
        Cmd.Type = ERenderCommandType::Gizmo;
        Cmd.MeshBuffer = GizmoMesh;

        Cmd.SectionIndexStart = 0;
        Cmd.SectionIndexCount = GizmoMesh->GetIndexBuffer().GetIndexCount();

        Cmd.PerObjectConstants = FPerObjectConstants{WorldMatrix};

        if (bInner)
        {
            Cmd.DepthStencilState = EDepthStencilState::GizmoInside;
            Cmd.BlendState = EBlendState::AlphaBlend;
        }
        else
        {
            Cmd.DepthStencilState = EDepthStencilState::GizmoOutside;
            Cmd.BlendState = EBlendState::Opaque;
        }
        Cmd.Constants.Gizmo.ColorTint = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
        Cmd.Constants.Gizmo.bIsInnerGizmo = bInner ? 1 : 0;
        Cmd.Constants.Gizmo.bClicking = bHolding ? 1 : 0;
        Cmd.Constants.Gizmo.SelectedAxis =
            (SelectedAxis >= 0 && bIsActiveOperation) ? (uint32)SelectedAxis : 0xffffffffu;
        Cmd.Constants.Gizmo.HoveredAxisOpacity = 0.3f;
        return Cmd;
    };

    RenderBus.AddCommand(ERenderPass::DepthLess, CreateGizmoCmd(false));

    if (!bHolding)
    {
        RenderBus.AddCommand(ERenderPass::DepthLess, CreateGizmoCmd(true));
    }
}

void FRenderCollector::CollectFromActor(AActor* Actor, const FShowFlags& ShowFlags, EViewMode ViewMode,
                                        FRenderBus& RenderBus)
{
    if (!Actor->IsVisible())
        return;

    for (UPrimitiveComponent* Primitive : Actor->GetPrimitiveComponents())
    {
        CollectFromComponent(Primitive, ShowFlags, ViewMode, RenderBus);
    }

    // 라이트 컴포넌트 수집 (Frustum culling이 필요 없는 라이트는 여기서 수집)
    for (ULightComponent* Light : Actor->GetLightComponents())
    {
        if (Light == nullptr)
            continue;

        CollectLight(Light, ShowFlags, RenderBus);
    }
}

bool FRenderCollector::CollectFromSelectedActor(AActor* Actor, const FShowFlags& ShowFlags, EViewMode ViewMode,
                                                FRenderBus& RenderBus)
{
    (void)ViewMode;
    if (!Actor->IsVisible())
        return false;

    bool                      bHasSelectionMask = false;
    std::unordered_set<int32> SeenBVHNodeIndices;

    for (UPrimitiveComponent* primitiveComponent : Actor->GetPrimitiveComponents())
    {

        if (!primitiveComponent->IsVisible())
            continue;

        if (primitiveComponent->GetPrimitiveType() == EPrimitiveType::EPT_Decal)
        {
            FRenderCommand BoxCmd = {};
            BoxCmd.Type = ERenderCommandType::DebugBox;

            BoxCmd.Constants.AABB.Min = FVector(-0.5f, -0.5f, -0.5f);
            BoxCmd.Constants.AABB.Max = FVector(0.5f, 0.5f, 0.5f);
            BoxCmd.Constants.AABB.Color = FColor(0.0f, 1.0f, 0.0f, 1.0f);

            BoxCmd.PerObjectConstants.Model = primitiveComponent->GetWorldMatrix();

            RenderBus.AddCommand(ERenderPass::Editor, BoxCmd);
        }

        if (primitiveComponent->GetPrimitiveType() == EPrimitiveType::EPT_FireBall)
        {
            FRenderCommand BoxCmd = {};
            BoxCmd.Type = ERenderCommandType::DebugBox;

            BoxCmd.Constants.AABB.Min = FVector(-0.5f, -0.5f, -0.5f);
            BoxCmd.Constants.AABB.Max = FVector(0.5f, 0.5f, 0.5f);
            BoxCmd.Constants.AABB.Color = FColor(0.0f, 1.0f, 0.0f, 1.0f);

            auto* FireballComp = Cast<UFireBallComponent>(primitiveComponent);
            BoxCmd.PerObjectConstants.Model = FireballComp->GetWorldMatrixWithRadius();

            RenderBus.AddCommand(ERenderPass::Editor, BoxCmd);
        }

        FMeshBuffer* MeshBuffer = nullptr;
        if (primitiveComponent->GetPrimitiveType() == EPrimitiveType::EPT_StaticMesh)
        {
            auto* StaticMeshComp = static_cast<UStaticMeshComponent*>(primitiveComponent);
            MeshBuffer = MeshBufferManager.GetStaticMeshBuffer(StaticMeshComp->GetStaticMesh());
        }
        else
        {
            MeshBuffer = &MeshBufferManager.GetMeshBuffer(primitiveComponent->GetPrimitiveType());
        }

        if (!MeshBuffer)
        {
            continue;
        }

        FRenderCommand BaseCmd{};
        BaseCmd.MeshBuffer = MeshBuffer;
        BaseCmd.PerObjectConstants = FPerObjectConstants{primitiveComponent->GetWorldMatrix()};
        BaseCmd.SectionIndexStart = 0;
        BaseCmd.SectionIndexCount = MeshBuffer->GetIndexBuffer().GetIndexCount();

        if (primitiveComponent->GetPrimitiveType() == EPrimitiveType::EPT_Text)
        {
            UTextRenderComponent* TextComp = static_cast<UTextRenderComponent*>(primitiveComponent);
            const FFontResource*  Font = TextComp->GetFont();
            if (!Font || !Font->IsLoaded())
                continue;
            const FString& Text = TextComp->GetText();
            if (Text.empty())
                continue;

            FMatrix WorldMatrix = TextComp->GetTextMatrix();

            FRenderCommand TextCmd = BaseCmd;
            BaseCmd.PerObjectConstants.Model = WorldMatrix;
            TextCmd.PerObjectConstants = FPerObjectConstants{TextComp->GetWorldMatrix()};
            TextCmd.Type = ERenderCommandType::Font;
            TextCmd.PerObjectConstants.Color = TextComp->GetColor();
            TextCmd.Constants.Font.Text = &Text;
            TextCmd.Constants.Font.Font = Font;
            TextCmd.Constants.Font.Scale = TextComp->GetFontSize();
            TextCmd.BlendState = EBlendState::AlphaBlend;
            TextCmd.DepthStencilState = EDepthStencilState::Default;
            RenderBus.AddCommand(ERenderPass::Font, TextCmd);
        }
        else if (primitiveComponent->GetPrimitiveType() == EPrimitiveType::EPT_SubUV)
        {
            BaseCmd.PerObjectConstants.Model =
                MakeViewSubUVSelectionMatrix(static_cast<USubUVComponent*>(primitiveComponent), RenderBus);
        }

        else if (primitiveComponent->GetPrimitiveType() == EPrimitiveType::EPT_Billboard)
        {
            BaseCmd.PerObjectConstants.Model = MakeViewBillboardMatrix(primitiveComponent, RenderBus);
        }

        const bool bSupportsOutline = primitiveComponent->SupportsOutline();
        if (bSupportsOutline && primitiveComponent->IsOutlineEnabled())
        {
            FRenderCommand MaskCmd = BaseCmd;
            MaskCmd.Type = ERenderCommandType::SelectionMask;
            RenderBus.AddCommand(ERenderPass::SelectionMask, MaskCmd);
            bHasSelectionMask = true;
        }

        if (!bSupportsOutline)
            continue;
        CollectAABBCommand(primitiveComponent, ShowFlags, RenderBus);
        CollectBVHInternalNodeAABBs(primitiveComponent, ShowFlags, RenderBus, SeenBVHNodeIndices);
    }

    return bHasSelectionMask;
}

void FRenderCollector::CollectFromComponent(UPrimitiveComponent* Primitive, const FShowFlags& ShowFlags,
                                            EViewMode ViewMode, FRenderBus& RenderBus)
{
    if (!Primitive->IsVisible())
        return;

    ID3D11ShaderResourceView* DefaultSRV = FResourceManager::Get().GetDefaultWhiteSRV();
	auto ResolveSRV = [&](const FString& Path) -> ID3D11ShaderResourceView*
    {
        FMaterialResource* Res = FResourceManager::Get().FindTexture(Path);
        return (Res && Res->SRV) ? Res->SRV.Get() : DefaultSRV;
    };

    EPrimitiveType PrimType = Primitive->GetPrimitiveType();

    switch (PrimType)
    {
    case EPrimitiveType::EPT_StaticMesh:
    {
        if (!ShowFlags.bPrimitives)
            return;

        UStaticMeshComponent* StaticMeshComp = static_cast<UStaticMeshComponent*>(Primitive);
        const UStaticMesh*    StaticMesh = StaticMeshComp->GetStaticMesh();
        FMeshBuffer*          MeshBuffer = MeshBufferManager.GetStaticMeshBuffer(StaticMesh);

        if (!MeshBuffer)
            return;

        const TArray<FStaticMeshSection>& Sections = StaticMesh->GetSections();
        for (int32 SectionIdx = 0; SectionIdx < static_cast<int32>(Sections.size()); ++SectionIdx)
        {
            const FStaticMeshSection& Section = Sections[SectionIdx];

            FRenderCommand Cmd = {};
            FMatrix        InvModel = Primitive->GetWorldMatrix();
            InvModel.Inverse();
            Cmd.PerObjectConstants =
                FPerObjectConstants{Primitive->GetWorldMatrix(), InvModel, FColor::White().ToVector4()};
            Cmd.Type = ERenderCommandType::StaticMesh;
            Cmd.MeshBuffer = MeshBuffer;
            Cmd.DepthStencilState = EDepthStencilState::DepthReadOnly;
            Cmd.BlendState = EBlendState::Opaque;

            Cmd.SectionIndexStart = Section.StartIndex;
            Cmd.SectionIndexCount = Section.IndexCount;


            // 메테리얼 정보가 없을 시 디폴트 메테리얼을 사용합니다.
            static const FMaterial EngineDefaultMaterial{};

            const FMaterial* MtlData = StaticMeshComp->GetMaterial(SectionIdx);

            if (!MtlData)
                MtlData = &EngineDefaultMaterial;

            Cmd.Constants.StaticMesh.AmbientColor = MtlData->AmbientColor;
            Cmd.Constants.StaticMesh.DiffuseColor = MtlData->DiffuseColor;
            Cmd.Constants.StaticMesh.SpecularColor = MtlData->SpecularColor;
            Cmd.Constants.StaticMesh.Shininess = MtlData->Shininess;

			Cmd.Constants.StaticMesh.bHasNormalMap = MtlData->bHasBumpTexture;

            Cmd.Constants.StaticMesh.ScrollX = StaticMeshComp->GetScroll().first;
            Cmd.Constants.StaticMesh.ScrollY = StaticMeshComp->GetScroll().second;
  

            // 와이어 프레임이 있는 경우 텍스쳐를 사용하지 않는 메테리얼에게 기본 텍스쳐를 강제 주입
            if (ViewMode == EViewMode::Wireframe)
            {
                Cmd.Constants.StaticMesh.bHasDiffuseMap = 1u;
                Cmd.Constants.StaticMesh.bHasSpecularMap = 1u;
                Cmd.Constants.StaticMesh.DiffuseSRV = DefaultSRV;
                Cmd.Constants.StaticMesh.AmbientSRV = DefaultSRV;
                Cmd.Constants.StaticMesh.SpecularSRV = DefaultSRV;
                Cmd.Constants.StaticMesh.BumpSRV = DefaultSRV;
            }
            else
            {
                Cmd.Constants.StaticMesh.bHasDiffuseMap = MtlData->bHasDiffuseTexture ? 1u : 0u;
                Cmd.Constants.StaticMesh.bHasSpecularMap = MtlData->bHasSpecularTexture ? 1u : 0u;
                Cmd.Constants.StaticMesh.DiffuseSRV =
                    MtlData->bHasDiffuseTexture ? ResolveSRV(MtlData->DiffuseTexPath) : DefaultSRV;
                Cmd.Constants.StaticMesh.AmbientSRV =
                    MtlData->bHasAmbientTexture ? ResolveSRV(MtlData->AmbientTexPath) : DefaultSRV;
                Cmd.Constants.StaticMesh.SpecularSRV =
                    MtlData->bHasSpecularTexture ? ResolveSRV(MtlData->SpecularTexPath) : DefaultSRV;
                Cmd.Constants.StaticMesh.BumpSRV =
                    MtlData->bHasBumpTexture ? ResolveSRV(MtlData->BumpTexPath) : DefaultSRV;
            }

            RenderBus.AddCommand(ERenderPass::Opaque, Cmd);
        }

        break;
    }

    case EPrimitiveType::EPT_Text:
    {
        if (!ShowFlags.bBillboardText)
            return;

        UTextRenderComponent* TextComp = static_cast<UTextRenderComponent*>(Primitive);
        const FFontResource*  Font = TextComp->GetFont();
        if (!Font || !Font->IsLoaded())
            return;

        const FString& Text = TextComp->GetText();
        if (Text.empty())
            return;

        FRenderCommand Cmd = {};
        Cmd.Type = ERenderCommandType::Font;
        FMatrix InvModel = Primitive->GetWorldMatrix();
        InvModel.Inverse();
        Cmd.PerObjectConstants = FPerObjectConstants{TextComp->GetWorldMatrix(), InvModel, TextComp->GetColor()};
        Cmd.Constants.Font.Text = &Text;
        Cmd.Constants.Font.Font = Font;
        Cmd.Constants.Font.Scale = TextComp->GetFontSize();
        Cmd.BlendState = EBlendState::AlphaBlend;
        Cmd.DepthStencilState = EDepthStencilState::Default;

        RenderBus.AddCommand(ERenderPass::Font, Cmd);
        break;
    }

    case EPrimitiveType::EPT_SubUV:
    {
        USubUVComponent*         SubUVComp = static_cast<USubUVComponent*>(Primitive);
        const FParticleResource* Particle = SubUVComp->GetParticle();
        if (!Particle || !Particle->IsLoaded())
            return;

        float FadeAlpha = 1.0f;
        if (SubUVComp->IsDistanceFadeEnabled())
        {
            FVector CameraPos = RenderBus.GetView().GetInverse().GetOrigin();
            float   Distance = FVector::Distance(CameraPos, SubUVComp->GetWorldLocation());
            float   Start = SubUVComp->GetFadeStartDistance();
            float   End = SubUVComp->GetFadeEndDistance();

            if (Distance > Start)
            {
                FadeAlpha = 1.0f - std::clamp((Distance - Start) / (End - Start), 0.0f, 1.0f);
            }
        }

        FRenderCommand Cmd = {};
        FMatrix        InvModel = Primitive->GetWorldMatrix();
        InvModel.Inverse();
        Cmd.PerObjectConstants =
            FPerObjectConstants{MakeViewBillboardMatrix(Primitive, RenderBus), InvModel, FVector4(1.0f, 1.0f, 1.0f, FadeAlpha)};
        Cmd.Type = ERenderCommandType::SubUV;
        Cmd.Constants.SubUV.Particle = Particle;
        Cmd.Constants.SubUV.FrameIndex = SubUVComp->GetFrameIndex();
        Cmd.Constants.SubUV.Width = SubUVComp->GetWidth();
        Cmd.Constants.SubUV.Height = SubUVComp->GetHeight();
        Cmd.BlendState = EBlendState::AlphaBlend;
        Cmd.DepthStencilState = EDepthStencilState::Default;

        RenderBus.AddCommand(ERenderPass::SubUV, Cmd);
        break;
    }
    case EPrimitiveType::EPT_Billboard:
    {
        UBillboardComponent* BillboardComp = static_cast<UBillboardComponent*>(Primitive);
        FMaterialResource*   Sprite = BillboardComp->GetCachedSprite();

        ID3D11ShaderResourceView* SRV =
            (Sprite && Sprite->SRV) ? Sprite->SRV.Get() : FResourceManager::Get().GetDefaultWhiteSRV();

        FVector CameraPos = RenderBus.GetView().GetInverse().GetOrigin();
        float   Distance = FVector::Distance(CameraPos, BillboardComp->GetWorldLocation());

		float ScaleFactor = Distance / 10.0f;
        ScaleFactor = std::clamp(ScaleFactor, 1.0f, 3.0f);

		FMatrix FinalWorldMatrix = MakeViewBillboardMatrix(Primitive, RenderBus);
        FinalWorldMatrix =  FinalWorldMatrix.ApplyScale((FVector(ScaleFactor, ScaleFactor, ScaleFactor)));

        float FadeAlpha = 1.0f;
        if (BillboardComp->IsDistanceFadeEnabled())
        {
            float   Distance = FVector::Distance(CameraPos, BillboardComp->GetWorldLocation());
            float   Start = BillboardComp->GetFadeStartDistance();
            float   End = BillboardComp->GetFadeEndDistance();

            float Denom = End - Start;

            if (std::abs(Denom) > 0.001f)
            {
                float DistanceFade = 1.0f - std::clamp((Distance - Start) / Denom, 0.0f, 1.0f);
                FadeAlpha *= DistanceFade;
            }
        }

        FRenderCommand Cmd = {};
        Cmd.Type = ERenderCommandType::Billboard;
        FMatrix InvModel = FinalWorldMatrix;
        InvModel.Inverse();
        const FColor& Tint = BillboardComp->GetTintColor();
        Cmd.PerObjectConstants =
            FPerObjectConstants{FinalWorldMatrix, InvModel, FVector4(Tint.R, Tint.G, Tint.B, FadeAlpha)};
        Cmd.Constants.Billboard.SRV = SRV;
        Cmd.Constants.Billboard.Width = BillboardComp->GetWidth();
        Cmd.Constants.Billboard.Height = BillboardComp->GetHeight();
        Cmd.BlendState = EBlendState::AlphaBlend;
        Cmd.DepthStencilState = EDepthStencilState::Default;

        RenderBus.AddCommand(ERenderPass::SubUV, Cmd); // SubUV 패스 재사용
        break;
    }
    case EPrimitiveType::EPT_Decal:
    {
        if (!ShowFlags.bDecal)
            return;
        LastDecalStats.TotalVisibleDecalCount++;

        UDecalComponent* DecalComp = static_cast<UDecalComponent*>(Primitive);
        const FMaterial* DecalMat = DecalComp->GetDecalMaterial();

        FRenderCommand Cmd = {};
        Cmd.Type = ERenderCommandType::Decal;

        FMeshBuffer* UnitCubeMesh = &MeshBufferManager.GetMeshBuffer(EPrimitiveType::EPT_Decal);
        Cmd.MeshBuffer = UnitCubeMesh;
        Cmd.SectionIndexStart = 0;
        Cmd.SectionIndexCount = UnitCubeMesh->GetIndexBuffer().GetIndexCount();

        FMatrix WorldMatrix = DecalComp->GetWorldMatrix();
        Cmd.PerObjectConstants.Model = WorldMatrix;
        FMatrix ViewMatrix = RenderBus.GetView();
        FMatrix ProjMatrix = RenderBus.GetProj();

        FMatrix WVP = WorldMatrix * ViewMatrix * ProjMatrix;
        Cmd.Constants.Decal.InverseClipToLocal = WVP.GetInverse();

        float FinalFadeAlpha = DecalComp->GetFadeAmount();
        if (DecalComp->IsDistanceFadeEnabled())
        {
            FVector CameraPos = RenderBus.GetView().GetInverse().GetOrigin();
            float   Distance = FVector::Distance(CameraPos, DecalComp->GetWorldLocation());
            float   Start = DecalComp->GetFadeStartDistance();
            float   End = DecalComp->GetFadeEndDistance();
            float   Denom = End - Start;

            if (std::abs(Denom) > 0.001f)
            {
                float DistanceFade = 1.0f - std::clamp((Distance - Start) / Denom, 0.0f, 1.0f);
                FinalFadeAlpha *= DistanceFade;
            }
        }
        Cmd.Constants.Decal.FadeAlpha = FinalFadeAlpha;
        Cmd.SortKey = static_cast<float>(DecalComp->GetSortOrder());

        ID3D11ShaderResourceView* FinalSRV = FResourceManager::Get().GetDefaultWhiteSRV();
        static const FMaterial EngineDefaultMaterial{};
        if (!DecalMat) DecalMat = &EngineDefaultMaterial;

		Cmd.Constants.Decal.AmbientColor = DecalMat->AmbientColor;
        Cmd.Constants.Decal.DiffuseColor = DecalMat->DiffuseColor;
        Cmd.Constants.Decal.SpecularColor = DecalMat->SpecularColor;

        Cmd.Constants.Decal.bHasDiffuseMap = DecalMat->bHasDiffuseTexture ? 1u : 0u;
        Cmd.Constants.Decal.bHasSpecularMap = DecalMat->bHasSpecularTexture ? 1u : 0u;
        Cmd.Constants.Decal.bHasNormalMap = DecalMat->bHasBumpTexture && !DecalComp->IsUsingSurfaceNormal() ? 1u : 0u;
        Cmd.Constants.Decal.DiffuseSRV = DecalMat->bHasDiffuseTexture ? ResolveSRV(DecalMat->DiffuseTexPath) : DefaultSRV;
        Cmd.Constants.Decal.AmbientSRV = DecalMat->bHasAmbientTexture ? ResolveSRV(DecalMat->AmbientTexPath) : DefaultSRV;
        Cmd.Constants.Decal.SpecularSRV = DecalMat->bHasSpecularTexture ? ResolveSRV(DecalMat->SpecularTexPath) : DefaultSRV;
        Cmd.Constants.Decal.BumpSRV = DecalMat->bHasBumpTexture ? ResolveSRV(DecalMat->BumpTexPath) : DefaultSRV;
       
        Cmd.DepthStencilState = EDepthStencilState::DepthReadOnly;
        Cmd.BlendState = EBlendState::AlphaBlend;

        RenderBus.AddCommand(ERenderPass::Decal, Cmd);
        break;
    }
    case EPrimitiveType::EPT_FireBall:
    {
        UFireBallComponent* Fireball = static_cast<UFireBallComponent*>(Primitive);

        FRenderCommand Cmd = {};
        Cmd.Type = ERenderCommandType::FireBall;

        FMeshBuffer* UnitCubeMesh = &MeshBufferManager.GetMeshBuffer(EPrimitiveType::EPT_Decal);
        Cmd.MeshBuffer = UnitCubeMesh;
        Cmd.SectionIndexStart = 0;
        Cmd.SectionIndexCount = UnitCubeMesh->GetIndexBuffer().GetIndexCount();

        Cmd.PerObjectConstants.Model = Fireball->GetWorldMatrixWithRadius();
        Cmd.PerObjectConstants.Color = Fireball->GetColor().ToVector4();

        FMatrix WorldMatrix = Fireball->GetWorldMatrix();
        FMatrix ViewMatrix = RenderBus.GetView();
        FMatrix ProjMatrix = RenderBus.GetProj();

        FMatrix WVP = WorldMatrix * ViewMatrix * ProjMatrix;
        Cmd.Constants.FireBall.InverseClipToLocal = WVP.GetInverse();
        Cmd.Constants.FireBall.Intensity = Fireball->GetIntensity();
        Cmd.Constants.FireBall.Radius = Fireball->GetRadius();
        Cmd.Constants.FireBall.RadiusFallOff = Fireball->GetRadiusFallOff();

        Cmd.DepthStencilState = EDepthStencilState::DepthReadOnly;
        Cmd.BlendState = EBlendState::Additive;

        RenderBus.AddCommand(ERenderPass::FireBall, Cmd);
        break;
    }
    default:
        if (PrimType == EPrimitiveType::EPT_TransGizmo || PrimType == EPrimitiveType::EPT_RotGizmo ||
            PrimType == EPrimitiveType::EPT_ScaleGizmo)
        {
            return;
        }
        return;
    }
}

void FRenderCollector::CollectBVHInternalNodeAABBs(UPrimitiveComponent* PrimitiveComponent, const FShowFlags& ShowFlags,
                                                   FRenderBus& RenderBus, std::unordered_set<int32>& SeenNodeIndices)
{
    if (!ShowFlags.bBoundingVolume || !ShowFlags.bBVHBoundingVolume || PrimitiveComponent == nullptr)
    {
        return;
    }

    AActor* Owner = PrimitiveComponent->GetOwner();
    UWorld* World = Owner ? Owner->GetWorld() : nullptr;
    if (World == nullptr)
    {
        return;
    }

    const FWorldSpatialIndex& SpatialIndex = World->GetSpatialIndex();
    const int32               ObjectIndex = SpatialIndex.FindObjectIndex(PrimitiveComponent);
    if (ObjectIndex == FBVH::INDEX_NONE)
    {
        return;
    }

    const FBVH&          BVH = SpatialIndex.GetBVH();
    const TArray<int32>& ObjectToLeafNode = BVH.GetObjectToLeafNode();
    if (ObjectIndex < 0 || ObjectIndex >= static_cast<int32>(ObjectToLeafNode.size()))
    {
        return;
    }

    const int32 LeafNodeIndex = ObjectToLeafNode[ObjectIndex];
    if (LeafNodeIndex == FBVH::INDEX_NONE)
    {
        return;
    }

    const TArray<FBVH::FNode>& Nodes = BVH.GetNodes();
    if (LeafNodeIndex < 0 || LeafNodeIndex >= static_cast<int32>(Nodes.size()))
    {
        return;
    }

    TArray<int32> PathToRoot;
    PathToRoot.reserve(16);

    int32 CurrentNodeIndex = Nodes[LeafNodeIndex].Parent;
    while (CurrentNodeIndex != FBVH::INDEX_NONE)
    {
        if (CurrentNodeIndex < 0 || CurrentNodeIndex >= static_cast<int32>(Nodes.size()))
        {
            break;
        }

        PathToRoot.push_back(CurrentNodeIndex);
        CurrentNodeIndex = Nodes[CurrentNodeIndex].Parent;
    }

    for (int32 PathIndex = 0; PathIndex < static_cast<int32>(PathToRoot.size()); ++PathIndex)
    {
        const int32 NodeIndex = PathToRoot[PathIndex];
        if (!SeenNodeIndices.insert(NodeIndex).second)
        {
            continue;
        }

        const FBVH::FNode& Node = Nodes[NodeIndex];
        if (Node.IsLeaf())
        {
            continue;
        }

        const FColor Color = MakeBVHInternalNodeColor(PathIndex, static_cast<int32>(PathToRoot.size()));
        CollectAABBCommand(Node.Bounds, Color, RenderBus);
    }
}

void FRenderCollector::CollectAABBCommand(const FAABB& Box, const FColor& Color, FRenderBus& RenderBus)
{
    FRenderCommand AABBCmd = {};
    AABBCmd.Type = ERenderCommandType::DebugBox;
    AABBCmd.Constants.AABB.Min = Box.Min;
    AABBCmd.Constants.AABB.Max = Box.Max;
    AABBCmd.Constants.AABB.Color = Color;
    RenderBus.AddCommand(ERenderPass::Editor, AABBCmd);
}

void FRenderCollector::CollectAABBCommand(UPrimitiveComponent* PrimitiveComponent, const FShowFlags& ShowFlags,
                                          FRenderBus& RenderBus)
{
    if (!ShowFlags.bBoundingVolume)
        return;

    const FAABB Box = BuildRenderAABB(PrimitiveComponent, RenderBus);
    CollectAABBCommand(Box, FColor::White(), RenderBus);
}
