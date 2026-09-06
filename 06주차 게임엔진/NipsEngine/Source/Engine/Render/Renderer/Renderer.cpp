#include "Renderer.h"
#include "Renderer.h"

#include <iostream>
#include <algorithm>
#include <cmath>
#include "Core/Paths.h"
#include "Core/ResourceManager.h"
#include "Render/Common/RenderTypes.h"
#include "Render/Mesh/MeshManager.h"
#include "Core/Logging/Stats.h"
#include "Core/Logging/GPUProfiler.h"
#include "Render/Scene/RenderCollector.h"

void FRenderer::Create(HWND hWindow)
{
    Device.Create(hWindow);

    if (Device.GetDevice() == nullptr)
    {
        std::cout << "Failed to create D3D Device." << std::endl;
    }

    // 1. 일반 메쉬 (Primitive.hlsl)
    Resources.PrimitiveShader.Create(Device.GetDevice(), L"Shaders/Primitive.hlsl", "VS", "PS", PrimitiveInputLayout,
                                     ARRAYSIZE(PrimitiveInputLayout));

    // 2. 기즈모 (Gizmo.hlsl)
    Resources.GizmoShader.Create(Device.GetDevice(), L"Shaders/Gizmo.hlsl", "VS", "PS", PrimitiveInputLayout,
                                 ARRAYSIZE(PrimitiveInputLayout));

    // 3. 에디터/라인 (Editor.hlsl)
    Resources.EditorShader.Create(Device.GetDevice(), L"Shaders/Editor.hlsl", "VS", "PS", PrimitiveInputLayout,
                                  ARRAYSIZE(PrimitiveInputLayout));

    // 4. 월드 그리드 / 축 (ShaderGrid.hlsl, ShaderAxis.hlsl)
    Resources.GridShader.Create(Device.GetDevice(), L"Shaders/ShaderGrid.hlsl", "GridVS", "GridPS", nullptr, 0);
    Resources.AxisShader.Create(Device.GetDevice(), L"Shaders/ShaderAxis.hlsl", "VS", "PS", nullptr, 0);

    // 5. 선택 마스크 (SelectionMask.hlsl)
    Resources.SelectionMaskShader.Create(Device.GetDevice(), L"Shaders/SelectionMask.hlsl", "VS", "PS",
                                         PrimitiveInputLayout, ARRAYSIZE(PrimitiveInputLayout));

    // 6. 포스트 프로세스 아웃라인 (OutlinePostProcess.hlsl)
    Resources.OutlineShader.Create(Device.GetDevice(), L"Shaders/OutlinePostProcess.hlsl", "VS", "PS", nullptr, 0);

    // 7. 스태틱 메시 (ShaderStaticMesh.hlsl)
    Resources.StaticMeshShader.Create(Device.GetDevice(), L"Shaders/ShaderStaticMesh.hlsl", "mainVS", "mainPS",
                                      NormalVertexInputLayout, ARRAYSIZE(NormalVertexInputLayout));

    // 8. 데칼 (Decal.hlsl)
    Resources.DecalShader.Create(Device.GetDevice(), L"Shaders/Decal.hlsl", "VS", "PS", PrimitiveInputLayout,
                                 ARRAYSIZE(PrimitiveInputLayout));

    // 9. 파이어볼 (FireBall.hlsl)
    Resources.FireBallShader.Create(Device.GetDevice(), L"Shaders/FireBall.hlsl", "VS", "PS", PrimitiveInputLayout,
                                    ARRAYSIZE(PrimitiveInputLayout));

    // 10. Depth Scene View 모드 (DepthScene.hlsl)
    Resources.DepthVisualizerShader.Create(Device.GetDevice(), L"Shaders/DepthScene.hlsl", "VS", "PS", nullptr, 0);

    // 11. Fog (Fog.hlsl)
    Resources.FogShader.Create(Device.GetDevice(), L"Shaders/Fog.hlsl", "VS", "PS", nullptr, 0);

    // 12. FXAA 모드 (ShaderFXAA.hlsl)
    Resources.FXAAShader.Create(Device.GetDevice(), L"Shaders/ShaderFXAA.hlsl", "FxaaVS", "FxaaPS", nullptr, 0);

    Resources.PerObjectConstantBuffer.Create(Device.GetDevice(), sizeof(FPerObjectConstants));
    Resources.FrameBuffer.Create(Device.GetDevice(), sizeof(FFrameConstants));
    Resources.GizmoPerObjectConstantBuffer.Create(Device.GetDevice(), sizeof(FGizmoConstants));
    Resources.EditorConstantBuffer.Create(Device.GetDevice(), sizeof(FEditorConstants));
    Resources.OutlineConstantBuffer.Create(Device.GetDevice(), sizeof(FOutlineConstants));
    Resources.StaticMeshConstantBuffer.Create(Device.GetDevice(), sizeof(FStaticMeshConstants));
    Resources.DecalConstantBuffer.Create(Device.GetDevice(), sizeof(FDecalConstants));
    Resources.FireBallConstantBuffer.Create(Device.GetDevice(), sizeof(FFireBallConstants));
    Resources.SceneDepthBuffer.Create(Device.GetDevice(), sizeof(FSceneDepthConstants));
    Resources.FogConstantBuffer.Create(Device.GetDevice(), sizeof(FFogConstants));
    Resources.FXAAConstantBuffer.Create(Device.GetDevice(), sizeof(FFXAAConstants));

    // TODO : SamplerState 관리
    D3D11_SAMPLER_DESC SampDesc = {};
    SampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    SampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    SampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    SampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    Device.GetDevice()->CreateSamplerState(&SampDesc, Resources.MeshSamplerState.ReleaseAndGetAddressOf());

    SampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    SampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    SampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    Device.GetDevice()->CreateSamplerState(&SampDesc, Resources.FXAASamplerState.ReleaseAndGetAddressOf());

    //	MeshManager init
    FMeshManager::Initialize();

    EditorLineBatcher.Create(Device.GetDevice());
    GridLineBatcher.Create(Device.GetDevice());

    // 텍스처는 ResourceManager가 소유 — Batcher 는 셰이더/버퍼만 초기화
    FontBatcher.Create(Device.GetDevice());
    SubUVBatcher.Create(Device.GetDevice());

    InitializePassRenderStates();
    InitializePassBatchers();
    UseBackBufferRenderTargets();

    // GPU Profiler 초기화
    FGPUProfiler::Get().Initialize(Device.GetDevice(), Device.GetDeviceContext());
}

