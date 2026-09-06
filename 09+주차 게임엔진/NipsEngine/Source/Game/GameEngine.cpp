#include "Game/GameEngine.h"
#include "Game/GameViewportClient.h"
#include "Game/GameRenderPipeline.h"
#include "Core/Paths.h"
#include "Engine/Component/Script/ScriptComponent.h"
#include "Engine/GameFramework/World.h"
#include "Engine/Input/InputSystem.h"
#include "Engine/Scripting/LuaBinder.h"
#include "Serialization/SceneSaveManager.h"

#include <filesystem>

DEFINE_CLASS(UGameEngine, UEngine)

void UGameEngine::Init(FWindowsWindow* InWindow)
{
    UEngine::Init(InWindow);

    if (!LoadStartLevel())
    {
        return;
    }

    ViewportClient.Initialize(InWindow);
    SetRenderPipeline(std::make_unique<FGameRenderPipeline>(this, Renderer));

    ActivateLoadedStartLevel(false);
}

bool UGameEngine::LoadStartLevel()
{
    std::filesystem::path ScenePath = std::filesystem::path(FSceneSaveManager::GetSceneDirectory())
        / (FPaths::ToWide("Scene_Game") + FSceneSaveManager::SceneExtension);

    FWorldContext LoadedContext;
    FSceneSaveManager::Load(FPaths::ToUtf8(ScenePath.wstring()), LoadedContext);
    if (!LoadedContext.World)
    {
        return false;
    }

    LoadedContext.WorldType = EWorldType::Game;
    LoadedContext.ContextHandle = FName("Game");
    LoadedContext.ContextName = "Game";
    WorldList.push_back(LoadedContext);
    return true;
}

void UGameEngine::BeginPlay()
{
    UWorld* World = GetWorld();
    if (World && !World->HasBegunPlay())
    {
        World->BeginPlay();
    }
}

void UGameEngine::ResetStartLevelRuntimeState()
{
    LuaBinder::SetUIMode(false);
    LuaBinder::ResetDriftSalvageStats();
}

bool UGameEngine::ActivateLoadedStartLevel(bool bBeginPlayNow)
{
    if (WorldList.empty() || !WorldList.back().World)
    {
        return false;
    }

    FWorldContext& Context = WorldList.back();
    Context.WorldType = EWorldType::Game;
    Context.ContextHandle = FName("Game");
    Context.ContextName = "Game";

    SetActiveWorld(Context.ContextHandle);
    Context.World->SetWorldType(EWorldType::Game);
    ApplySpatialIndexMaintenanceSettings(Context.World);

    ResetStartLevelRuntimeState();
    ViewportClient.SetWorld(Context.World);
    ViewportClient.ResetInputState();
    CreateDriftSalvageHud();

    if (bBeginPlayNow && !Context.World->HasBegunPlay())
    {
        Context.World->BeginPlay();
    }

    return true;
}

void UGameEngine::RequestGameRestart()
{
    bRestartRequested = true;
}

bool UGameEngine::RestartStartLevel()
{
    bRestartRequested = false;

    DestroyWorldContext(FName("Game"));

    if (!LoadStartLevel())
    {
        return false;
    }

    return ActivateLoadedStartLevel(true);
}

void UGameEngine::Tick(float DeltaTime)
{
    if (bRestartRequested)
    {
        RestartStartLevel();
    }

    UWorld* World = GetWorld();
    const float UnscaledDeltaTime = DeltaTime;
    float ScaledDeltaTime = DeltaTime;
    if (World)
    {
        // Gameplay uses scaled delta time, but camera/post-process timers keep the original frame delta.
        World->PrepareFrame(UnscaledDeltaTime);
        ScaledDeltaTime = World->GetScaledDeltaTime();
    }

	InputSystem::Get().Tick();
	ViewportClient.Tick(World ? World->GetUnscaledDeltaTime() : UnscaledDeltaTime);
    ViewportClient.UpdateCamera(World ? World->GetUnscaledDeltaTime() : UnscaledDeltaTime);
	WorldTick(ScaledDeltaTime);
    ViewportClient.UpdateCamera(World ? World->GetUnscaledDeltaTime() : UnscaledDeltaTime);
	++FrameCounter;
	Render(DeltaTime);
}

void UGameEngine::OnWindowResized(uint32 Width, uint32 Height)
{
    UEngine::OnWindowResized(Width, Height);
    ViewportClient.SetViewportSize(Window->GetWidth(), Window->GetHeight());
}

void UGameEngine::SetPlayerControlEnabled(bool bEnabled)
{
    ViewportClient.SetPlayerControlEnabled(bEnabled);
}
