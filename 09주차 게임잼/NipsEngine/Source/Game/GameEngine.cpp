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
    CreateLogoHud();

    if (bBeginPlayNow && !Context.World->HasBegunPlay())
    {
        Context.World->BeginPlay();
    }

    return true;
}

void UGameEngine::CreateLogoHud()
{
    UWorld* World = GetWorld();
    if (!World) return;

    AActor* HudActor = World->SpawnActor<AActor>();
    UScriptComponent* Script = HudActor->AddComponent<UScriptComponent>();
    Script->SetScriptPath("Asset/Scripts/LogoHud.lua");
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

	InputSystem::Get().Tick();
	ViewportClient.Tick(DeltaTime);
	WorldTick(DeltaTime);
	++FrameCounter;
	Render(DeltaTime);
}

void UGameEngine::OnWindowResized(uint32 Width, uint32 Height)
{
    UEngine::OnWindowResized(Width, Height);
    ViewportClient.SetViewportSize(Window->GetWidth(), Window->GetHeight());
}