void FRenderer::Release()
{
    Resources.PrimitiveShader.Release();
    Resources.GizmoShader.Release();
    Resources.EditorShader.Release();
    Resources.GridShader.Release();
    Resources.AxisShader.Release();
    Resources.SelectionMaskShader.Release();
    Resources.OutlineShader.Release();
    Resources.StaticMeshShader.Release();
    Resources.DepthVisualizerShader.Release();
    Resources.DecalShader.Release();
    Resources.FireBallShader.Release();
    Resources.FogShader.Release();
    Resources.FXAAShader.Release();

    Resources.PerObjectConstantBuffer.Release();
    Resources.FrameBuffer.Release();
    Resources.GizmoPerObjectConstantBuffer.Release();
    Resources.EditorConstantBuffer.Release();
    Resources.OutlineConstantBuffer.Release();
    Resources.StaticMeshConstantBuffer.Release();
    Resources.DecalConstantBuffer.Release();
    Resources.FireBallConstantBuffer.Release();
    Resources.SceneDepthBuffer.Release();
    Resources.FogConstantBuffer.Release();
    Resources.FXAAConstantBuffer.Release();

    Resources.MeshSamplerState.Reset();
    Resources.FXAASamplerState.Reset();

    FGPUProfiler::Get().Shutdown();

    EditorLineBatcher.Release();
    GridLineBatcher.Release();
    FontBatcher.Release();
    SubUVBatcher.Release();

    Device.Release();
}

//	Bus → Batcher 데이터 수집 (CPU). BeginFrame 이전에 호출.
void FRenderer::PrepareBatchers(const FRenderBus& InRenderBus)
{
    for (uint32 i = 0; i < (uint32)ERenderPass::MAX; ++i)
    {
        if (!PassBatchers[i])
            continue;

        const auto& Commands = InRenderBus.GetCommands(static_cast<ERenderPass>(i));
        const auto& AlignedCommands = GetAlignedCommands(static_cast<ERenderPass>(i), Commands);

        PassBatchers[i].Clear();
        for (const auto& Cmd : AlignedCommands)
            PassBatchers[i].Collect(Cmd, InRenderBus);
    }
}

const TArray<FRenderCommand>& FRenderer::GetAlignedCommands(ERenderPass Pass, const TArray<FRenderCommand>& Commands)
{
    // SubUV 패스: Particle(SRV) 포인터 기준 정렬 → 같은 텍스쳐끼리 연속 배치
    if (Pass == ERenderPass::SubUV && Commands.size() > 1)
    {
        SortedCommandBuffer.assign(Commands.begin(), Commands.end());

        std::sort(SortedCommandBuffer.begin(), SortedCommandBuffer.end(),
                  [](const FRenderCommand& A, const FRenderCommand& B)
                  { return A.Constants.SubUV.Particle < B.Constants.SubUV.Particle; });

        return SortedCommandBuffer;
    }

    if (Pass == ERenderPass::Decal && Commands.size() > 1)
    {
        SortedCommandBuffer.assign(Commands.begin(), Commands.end());

        std::sort(SortedCommandBuffer.begin(), SortedCommandBuffer.end(),
                  [](const FRenderCommand& A, const FRenderCommand& B)
                  {
                      if (A.SortKey != B.SortKey) // key 비교 후 거리 비교
                          return A.SortKey < B.SortKey;
                      return FVector::DistSquared(A.PerObjectConstants.Model.GetOrigin(), FVector::Zero()) >
                             FVector::DistSquared(B.PerObjectConstants.Model.GetOrigin(), FVector::Zero());
                  });

        return SortedCommandBuffer;
    }

    return Commands;
}

//	GPU 프레임 시작. 반드시 Render 이전에 호출되어야 함.
void FRenderer::BeginFrame()
{
    Device.BeginFrame();
    UseBackBufferRenderTargets();

#if STATS
    FGPUProfiler::Get().BeginFrame();
#endif
}

void FRenderer::UseBackBufferRenderTargets()
{
    CurrentRenderTargets = Device.GetBackBufferRenderTargets();
    bUsePostProcessSceneColor = false;
    if (CurrentRenderTargets.IsValid())
    {
        ID3D11RenderTargetView* RTV = CurrentRenderTargets.SceneColorRTV;
        Device.GetDeviceContext()->OMSetRenderTargets(1, &RTV, CurrentRenderTargets.DepthStencilView);
        Device.SetSubViewport(0, 0, static_cast<int32>(CurrentRenderTargets.Width),
                              static_cast<int32>(CurrentRenderTargets.Height));
    }
}

void FRenderer::UseViewportRenderTargets()
{
    CurrentRenderTargets = Device.GetViewportRenderTargets();
    bUsePostProcessSceneColor = false;
    if (!CurrentRenderTargets.IsValid())
    {
        UseBackBufferRenderTargets();
        return;
    }

    Device.SetSubViewport(0, 0, static_cast<int32>(CurrentRenderTargets.Width),
                          static_cast<int32>(CurrentRenderTargets.Height));
}

//	RenderBus에 담긴 모든 RenderCommand에 대해서 Draw Call 수행 (GPU)
void FRenderer::Render(const FRenderBus& InRenderBus, const FFXAASettings* InFXAASettings)
{
    ID3D11DeviceContext* Context = Device.GetDeviceContext();
    bUsePostProcessSceneColor = false;
    UpdateFrameBuffer(Context, InRenderBus);

    RenderScenePasses(Context, InRenderBus);
    RenderPostProcess(Context, InRenderBus, InFXAASettings);
    RenderEditorOverlay(Context, InRenderBus);
}

