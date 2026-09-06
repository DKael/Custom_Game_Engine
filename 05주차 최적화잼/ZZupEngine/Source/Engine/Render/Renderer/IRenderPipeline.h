#pragma once

class FRenderer;
class UWorld;

class IRenderPipeline
{
public:
	virtual ~IRenderPipeline() = default;
	virtual void Execute(float DeltaTime, FRenderer& Renderer) = 0;
	virtual void BuildInitialRenderData(UWorld* World) = 0;
};
