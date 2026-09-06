#include "Engine/Runtime/Engine.h"

#include "Core/Paths.h"
#include "Core/Logging/Stats.h"
#include "Engine/Input/InputSystem.h"
#include "Engine/Core/SoundManager.h"
#include "Engine/Runtime/WindowsWindow.h"
#include "Core/ResourceManager.h"
#include "Render/Renderer/DefaultRenderPipeline.h"
#include "GameFramework/World.h"
#include "Engine/UI/UIManager.h"
#include "Engine/Component/Script/ScriptComponent.h"

DEFINE_CLASS(UEngine, UObject)

UEngine* GEngine = nullptr;

void UEngine::Init(FWindowsWindow* InWindow)
{
    Window = InWindow;

	// 싱글턴 초기화 순서 보장
	FNamePool::Get();
	FObjectFactory::Get();
	FSoundManager::Get();

    InputSystem::Get().SetOwnerWindow(Window->GetHWND());
    Renderer.Create(Window->GetHWND());

	FResourceManager::Get().LoadFromAssetDirectory(FPaths::ToUtf8(FPaths::AssetDirectoryPath()));
	FSoundManager::Get().Initialize();

    LuaScriptSubsystem.Initialize();
    Renderer.CreateResources();

    SetRenderPipeline(std::make_unique<FDefaultRenderPipeline>(this, Renderer));
    FrameCounter = 0;
}

void UEngine::Shutdown()
{
	RenderPipeline.reset();
	FResourceManager::Get().ReleaseGPUResources();
	FSoundManager::Get().Release();
	// Lua state 소멸 전에 UI hover 델리게이트 해제 (sol::function dangling 방지)
	FUIManager::Get().ClearAllHoverDelegates();
	LuaScriptSubsystem.Shutdown();
	Renderer.Release();
}

void UEngine::BeginPlay()
{
    FWorldContext* Context = GetWorldContextFromHandle(ActiveWorldHandle);
    if (Context && Context->World)
    {
        if (Context->WorldType == EWorldType::Game || Context->WorldType == EWorldType::PIE)
        {
            Context->World->BeginPlay();
        }
    }
}

void UEngine::Tick(float DeltaTime)
{
    InputSystem::Get().Tick();
    FSoundManager::Get().Update();
    WorldTick(DeltaTime);
    ++FrameCounter;
    Render(DeltaTime);
}

void UEngine::Render(float DeltaTime)
{
    if (RenderPipeline)
    {
        SCOPE_STAT("UEngine::Render");
        RenderPipeline->Execute(DeltaTime, Renderer);
    }
}

void UEngine::SetRenderPipeline(std::unique_ptr<IRenderPipeline> InPipeline)
{
    RenderPipeline = std::move(InPipeline);
}

void UEngine::OnWindowResized(uint32 Width, uint32 Height)
{
    if (Width <= 0 || Height <= 0)
    {
        return;
    }

    Renderer.InvalidateSceneFinalTargets();
    Renderer.GetFD3DDevice().OnResizeViewport(Width, Height);
    FUIManager::Get().OnResize(static_cast<float>(Width), static_cast<float>(Height));
}

void UEngine::WorldTick(float DeltaTime)
{
    UWorld* World = GetWorld();
    if (World)
    {
        World->Tick(DeltaTime);
    }
}

void UEngine::CreateDriftSalvageHud()
{
    UWorld* World = GetWorld();
    if (!World) return;

    AActor* HudActor = World->SpawnActor<AActor>();
    UScriptComponent* Script = HudActor->AddComponent<UScriptComponent>();
    Script->SetScriptPath("Asset/Scripts/DriftSalvageHud.lua");
}

UWorld* UEngine::GetWorld() const
{
    const FWorldContext* Context = GetWorldContextFromHandle(ActiveWorldHandle);
    return Context ? Context->World : nullptr;
}

FWorldContext& UEngine::CreateWorldContext(EWorldType Type, const FName& Handle, const FString& Name)
{
    FWorldContext Context;
    Context.WorldType = Type;
    Context.ContextHandle = Handle;
    Context.ContextName = Name.empty() ? Handle.ToString() : Name;
    Context.World = UObjectManager::Get().CreateObject<UWorld>();
    WorldList.push_back(Context);
    return WorldList.back();
}

void UEngine::DestroyWorldContext(const FName& Handle)
{
    for (auto it = WorldList.begin(); it != WorldList.end(); ++it)
    {
        if (it->ContextHandle == Handle)
        {
            it->World->EndPlay(EEndPlayReason::Type::Destroyed);
            UObjectManager::Get().DestroyObject(it->World);
            WorldList.erase(it);
            return;
        }
    }
}

FWorldContext* UEngine::GetWorldContextFromHandle(const FName& Handle)
{
    for (FWorldContext& Ctx : WorldList)
    {
        if (Ctx.ContextHandle == Handle)
        {
            return &Ctx;
        }
    }
    return nullptr;
}

const FWorldContext* UEngine::GetWorldContextFromHandle(const FName& Handle) const
{
    for (const FWorldContext& Ctx : WorldList)
    {
        if (Ctx.ContextHandle == Handle)
        {
            return &Ctx;
        }
    }
    return nullptr;
}

FWorldContext* UEngine::GetWorldContextFromWorld(const UWorld* World)
{
    for (FWorldContext& Ctx : WorldList)
    {
        if (Ctx.World == World)
        {
            return &Ctx;
        }
    }
    return nullptr;
}

void UEngine::SetActiveWorld(const FName& Handle)
{
    ActiveWorldHandle = Handle;
}

void UEngine::ApplySpatialIndexMaintenanceSettings(UWorld* TargetWorld)
{
	if (TargetWorld == nullptr)
	{
		return;
	}

	FWorldSpatialIndex::FMaintenancePolicy& Policy = TargetWorld->GetSpatialIndex().GetMaintenancePolicy();

	// Default values
	Policy.BatchRefitMinDirtyCount = 8;
	Policy.BatchRefitDirtyPercentThreshold = 15;
	Policy.RotationStructuralChangeThreshold = 8;
	Policy.RotationDirtyCountThreshold = 24;
	Policy.RotationDirtyPercentThreshold = 30;
}