// ============================================================
// 패스별 기본 렌더 상태 테이블 초기화
// ============================================================
void FRenderer::InitializePassRenderStates()
{
    using E = ERenderPass;
    auto& S = PassRenderStates;

    //                              DepthStencil                   Blend                Rasterizer Topology Shader
    //                              WireframeAware
    S[(uint32)E::Opaque] = {EDepthStencilState::Default,     EBlendState::Opaque,
                            ERasterizerState::SolidBackCull, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
                            &Resources.PrimitiveShader,      true};
    S[(uint32)E::Translucent] = {EDepthStencilState::Default,     EBlendState::AlphaBlend,
                                 ERasterizerState::SolidBackCull, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
                                 &Resources.PrimitiveShader,      false};
    S[(uint32)E::SelectionMask] = {EDepthStencilState::StencilWrite, EBlendState::Opaque,
                                   ERasterizerState::SolidNoCull,    D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
                                   &Resources.SelectionMaskShader,   false};
    S[(uint32)E::Editor] = {EDepthStencilState::Default,       EBlendState::AlphaBlend, ERasterizerState::SolidBackCull,
                            D3D11_PRIMITIVE_TOPOLOGY_LINELIST, &Resources.EditorShader, true};
    S[(uint32)E::Grid] = {EDepthStencilState::Default,
                          EBlendState::AlphaBlend,
                          ERasterizerState::SolidNoCull,
                          D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
                          nullptr,
                          false};
    S[(uint32)E::DepthLess] = {
        EDepthStencilState::DepthReadOnly,     EBlendState::AlphaBlend, ERasterizerState::SolidBackCull,
        D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, &Resources.GizmoShader,  false};
    S[(uint32)E::Font] = {EDepthStencilState::Default,
                          EBlendState::AlphaBlend,
                          ERasterizerState::SolidNoCull,
                          D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
                          nullptr,
                          true};
    S[(uint32)E::SubUV] = {EDepthStencilState::Default,
                           EBlendState::AlphaBlend,
                           ERasterizerState::SolidBackCull,
                           D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
                           nullptr,
                           true};
    S[(uint32)E::PostProcessOutline] = {EDepthStencilState::Default,   EBlendState::AlphaBlend,
                                        ERasterizerState::SolidNoCull, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
                                        &Resources.OutlineShader,      false};
    S[(uint32)E::Decal] = {EDepthStencilState::DepthNone,    EBlendState::AlphaBlend,
                           ERasterizerState::SolidFrontCull, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
                           &Resources.DecalShader,           false};
    S[(uint32)E::FireBall] = {EDepthStencilState::DepthNone,    EBlendState::AlphaBlend,
                              ERasterizerState::SolidFrontCull, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
                              &Resources.FireBallShader,        false};
    S[(uint32)E::Fog] = {EDepthStencilState::DepthNone,         EBlendState::AlphaBlend, ERasterizerState::SolidNoCull,
                         D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, &Resources.FogShader,    false};
}

// ============================================================
// Pass Batcher 바인딩 초기화
// ============================================================
void FRenderer::InitializePassBatchers()
{
    // --- Editor 패스: AABB 디버그 박스 → EditorLineBatcher ---
    PassBatchers[(uint32)ERenderPass::Editor] = {
        /*.Clear   =*/[this]() { EditorLineBatcher.Clear(); },
        /*.Collect =*/
        [this](const FRenderCommand& Cmd, const FRenderBus&)
        {
            if (Cmd.Type == ERenderCommandType::DebugBox)
            {
                if (Cmd.PerObjectConstants.Model.IsIdentity())
                {
                    EditorLineBatcher.AddAABB(FBoundingBox{Cmd.Constants.AABB.Min, Cmd.Constants.AABB.Max},
                                              Cmd.Constants.AABB.Color);
                }
                else
                {
                    EditorLineBatcher.AddOBB(Cmd.PerObjectConstants.Model, Cmd.Constants.AABB.Color);
                }
            }
        },
        /*.Flush   =*/[this](ERenderPass Pass, const FRenderBus& Bus, ID3D11DeviceContext* Ctx)
        { FlushLineBatcher(EditorLineBatcher, Pass, Bus, Ctx); }};

    // --- Grid 패스: ShaderGrid + ShaderAxis ---
    PassBatchers[(uint32)ERenderPass::Grid] = {
        /*.Clear   =*/[this]() { GridShaderPassState = {}; },
        /*.Collect =*/
        [this](const FRenderCommand& Cmd, const FRenderBus& Bus)
        {
            if (Cmd.Type == ERenderCommandType::Grid)
            {
                BuildGridShaderConstants(Cmd, Bus, GridShaderPassState.Constants);
                GridShaderPassState.bDrawGrid = Bus.GetShowFlags().bGrid;
                GridShaderPassState.bDrawAxis = Bus.GetShowFlags().bAxis;
            }
        },
        /*.Flush   =*/[this](ERenderPass, const FRenderBus&, ID3D11DeviceContext* Ctx) { DrawGridShaderPass(Ctx); }};

    // --- Font 패스: 텍스트 → FontBatcher ---
    PassBatchers[(uint32)ERenderPass::Font] = {
        /*.Clear   =*/[this]() { FontBatcher.Clear(); },
        /*.Collect =*/
        [this](const FRenderCommand& Cmd, const FRenderBus& Bus)
        {
            if (Cmd.Type == ERenderCommandType::Font && Cmd.Constants.Font.Text && !Cmd.Constants.Font.Text->empty())
            {
                FontBatcher.AddText(*Cmd.Constants.Font.Text, Cmd.PerObjectConstants.Model, Cmd.Constants.Font.Scale);
            }
        },
        /*.Flush   =*/
        [this](ERenderPass, const FRenderBus&, ID3D11DeviceContext* Ctx)
        {
            const FFontResource* FontRes = FResourceManager::Get().FindFont(FName("Default"));
            FontBatcher.Flush(Ctx, FontRes);
        }};

    // --- SubUV 패스: 스프라이트 → SubUVBatcher ---
    // Collect 시 첫 번째 유효한 SRV를 캡처하여 Flush에서 재순회 방지
    PassBatchers[(uint32)ERenderPass::SubUV] = {
        /*.Clear   =*/[this]()
        {
            SubUVBatcher.Clear();
            SubUVCachedSRV = nullptr;
        },
        /*.Collect =*/
        [this](const FRenderCommand& Cmd, const FRenderBus& Bus)
        {
            const FMatrix& ModelMat = Cmd.PerObjectConstants.Model;
            FVector        MatrixRight = FVector(ModelMat.M[1][0], ModelMat.M[1][1], ModelMat.M[1][2]).GetSafeNormal();
            FVector        MatrixUp = FVector(ModelMat.M[2][0], ModelMat.M[2][1], ModelMat.M[2][2]).GetSafeNormal();
            if (Cmd.Type == ERenderCommandType::SubUV && Cmd.Constants.SubUV.Particle)
            {
                const auto& SubUV = Cmd.Constants.SubUV;
                if (!SubUVCachedSRV && SubUV.Particle->IsLoaded())
                {
                    SubUVCachedSRV = SubUV.Particle->SRV.Get();
                }

                SubUVBatcher.AddSprite(SubUV.Particle->SRV.Get(), Cmd.PerObjectConstants.Model.GetOrigin(),
                                       MatrixRight, // <--- Bus.GetCameraRight() 대신 적용!
                                       MatrixUp,    // <--- Bus.GetCameraUp() 대신 적용!
                                       Cmd.PerObjectConstants.Model.GetScaleVector(), SubUV.FrameIndex,
                                       SubUV.Particle->Columns, SubUV.Particle->Rows, SubUV.Width, SubUV.Height,
                                       Cmd.PerObjectConstants.Color);
            }
            // 기존 SubUV 분기 아래에
            else if (Cmd.Type == ERenderCommandType::Billboard && Cmd.Constants.Billboard.SRV)
            {
                SubUVBatcher.AddSprite(Cmd.Constants.Billboard.SRV, Cmd.PerObjectConstants.Model.GetOrigin(),
                                       MatrixRight, // <--- Bus.GetCameraRight() 대신 적용!
                                       MatrixUp,    // <--- Bus.GetCameraUp() 대신 적용!
                                       Cmd.PerObjectConstants.Model.GetScaleVector(),
                                       0, // FrameIndex 고정
                                       1, // Columns 고정
                                       1, // Rows 고정
                                       Cmd.Constants.Billboard.Width, Cmd.Constants.Billboard.Height,
                                       Cmd.PerObjectConstants.Color);
            }
        },
        /*.Flush   =*/[this](ERenderPass, const FRenderBus&, ID3D11DeviceContext* Ctx) { SubUVBatcher.Flush(Ctx); }};
}

