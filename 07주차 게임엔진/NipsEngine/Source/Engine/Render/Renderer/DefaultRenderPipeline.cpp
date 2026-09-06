#include "DefaultRenderPipeline.h"
#include "Render/Scene/RenderCommand.h"

#include "Renderer.h"
#include "Engine/Runtime/Engine.h"
#include "Editor/Viewport/ViewportCamera.h"
#include "GameFramework/World.h"

FDefaultRenderPipeline::FDefaultRenderPipeline(UEngine* InEngine, FRenderer& InRenderer)
	: Engine(InEngine)
{
	Collector.Initialize(InRenderer.GetFD3DDevice().GetDevice());
}

FDefaultRenderPipeline::~FDefaultRenderPipeline()
{
	Collector.Release();
}

void FDefaultRenderPipeline::Execute(float DeltaTime, FRenderer& Renderer)
{
	Bus.Clear();

	UWorld* World = Engine->GetWorld();
	FViewportCamera* Camera = World ? World->GetActiveCamera() : nullptr;
	if (Camera)
	{
		FMatrix ViewMat = Camera->GetViewMatrix();
		FMatrix ProjMat = Camera->GetProjectionMatrix();

		FShowFlags ShowFlags;
		EViewMode ViewMode = EViewMode::Unlit;

		Bus.SetViewProjection(Camera->GetViewMatrix(), Camera->GetProjectionMatrix());
		Bus.SetRenderSettings(ViewMode, ShowFlags);

		const FFrustum& ViewFrustum = Camera->GetFrustum();
		Collector.CollectWorld(World, ShowFlags, ViewMode, Bus, &ViewFrustum);
	}

	Renderer.PrepareBatchers(Bus);

	// 포그 색 클리어
	const FFogConstants& Fog = Bus.GetFogConstants();
	if (Fog.bEnabled)
	{
		Renderer.GetFD3DDevice().SetClearColor(
			Fog.InscatteringColor.X,
			Fog.InscatteringColor.Y,
			Fog.InscatteringColor.Z
		);
	}

	Renderer.BeginFrame();
	Renderer.Render(Bus);
	Renderer.EndFrame();
}
