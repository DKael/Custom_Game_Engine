#pragma once

#include "Engine/Runtime/Engine.h"
#include "Game/GameViewportClient.h"

class UGameEngine : public UEngine
{
public:
    DECLARE_CLASS(UGameEngine, UEngine)

    UGameEngine() = default;
    ~UGameEngine() override = default;

    void Init(FWindowsWindow* InWindow) override;
    void BeginPlay() override;
    void Tick(float DeltaTime) override;
    void OnWindowResized(uint32 Width, uint32 Height) override;
    void RequestGameRestart() override;
    void SetPlayerControlEnabled(bool bEnabled) override;

    const FGameViewportClient& GetViewportClient() const { return ViewportClient; }
    FGameViewportClient& GetViewportClient() { return ViewportClient; }

private:
    bool LoadStartLevel();
    bool ActivateLoadedStartLevel(bool bBeginPlayNow);
    bool RestartStartLevel();
    void ResetStartLevelRuntimeState();
    
    FGameViewportClient ViewportClient;
    bool bRestartRequested = false;
};