void FRenderer::BuildGridShaderConstants(const FRenderCommand& InCommand, const FRenderBus& InRenderBus,
                                         FEditorConstants& OutConstants) const
{
    const float   GridSpacing = std::max(InCommand.Constants.Grid.GridSpacing, 0.001f);
    const int32   HalfLineCount = std::max(InCommand.Constants.Grid.GridHalfLineCount, 1);
    const FVector CameraPosition = InRenderBus.GetCameraPosition();

    const float BaseRange = GridSpacing * static_cast<float>(HalfLineCount);
    const float PlaneDistance = std::fabs(CameraPosition.Z);
    const float DynamicRange = std::max(BaseRange, PlaneDistance * 2.0f + GridSpacing * 4.0f);
    const float EffectiveRange =
        std::max(DynamicRange * std::max(InCommand.Constants.Grid.RangeScale, 0.01f), GridSpacing);
    const float EffectiveMaxDistance =
        std::max(EffectiveRange * std::max(InCommand.Constants.Grid.MaxDistanceScale, 0.01f), GridSpacing);
    const float EffectiveAxisLength =
        std::max(EffectiveRange * std::max(InCommand.Constants.Grid.AxisLengthScale, 0.01f), GridSpacing);

    OutConstants = {};
    OutConstants.CameraPosition = FVector4(CameraPosition, 1.0f);
    OutConstants.MaxDistance = EffectiveMaxDistance;
    OutConstants.Range = EffectiveRange;
    OutConstants.GridSize = GridSpacing;
    OutConstants.LineThickness = std::max(InCommand.Constants.Grid.LineThickness, 0.01f);
    OutConstants.MajorLineThickness = std::max(InCommand.Constants.Grid.MajorLineThickness, 0.01f);
    OutConstants.MajorLineInterval = std::max(InCommand.Constants.Grid.MajorLineInterval, 1.0f);
    OutConstants.MinorIntensity = std::clamp(InCommand.Constants.Grid.MinorIntensity, 0.0f, 1.0f);
    OutConstants.MajorIntensity = std::clamp(InCommand.Constants.Grid.MajorIntensity, 0.0f, 1.0f);
    OutConstants.AxisThickness = std::max(InCommand.Constants.Grid.AxisThickness, 0.01f);
    OutConstants.AxisLength = EffectiveAxisLength;
}

void FRenderer::DrawGridShaderPass(ID3D11DeviceContext* InDeviceContext)
{
    if (!GridShaderPassState.bDrawGrid && !GridShaderPassState.bDrawAxis)
    {
        return;
    }

    const bool       bDrawPlaneHelpers = GridShaderPassState.bDrawGrid || GridShaderPassState.bDrawAxis;
    FEditorConstants EditorConstants = GridShaderPassState.Constants;

    if (!GridShaderPassState.bDrawGrid)
    {
        EditorConstants.MinorIntensity = 0.0f;
        EditorConstants.MajorIntensity = 0.0f;
    }

    if (!GridShaderPassState.bDrawAxis)
    {
        EditorConstants.AxisThickness = 0.0f;
    }

    Resources.EditorConstantBuffer.Update(InDeviceContext, &EditorConstants, sizeof(FEditorConstants));
    ID3D11Buffer* EditorCB = Resources.EditorConstantBuffer.GetBuffer();
    InDeviceContext->VSSetConstantBuffers(4, 1, &EditorCB);
    InDeviceContext->PSSetConstantBuffers(4, 1, &EditorCB);

    if (bDrawPlaneHelpers)
    {
        Resources.GridShader.Bind(InDeviceContext);
        InDeviceContext->Draw(6, 0);
    }

    if (GridShaderPassState.bDrawAxis)
    {
        Resources.AxisShader.Bind(InDeviceContext);
        InDeviceContext->Draw(12, 0);
    }
}

