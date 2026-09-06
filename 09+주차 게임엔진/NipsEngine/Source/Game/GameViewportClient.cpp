#include "Game/GameViewportClient.h"

#include "Engine/GameFramework/World.h"
#include "Engine/Runtime/SceneView.h"

void FGameViewportClient::Initialize(FWindowsWindow* InWindow)
{
    FViewportClient::Initialize(InWindow);

    Camera = FViewportCamera();
    Camera.SetFOV(1.0f);
    Camera.SetNearPlane(0.1f);
    Camera.SetFarPlane(1000.0f);
    
    if (InWindow)
    {
        FViewportClient::SetViewportSize(
            static_cast<float>(InWindow->GetWidth()),
            static_cast<float>(InWindow->GetHeight()));
    }
    Camera.OnResize(static_cast<uint32>(WindowWidth), static_cast<uint32>(WindowHeight));

    InputRouter.SetMode(EViewportInputMode::Standalone);
    InputRouter.GetGameInputController().SetWindow(InWindow);
    InputRouter.GetGameInputController().SetCamera(&Camera);
    InputRouter.GetGameInputController().SetViewportRect(0.0f, 0.0f, WindowWidth, WindowHeight);
}

void FGameViewportClient::SetViewportSize(float InWidth, float InHeight)
{
    FViewportClient::SetViewportSize(InWidth, InHeight);
    Camera.OnResize(static_cast<uint32>(WindowWidth), static_cast<uint32>(WindowHeight));
    InputRouter.GetGameInputController().SetViewportRect(0.0f, 0.0f, WindowWidth, WindowHeight);
}

void FGameViewportClient::Tick(float DeltaTime)
{
    TickInput(DeltaTime);
}

void FGameViewportClient::BuildSceneView(FSceneView& OutView) const
{
    OutView.ViewMatrix = Camera.GetViewMatrix();
    OutView.ProjectionMatrix = Camera.GetProjectionMatrix();
    OutView.ViewProjectionMatrix = OutView.ViewMatrix * OutView.ProjectionMatrix;
    OutView.CameraPosition = Camera.GetLocation();
    OutView.CameraForward = Camera.GetForwardVector();
    OutView.CameraRight = Camera.GetRightVector();
    OutView.CameraUp = Camera.GetUpVector();
    OutView.NearPlane = Camera.GetNearPlane();
    OutView.FarPlane = Camera.GetFarPlane();
    OutView.bOrthographic = Camera.IsOrthographic();
    OutView.CameraOrthoHeight = Camera.GetOrthoHeight();
    OutView.CameraFrustum = Camera.GetFrustum();

    OutView.ViewRect = FViewportRect(0, 0, static_cast<int32>(WindowWidth), static_cast<int32>(WindowHeight));
    OutView.ViewMode = EViewMode::Lit;
}

void FGameViewportClient::SetWorld(UWorld* InWorld)
{
    World = InWorld;
    InputRouter.GetGameInputController().SetWorld(InWorld);

    if (World)
    {
        World->SetActiveCamera(&Camera);
    }
}

void FGameViewportClient::UpdateCamera(float UnscaledDeltaTime)
{
    if (World == nullptr)
    {
        return;
    }

    // The camera manager owns transient blends/shakes and writes the temporary final POV into the viewport camera.
    FPlayerCameraManager& PlayerCameraManager = World->GetPlayerCameraManager();
    PlayerCameraManager.Update(UnscaledDeltaTime);
    PlayerCameraManager.ApplyToViewportCamera(Camera);
    World->SetActiveCamera(&Camera);
}

void FGameViewportClient::SetPlayerControlEnabled(bool bEnabled)
{
    InputRouter.GetGameInputController().SetPlayerControlEnabled(bEnabled);
}

void FGameViewportClient::ResetInputState()
{
    InputRouter.GetGameInputController().Reset();
    InputRouter.GetGameInputController().SetCamera(&Camera);
    InputRouter.GetGameInputController().SetViewportRect(0.0f, 0.0f, WindowWidth, WindowHeight);
    InputRouter.GetGameInputController().SetWorld(World);
}

void FGameViewportClient::TickInput(float DeltaTime)
{
    InputRouter.Tick(DeltaTime);
}
