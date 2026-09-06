#pragma once

#include "Engine/Input/ViewportInputRouter.h"
#include "Engine/Runtime/ViewportClient.h"
#include "Engine/Viewport/ViewportCamera.h"

class UWorld;

class FGameViewportClient : public FViewportClient
{
public:
    FGameViewportClient() = default;
    virtual ~FGameViewportClient() override = default;

    virtual void Initialize(FWindowsWindow* InWindow) override;
    virtual void SetViewportSize(float InWidth, float InHeight) override;
    virtual void Tick(float DeltaTime) override;
    virtual void BuildSceneView(FSceneView& OutView) const override;

    void SetWorld(UWorld* InWorld);
    UWorld* GetWorld() const { return World; }

    FViewportCamera& GetCamera() { return Camera; }
    void ResetInputState();

private:
    void TickInput(float DeltaTime);

private:
    UWorld* World = nullptr;
    FViewportCamera Camera;
    FViewportInputRouter InputRouter;
};