// ============================================================
// LineBatcher Flush 공통
// ============================================================
void FRenderer::FlushLineBatcher(FLineBatcher& Batcher, ERenderPass Pass, const FRenderBus& Bus,
                                 ID3D11DeviceContext* Context)
{
    if (Batcher.GetLineCount() == 0)
        return;

    const FVector    CameraPosition = Bus.GetView().GetInverse().GetOrigin();
    FEditorConstants EditorConstants = {};
    EditorConstants.CameraPosition = FVector4(CameraPosition, 1.0f);
    Resources.EditorConstantBuffer.Update(Context, &EditorConstants, sizeof(FEditorConstants));

    ApplyPassRenderState(Pass, Context, Bus.GetViewMode());

    ID3D11Buffer* cb = Resources.EditorConstantBuffer.GetBuffer();
    Context->VSSetConstantBuffers(4, 1, &cb);
    Context->PSSetConstantBuffers(4, 1, &cb);

    Batcher.Flush(Context);
}

// ============================================================
// 기본 패스 실행기
// ============================================================
void FRenderer::ExecuteDefaultPass(ERenderPass Pass, const TArray<FRenderCommand>& Commands, const FRenderBus& Bus,
                                   ID3D11DeviceContext* Context)
{
    ApplyPassRenderState(Pass, Context, Bus.GetViewMode());

    const FPassRenderState& State = PassRenderStates[(uint32)Pass];
    ERenderCommandType      LastCommandType = static_cast<ERenderCommandType>(-1);
    for (const auto& Cmd : Commands)
    {
        EDepthStencilState TargetDepth =
            (Cmd.DepthStencilState != static_cast<EDepthStencilState>(-1)) ? Cmd.DepthStencilState : State.DepthStencil;

        EBlendState TargetBlend = (Cmd.BlendState != static_cast<EBlendState>(-1)) ? Cmd.BlendState : State.Blend;

        Device.SetDepthStencilState(TargetDepth);
        Device.SetBlendState(TargetBlend);

        BindShaderByType(Cmd, Context, LastCommandType);

        switch (Cmd.Type)
        {
        case ERenderCommandType::PostProcessOutline:
        {
            DrawPostProcessOutline(Context);
            break;
        }
        case ERenderCommandType::Fog:
        {
            DrawPostProcessFog(Context, Bus);
            break;
        }
        default:
            DrawCommand(Context, Cmd);
            break;
        }
    }
}

void FRenderer::ApplyPassRenderState(ERenderPass Pass, ID3D11DeviceContext* Context, EViewMode CurViewMode)
{
    //	Selection Mask에 대한 것인지 확인하여 RTV를 가져옴
    ID3D11RenderTargetView* RTV = (Pass == ERenderPass::SelectionMask)
                                      ? CurrentRenderTargets.SelectionMaskRTV
                                      : (bUsePostProcessSceneColor && CurrentRenderTargets.PostProcessSceneColorRTV
                                             ? CurrentRenderTargets.PostProcessSceneColorRTV
                                             : CurrentRenderTargets.SceneColorRTV);
    ID3D11DepthStencilView* DSV = CurrentRenderTargets.DepthStencilView;
    if (Pass == ERenderPass::Decal || Pass == ERenderPass::FireBall)
    {
        DSV = nullptr;
    }
    Context->OMSetRenderTargets(1, &RTV, DSV);

    const FPassRenderState& State = PassRenderStates[(uint32)Pass];

    ERasterizerState Rasterizer = State.Rasterizer;
    if (State.bWireframeAware && CurViewMode == EViewMode::Wireframe)
    {
        Rasterizer = ERasterizerState::WireFrame;
    }

    Device.SetDepthStencilState(State.DepthStencil);
    Device.SetBlendState(State.Blend);
    Device.SetRasterizerState(Rasterizer);
    Context->IASetPrimitiveTopology(State.Topology);

    if (State.Shader)
    {
        State.Shader->Bind(Context);
    }
}

