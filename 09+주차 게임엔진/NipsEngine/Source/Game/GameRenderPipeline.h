#pragma once

#include "Engine/Render/Renderer/IRenderPipeline.h"
#include "Engine/Render/Collector/RenderCollector.h"
#include "Engine/Render/Scene/RenderBus.h"

class UGameEngine;
class FGameViewportClient;
struct FSceneView;

class FGameRenderPipeline : public IRenderPipeline
{
public:
    FGameRenderPipeline(UGameEngine* InGameEngine, FRenderer& InRenderer);
    ~FGameRenderPipeline() override;

	void Execute(float DeltaTime, FRenderer& Renderer) override;

private:
    void RenderViewport(FRenderer& Renderer);
    bool PrepareViewport(FRenderer& Renderer, FSceneView& OutSceneView, FGameViewportClient*& OutViewportClient);

	UGameEngine* GameEngine = nullptr;
	FRenderCollector Collector;
	FRenderBus Bus;
};