void FRenderer::BindShaderByType(const FRenderCommand& InCmd, ID3D11DeviceContext* Context,
                                 ERenderCommandType& LastCommandType)
{
    bool bTypeChanged = (LastCommandType != InCmd.Type);

    // 객체별 Transform Data는 항상 업데이트해야 한다.
    Resources.PerObjectConstantBuffer.Update(Context, &InCmd.PerObjectConstants, sizeof(FPerObjectConstants));

    // 데이터 Update는 항상 수행하지만, 셰이더/상수 버퍼 바인딩은 타입이 변경된 경우에만 수행
    switch (InCmd.Type)
    {
    case ERenderCommandType::Gizmo:
        Resources.GizmoPerObjectConstantBuffer.Update(Context, &InCmd.Constants.Gizmo, sizeof(FGizmoConstants));

        if (bTypeChanged)
        {
            Resources.GizmoShader.Bind(Context);
            ID3D11Buffer* cb1 = Resources.PerObjectConstantBuffer.GetBuffer();
            Context->VSSetConstantBuffers(1, 1, &cb1);
            ID3D11Buffer* cb2 = Resources.GizmoPerObjectConstantBuffer.GetBuffer();
            Context->VSSetConstantBuffers(2, 1, &cb2);
            Context->PSSetConstantBuffers(2, 1, &cb2);
        }
        break;

    case ERenderCommandType::SelectionMask:
        break;

    case ERenderCommandType::PostProcessOutline:
    {
        FOutlineConstants outlineConstants = InCmd.Constants.Outline;
        outlineConstants.ViewportSize = FVector2(CurrentRenderTargets.Width, CurrentRenderTargets.Height);

        Resources.OutlineShader.Bind(Context);
        Resources.OutlineConstantBuffer.Update(Context, &outlineConstants, sizeof(FOutlineConstants));
        ID3D11Buffer* cb = Resources.OutlineConstantBuffer.GetBuffer();
        Context->VSSetConstantBuffers(5, 1, &cb);
        Context->PSSetConstantBuffers(5, 1, &cb);
        break;
    }

    case ERenderCommandType::StaticMesh:
        Resources.StaticMeshConstantBuffer.Update(Context, &InCmd.Constants.StaticMesh, sizeof(FStaticMeshConstants));

        if (bTypeChanged)
        {
            Resources.StaticMeshShader.Bind(Context);

            ID3D11Buffer* cb1 = Resources.PerObjectConstantBuffer.GetBuffer();
            Context->VSSetConstantBuffers(1, 1, &cb1);
            Context->PSSetConstantBuffers(1, 1, &cb1);

            ID3D11Buffer* cb6 = Resources.StaticMeshConstantBuffer.GetBuffer();
            Context->VSSetConstantBuffers(6, 1, &cb6);
            Context->PSSetConstantBuffers(6, 1, &cb6);

            // 샘플러 상태도 주로 렌더 타입에 종속적이므로 스킵 가능
            ID3D11SamplerState* Samplers[] = {Resources.MeshSamplerState.Get()};
            Context->PSSetSamplers(0, 1, Samplers);
        }

        // [주의] 텍스처(SRV)는 타입이 같아도 메시의 머티리얼마다 변경될 수 있으므로 분기문 밖에서 매번 바인딩합니다.
        {
            ID3D11ShaderResourceView* SRVs[4] = {
                InCmd.Constants.StaticMesh.DiffuseSRV, InCmd.Constants.StaticMesh.AmbientSRV,
                InCmd.Constants.StaticMesh.SpecularSRV, InCmd.Constants.StaticMesh.BumpSRV};
            Context->PSSetShaderResources(0, 4, SRVs);
        }
        break;

    case ERenderCommandType::Decal:
    {
        Resources.DecalConstantBuffer.Update(Context, &InCmd.Constants.Decal, sizeof(FDecalConstants));
        if (bTypeChanged)
        {
            Resources.DecalShader.Bind(Context);

            ID3D11Buffer* cb1 = Resources.PerObjectConstantBuffer.GetBuffer();
            Context->VSSetConstantBuffers(1, 1, &cb1);
            Context->PSSetConstantBuffers(1, 1, &cb1);

            // DecalBuffer (b7) - InverseClipToLocal 및 FadeAlpha
            ID3D11Buffer* cb7 = Resources.DecalConstantBuffer.GetBuffer();
            Context->VSSetConstantBuffers(7, 1, &cb7);
            Context->PSSetConstantBuffers(7, 1, &cb7);

            ID3D11SamplerState* Samplers[] = {Resources.MeshSamplerState.Get()};
            Context->PSSetSamplers(0, 1, Samplers);
        }

        UpdateSceneDepthBuffer(Context);
        // SceneDepthBuffer (b10)
        ID3D11Buffer* cb10 = Resources.SceneDepthBuffer.GetBuffer();
        Context->PSSetConstantBuffers(10, 1, &cb10);

        ID3D11ShaderResourceView* DecalSRV = InCmd.Constants.Decal.DecalSRV;
        Context->PSSetShaderResources(1, 1, &DecalSRV);

        ID3D11ShaderResourceView* DepthSRV = CurrentRenderTargets.DepthStencilSRV;
        Context->PSSetShaderResources(0, 1, &DepthSRV);
        break;
    }

    case ERenderCommandType::FireBall:
    {
        Resources.FireBallConstantBuffer.Update(Context, &InCmd.Constants.FireBall, sizeof(FFireBallConstants));
        if (bTypeChanged)
        {
            Resources.FireBallShader.Bind(Context);

            ID3D11Buffer* cb1 = Resources.PerObjectConstantBuffer.GetBuffer();
            Context->VSSetConstantBuffers(1, 1, &cb1);
            Context->PSSetConstantBuffers(1, 1, &cb1);

            // FireBallBuffer (b8)
            ID3D11Buffer* cb8 = Resources.FireBallConstantBuffer.GetBuffer();
            Context->VSSetConstantBuffers(8, 1, &cb8);
            Context->PSSetConstantBuffers(8, 1, &cb8);

            ID3D11SamplerState* Samplers[] = {Resources.MeshSamplerState.Get()};
            Context->PSSetSamplers(0, 1, Samplers);
        }

        UpdateSceneDepthBuffer(Context);
        // SceneDepthBuffer (b10)
        ID3D11Buffer* cb10 = Resources.SceneDepthBuffer.GetBuffer();
        Context->PSSetConstantBuffers(10, 1, &cb10);

        ID3D11ShaderResourceView* FireBallDepthSRV = CurrentRenderTargets.DepthStencilSRV;
        Context->PSSetShaderResources(0, 1, &FireBallDepthSRV);
        break;
    }
    }

    LastCommandType = InCmd.Type;
}

void FRenderer::RenderScenePasses(ID3D11DeviceContext* Context, const FRenderBus& InRenderBus)
{
    ExecuteSinglePass(ERenderPass::Opaque, Context, InRenderBus);
    ExecuteSinglePass(ERenderPass::Decal, Context, InRenderBus);
    ExecuteSinglePass(ERenderPass::Translucent, Context, InRenderBus);
}

void FRenderer::RenderPostProcess(ID3D11DeviceContext* Context, const FRenderBus& InRenderBus,
                                  const FFXAASettings* InFXAASettings)
{
    if (InRenderBus.GetViewMode() == EViewMode::DepthScene)
    {
        DrawDepthVisualizer(Context);
        ExecuteSinglePass(ERenderPass::Fog, Context, InRenderBus);
        ExecuteSinglePass(ERenderPass::SelectionMask, Context, InRenderBus);
        ExecuteSinglePass(ERenderPass::PostProcessOutline, Context, InRenderBus);
        ApplyFXAA(Context, nullptr);
    }
    else
    {
        ExecuteSinglePass(ERenderPass::Fog, Context, InRenderBus);
        ExecuteSinglePass(ERenderPass::FireBall, Context, InRenderBus);
        ExecuteSinglePass(ERenderPass::Grid, Context, InRenderBus);
        ExecuteSinglePass(ERenderPass::SelectionMask, Context, InRenderBus);
        ExecuteSinglePass(ERenderPass::PostProcessOutline, Context, InRenderBus);
        ApplyFXAA(Context, InFXAASettings);
    }
}

void FRenderer::RenderEditorOverlay(ID3D11DeviceContext* Context, const FRenderBus& InRenderBus)
{
    ExecuteSinglePass(ERenderPass::Editor, Context, InRenderBus);
    ExecuteSinglePass(ERenderPass::Font, Context, InRenderBus);
    ExecuteSinglePass(ERenderPass::SubUV, Context, InRenderBus);
    ExecuteSinglePass(ERenderPass::DepthLess, Context, InRenderBus); // 기즈모
}

void FRenderer::ExecuteSinglePass(ERenderPass Pass, ID3D11DeviceContext* Context, const FRenderBus& InRenderBus)
{
    /*if (Pass == ERenderPass::Decal)
    {
        FStatManager::RecordTime("Decal Render", )
    }*/
    uint32      PassIndex = static_cast<uint32>(Pass);
    const auto& Commands = InRenderBus.GetCommands(Pass);
    if (Commands.empty() && (!PassBatchers[PassIndex] || !PassBatchers[PassIndex].operator bool()))
        return;

    if (PassBatchers[PassIndex])
    {
        ApplyPassRenderState(Pass, Context, InRenderBus.GetViewMode());
        PassBatchers[PassIndex].Flush(Pass, InRenderBus, Context);
    }
    else
    {
        const auto& AlignedCommands = GetAlignedCommands(Pass, Commands);
        ExecuteDefaultPass(Pass, AlignedCommands, InRenderBus, Context);
    }
}

void FRenderer::DrawCommand(ID3D11DeviceContext* InDeviceContext, const FRenderCommand& InCommand)
{
    if (InCommand.MeshBuffer == nullptr || !InCommand.MeshBuffer->IsValid())
    {
        return;
    }

    uint32        offset = 0;
    ID3D11Buffer* vertexBuffer = InCommand.MeshBuffer->GetVertexBuffer().GetBuffer();
    if (vertexBuffer == nullptr)
    {
        return;
    }

    uint32 vertexCount = InCommand.MeshBuffer->GetVertexBuffer().GetVertexCount();
    uint32 stride = InCommand.MeshBuffer->GetVertexBuffer().GetStride();
    if (vertexCount == 0 || stride == 0)
    {
        return;
    }

    InDeviceContext->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);

    ID3D11Buffer* indexBuffer = InCommand.MeshBuffer->GetIndexBuffer().GetBuffer();
    if (indexBuffer != nullptr)
    {
        uint32 indexStart = InCommand.SectionIndexStart;
        uint32 indexCount = InCommand.SectionIndexCount;
        InDeviceContext->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R32_UINT, 0);
        InDeviceContext->DrawIndexed(indexCount, indexStart, 0);
    }
    else
    {
        InDeviceContext->Draw(vertexCount, 0);
    }

    if (InCommand.Type == ERenderCommandType::Decal || InCommand.Type == ERenderCommandType::FireBall)
    {
        ID3D11ShaderResourceView* nullSRV = nullptr;
        InDeviceContext->PSSetShaderResources(0, 1, &nullSRV);
    }
}

void FRenderer::DrawPostProcessOutline(ID3D11DeviceContext* InDeviceContext)
{
    ID3D11RenderTargetView* RTV = CurrentRenderTargets.SceneColorRTV;
    InDeviceContext->OMSetRenderTargets(1, &RTV, nullptr);
    InDeviceContext->OMSetDepthStencilState(nullptr, 0);

    ID3D11ShaderResourceView* maskSRV = CurrentRenderTargets.SelectionMaskSRV;
    InDeviceContext->PSSetShaderResources(7, 1, &maskSRV);

    InDeviceContext->Draw(3, 0);

    ID3D11ShaderResourceView* nullSRV = nullptr;
    InDeviceContext->PSSetShaderResources(7, 1, &nullSRV);
}

void FRenderer::DrawPostProcessFog(ID3D11DeviceContext* InDeviceContext, const FRenderBus& InRenderBus)
{
    Device.SetDepthStencilState(EDepthStencilState::DepthNone);
    Device.SetBlendState(EBlendState::AlphaBlend);
    Device.SetRasterizerState(ERasterizerState::SolidNoCull);

    UpdateSceneDepthBuffer(InDeviceContext);
    ID3D11Buffer* SceneDepthConstantBuffer = Resources.SceneDepthBuffer.GetBuffer();
    InDeviceContext->PSSetConstantBuffers(10, 1, &SceneDepthConstantBuffer);

    ID3D11RenderTargetView*   RTV = CurrentRenderTargets.SceneColorRTV;
    ID3D11ShaderResourceView* DepthSRV = CurrentRenderTargets.DepthStencilSRV;

    InDeviceContext->OMSetRenderTargets(1, &RTV, nullptr);
    InDeviceContext->OMSetDepthStencilState(nullptr, 0);

    InDeviceContext->PSSetShaderResources(0, 1, &DepthSRV);

    ID3D11Buffer* depthCB = Resources.SceneDepthBuffer.GetBuffer();
    InDeviceContext->PSSetConstantBuffers(10, 1, &depthCB);

    FFogConstants FogConstants = InRenderBus.GetFogConstants();
    FogConstants.CameraWorldPos = InRenderBus.GetCameraPosition();
    FogConstants.InverseViewProjection = (InRenderBus.GetView() * InRenderBus.GetProj()).GetInverse();

    Resources.FogConstantBuffer.Update(InDeviceContext, &FogConstants, sizeof(FFogConstants));
    ID3D11Buffer* FogBuffer = Resources.FogConstantBuffer.GetBuffer();
    InDeviceContext->PSSetConstantBuffers(9, 1, &FogBuffer);

    Resources.FogShader.Bind(InDeviceContext);

    InDeviceContext->Draw(3, 0);

    ID3D11ShaderResourceView* nullSRV = nullptr;
    InDeviceContext->PSSetShaderResources(0, 1, &nullSRV);
}

void FRenderer::DrawDepthVisualizer(ID3D11DeviceContext* InDeviceContext)
{
    D3D11_VIEWPORT CurrentSubViewportInfo = Device.GetSubViewportInfo();
    float          TextureWidth = CurrentRenderTargets.Width;
    float          TextureHeight = CurrentRenderTargets.Height;

    UpdateSceneDepthBuffer(InDeviceContext);
    ID3D11Buffer* SceneDepthConstantBuffer = Resources.SceneDepthBuffer.GetBuffer();
    InDeviceContext->PSSetConstantBuffers(10, 1, &SceneDepthConstantBuffer);

    Device.SetBlendState(EBlendState::Opaque);
    Device.SetRasterizerState(ERasterizerState::SolidNoCull);
    InDeviceContext->IASetInputLayout(nullptr);

    ID3D11RenderTargetView* RTV = CurrentRenderTargets.SceneColorRTV;
    InDeviceContext->OMSetRenderTargets(1, &RTV, nullptr);
    InDeviceContext->OMSetDepthStencilState(nullptr, 0);

    ID3D11ShaderResourceView* depthSRV = CurrentRenderTargets.DepthStencilSRV;
    InDeviceContext->PSSetShaderResources(0, 1, &depthSRV);

    ID3D11SamplerState* samplers[] = {Resources.MeshSamplerState.Get()};
    InDeviceContext->PSSetSamplers(0, 1, samplers);

    Resources.DepthVisualizerShader.Bind(InDeviceContext);

    InDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    InDeviceContext->Draw(3, 0);

    ID3D11ShaderResourceView* nullSRV = nullptr;
    InDeviceContext->PSSetShaderResources(0, 1, &nullSRV);
}

void FRenderer::UpdateSceneDepthBuffer(ID3D11DeviceContext* InDeviceContext)
{
    D3D11_VIEWPORT CurrentSubViewportInfo = Device.GetSubViewportInfo();
    float          TextureWidth = CurrentRenderTargets.Width;
    float          TextureHeight = CurrentRenderTargets.Height;

    FSceneDepthConstants DepthConstants = {};
    DepthConstants.ViewportUVOffset =
        FVector2(CurrentSubViewportInfo.TopLeftX / TextureWidth, CurrentSubViewportInfo.TopLeftY / TextureHeight);
    DepthConstants.ViewportUVScale =
        FVector2(CurrentSubViewportInfo.Width / TextureWidth, CurrentSubViewportInfo.Height / TextureHeight);
    DepthConstants.DepthTextureSize = FVector2(TextureWidth, TextureHeight);

    Resources.SceneDepthBuffer.Update(InDeviceContext, &DepthConstants, sizeof(FSceneDepthConstants));
}

void FRenderer::ApplyFXAA(ID3D11DeviceContext* InDeviceContext, const FFXAASettings* InFXAASettings)
{
    if (!CurrentRenderTargets.SceneColorSRV || !CurrentRenderTargets.PostProcessSceneColorRTV)
    {
        return;
    }

    UINT           ViewportCount = 1;
    D3D11_VIEWPORT Viewport = {};
    InDeviceContext->RSGetViewports(&ViewportCount, &Viewport);
    if (ViewportCount == 0 || Viewport.Width <= 0.0f || Viewport.Height <= 0.0f || CurrentRenderTargets.Width <= 0.0f ||
        CurrentRenderTargets.Height <= 0.0f)
    {
        return;
    }

    const FFXAASettings  DefaultSettings = {};
    const FFXAASettings& EffectiveSettings = InFXAASettings ? *InFXAASettings : DefaultSettings;

    FFXAAConstants FXAAConstants = {};
    FXAAConstants.InvResolution = FVector2(1.0f / Viewport.Width, 1.0f / Viewport.Height);
    FXAAConstants.ViewportUVMin =
        FVector2(Viewport.TopLeftX / CurrentRenderTargets.Width, Viewport.TopLeftY / CurrentRenderTargets.Height);
    FXAAConstants.ViewportUVSize =
        FVector2(Viewport.Width / CurrentRenderTargets.Width, Viewport.Height / CurrentRenderTargets.Height);
    FXAAConstants.FxaaQualitySubpix = EffectiveSettings.Subpix;
    FXAAConstants.FxaaQualityEdgeThreshold = EffectiveSettings.EdgeThreshold;
    FXAAConstants.FxaaQualityEdgeThresholdMin = EffectiveSettings.EdgeThresholdMin;
    FXAAConstants.FxaaEnabled = InFXAASettings ? 1.0f : 0.0f;

    ID3D11RenderTargetView* RTV = CurrentRenderTargets.PostProcessSceneColorRTV;
    InDeviceContext->OMSetRenderTargets(1, &RTV, nullptr);
    InDeviceContext->OMSetDepthStencilState(nullptr, 0);

    Device.SetBlendState(EBlendState::Opaque);
    Device.SetRasterizerState(ERasterizerState::SolidNoCull);
    InDeviceContext->IASetInputLayout(nullptr);
    InDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    Resources.FXAAShader.Bind(InDeviceContext);
    Resources.FXAAConstantBuffer.Update(InDeviceContext, &FXAAConstants, sizeof(FFXAAConstants));

    ID3D11Buffer* FXAACB = Resources.FXAAConstantBuffer.GetBuffer();
    InDeviceContext->PSSetConstantBuffers(9, 1, &FXAACB);

    ID3D11ShaderResourceView* SceneColorSRV = CurrentRenderTargets.SceneColorSRV;
    InDeviceContext->PSSetShaderResources(0, 1, &SceneColorSRV);

    ID3D11SamplerState* Samplers[] = {Resources.FXAASamplerState.Get()};
    InDeviceContext->PSSetSamplers(0, 1, Samplers);

    InDeviceContext->Draw(3, 0);

    ID3D11ShaderResourceView* NullSRV = nullptr;
    InDeviceContext->PSSetShaderResources(0, 1, &NullSRV);

    bUsePostProcessSceneColor = true;
}

//	Present the rendered frame to the screen. 반드시 Render 이후에 호출되어야 함.
void FRenderer::EndFrame()
{
#if STATS
    FGPUProfiler::Get().EndFrame();
#endif
    Device.EndFrame();
}

void FRenderer::UpdateFrameBuffer(ID3D11DeviceContext* Context, const FRenderBus& InRenderBus)
{
    FFrameConstants frameConstantData;
    frameConstantData.View = InRenderBus.GetView();
    frameConstantData.Projection = InRenderBus.GetProj();
    frameConstantData.bIsWireframe = (InRenderBus.GetViewMode() == EViewMode::Wireframe);
    frameConstantData.WireframeColor = InRenderBus.GetWireframeColor();

    Resources.FrameBuffer.Update(Context, &frameConstantData, sizeof(FFrameConstants));
    ID3D11Buffer* b0 = Resources.FrameBuffer.GetBuffer();
    Context->VSSetConstantBuffers(0, 1, &b0);
    Context->PSSetConstantBuffers(0, 1, &b0);
}
