#include "Engine/Input/GameInputController.h"

#include "Engine/Component/CameraComponent.h"
#include "Engine/Core/Paths.h"
#include "Engine/GameFramework/Pawn.h"
#include "Engine/GameFramework/World.h"
#include "Engine/Input/InputSystem.h"
#include "Engine/Math/Quat.h"
#include "Engine/Runtime/Engine.h"
#include "Engine/Runtime/WindowsWindow.h"
#include "Engine/Scripting/LuaBinder.h"
#include "Engine/Viewport/ViewportCamera.h"
#include <filesystem>
#include <fstream>

namespace
{
    constexpr int32 WatchedKeys[] = { 'W', 'A', 'S', 'D', 'Q', 'E' };
    constexpr int32 WatchedMouseButtons[] = { VK_LBUTTON, VK_RBUTTON, VK_MBUTTON };

    bool ReadFileToStringByWidePath(const FWString& FilePath, FString& OutSource)
    {
        std::ifstream File(std::filesystem::path(FilePath), std::ios::binary);
        if (!File.is_open())
        {
            return false;
        }

        File.seekg(0, std::ios::end);
        const std::streamsize Size = File.tellg();
        File.seekg(0, std::ios::beg);
        if (Size < 0)
        {
            return false;
        }

        OutSource.resize(static_cast<size_t>(Size));
        if (Size == 0)
        {
            return true;
        }

        File.read(OutSource.data(), Size);
        return static_cast<std::streamsize>(File.gcount()) == Size;
    }

    void StripUtf8Bom(FString& Source)
    {
        if (Source.size() >= 3 &&
            static_cast<unsigned char>(Source[0]) == 0xEF &&
            static_cast<unsigned char>(Source[1]) == 0xBB &&
            static_cast<unsigned char>(Source[2]) == 0xBF)
        {
            Source.erase(0, 3);
        }
    }
}

void FGameInputController::SetCamera(FViewportCamera* InCamera)
{
    Camera = InCamera;
}

void FGameInputController::SetViewportRect(float InX, float InY, float InWidth, float InHeight)
{
    ViewportX = InX;
    ViewportY = InY;
    ViewportWidth = InWidth;
    ViewportHeight = InHeight;
}

void FGameInputController::SetDefaultControllerScriptPath(const FString& InScriptPath)
{
    DefaultControllerScriptPath = InScriptPath;
    bScriptLoadAttempted = false;
    bScriptLoaded = false;
    ActiveControllerScriptPath.clear();
    LoadedScriptPath.clear();
    ScriptEnvironment = sol::environment();
}

void FGameInputController::SetUIMode(bool bInUIMode)
{
    if (bUIMode == bInUIMode)
    {
        return;
    }

    bUIMode = bInUIMode;
    LuaBinder::SetUIMode(bUIMode);
    ApplyUIModeState();
}

void FGameInputController::Tick(float DeltaTime)
{
    if (!Camera)
    {
        return;
    }

    InputSystem& Input = InputSystem::Get();
    const bool bRequestedUIMode = LuaBinder::IsUIMode();
    if (bUIMode != bRequestedUIMode)
    {
        bUIMode = bRequestedUIMode;
    }

    RefreshControlledPawn();

    SyncViewportCameraFromPawn();

    ApplyUIModeState();
    if (bUIMode)
    {
        return;
    }

    ActiveControllerScriptPath = ResolveActiveControllerScriptPath();
    EnsureScriptLoaded();
    if (bScriptLoaded)
    {
        ScriptEnvironment["Pawn"] = ControlledPawn;
    }

    POINT MousePoint = Input.GetMousePos();
    if (Window)
    {
        MousePoint = Window->ScreenToClientPoint(MousePoint);
    }

    CachedLocalMouseX = static_cast<float>(MousePoint.x) - ViewportX;
    CachedLocalMouseY = static_cast<float>(MousePoint.y) - ViewportY;

    // TODO: OnUpdate는.. 이 함수와 연관된 모든 것들을 지워야합니다. 더러워요
    CallLuaFunction("OnUpdate", DeltaTime);
    TickLuaInput(Input);
}

void FGameInputController::TickLuaInput(InputSystem& Input)
{
    for (int32 KeyCode : WatchedKeys)
    {
        if (Input.GetKey(KeyCode))
        {
            CallLuaFunction("OnKeyDown", GetKeyName(KeyCode), KeyCode);
        }

        if (Input.GetKeyUp(KeyCode))
        {
            CallLuaFunction("OnKeyUp", GetKeyName(KeyCode), KeyCode);
        }
    }

    if (Input.MouseMoved())
    {
        const float DeltaX = static_cast<float>(Input.MouseDeltaX());
        const float DeltaY = static_cast<float>(Input.MouseDeltaY());
        CallLuaFunction("OnMouseMove", DeltaX, DeltaY, CachedLocalMouseX, CachedLocalMouseY);
    }

    for (int32 MouseButton : WatchedMouseButtons)
    {
        if (Input.GetKeyDown(MouseButton))
        {
            const char* ButtonName = GetMouseButtonName(MouseButton);
            CallLuaFunction("OnMouseClick", ButtonName, true, CachedLocalMouseX, CachedLocalMouseY);
        }

        if (Input.GetKeyUp(MouseButton))
        {
            const char* ButtonName = GetMouseButtonName(MouseButton);
            CallLuaFunction("OnMouseClick", ButtonName, false, CachedLocalMouseX, CachedLocalMouseY);
        }
    }
}

void FGameInputController::Reset()
{
    bUIMode = false;
    LuaBinder::SetUIMode(false);
    ApplyUIModeState();
    ControlledPawn = nullptr;
    ControlledCameraComponent = nullptr;
    bScriptLoadAttempted = false;
    bScriptLoaded = false;
    ActiveControllerScriptPath.clear();
    LoadedScriptPath.clear();
    ScriptEnvironment = sol::environment();
}

void FGameInputController::RefreshControlledPawn()
{
    if (World == nullptr || World->GetPersistentLevel() == nullptr)
    {
        ControlledPawn = nullptr;
        ControlledCameraComponent = nullptr;
        return;
    }

    const bool bCurrentPawnInvalid =
        ControlledPawn == nullptr ||
        !UObject::IsValid(ControlledPawn) ||
        ControlledPawn->IsPendingDestroy() ||
        ControlledPawn->GetFocusedWorld() != World;

    if (bCurrentPawnInvalid)
    {
        ControlledPawn = nullptr;
        const TArray<AActor*>& Actors = World->GetPersistentLevel()->GetActors();
        for (AActor* Actor : Actors)
        {
            APawn* Pawn = Cast<APawn>(Actor);
            if (!Pawn || Pawn->IsPendingDestroy())
            {
                continue;
            }

            ControlledPawn = Pawn;
            break;
        }
    }

    ControlledCameraComponent = ControlledPawn ? ControlledPawn->GetCameraComponent() : nullptr;
}

void FGameInputController::SyncViewportCameraFromPawn()
{
    if (!Camera || !ControlledCameraComponent)
    {
        return;
    }

    const FTransform CameraTransform = ControlledCameraComponent->GetWorldTransform();
    const FCameraState& CameraState = ControlledCameraComponent->GetCameraState();

    Camera->SetLocation(CameraTransform.GetLocation());
    Camera->SetRotation(CameraTransform.GetRotation());
    Camera->SetNearPlane(CameraState.NearZ);
    Camera->SetFarPlane(CameraState.FarZ);

    if (CameraState.bIsOrthogonal)
    {
        Camera->SetProjectionType(EViewportProjectionType::Orthographic);
        const float AspectRatio = Camera->GetAspectRatio() > 0.0f ? Camera->GetAspectRatio() : 1.0f;
        Camera->SetOrthoHeight(CameraState.OrthoWidth / AspectRatio);
    }
    else
    {
        Camera->SetProjectionType(EViewportProjectionType::Perspective);
        Camera->SetFOV(CameraState.FOV);
    }
}

void FGameInputController::ApplyUIModeState()
{
    const bool bGameplayInputActive = !bUIMode;
    bCursorHidden = bGameplayInputActive;
    bMouseLocked = bGameplayInputActive;

    InputSystem& Input = InputSystem::Get();
    Input.SetCursorVisibility(!bCursorHidden);

    if (!bMouseLocked)
    {
        Input.LockMouse(false);
        return;
    }

    if (!Window || ViewportWidth <= 0.0f || ViewportHeight <= 0.0f)
    {
        return;
    }

    POINT Origin = { static_cast<LONG>(ViewportX), static_cast<LONG>(ViewportY) };
    ::ClientToScreen(Window->GetHWND(), &Origin);
    Input.LockMouse(true,
                    static_cast<float>(Origin.x),
                    static_cast<float>(Origin.y),
                    ViewportWidth,
                    ViewportHeight);
}

void FGameInputController::EnsureScriptLoaded()
{
    if (ActiveControllerScriptPath.empty())
    {
        return;
    }

    if (LoadedScriptPath != ActiveControllerScriptPath)
    {
        bScriptLoadAttempted = false;
        bScriptLoaded = false;
        LoadedScriptPath.clear();
        ScriptEnvironment = sol::environment();
    }

    if (bScriptLoadAttempted)
    {
        return;
    }

    bScriptLoadAttempted = true;
    bScriptLoaded = LoadScript();
}

bool FGameInputController::LoadScript()
{
    if (!GEngine)
    {
        return false;
    }

    sol::state& Lua = GEngine->GetLuaScriptSubsystem().GetLuaState();
    ScriptEnvironment = sol::environment(Lua, sol::create, Lua.globals());
    InstallBindings();

    const FWString ResolvedScriptPathWide = ResolveScriptPathWide(ActiveControllerScriptPath);
    LoadedScriptPath = FPaths::ToUtf8(ResolvedScriptPathWide);

    FString ScriptSource;
    if (!ReadFileToStringByWidePath(ResolvedScriptPathWide, ScriptSource))
    {
        printf("[Lua Input Load Error] Failed to open script file: %s\n", LoadedScriptPath.c_str());
        return false;
    }

    StripUtf8Bom(ScriptSource);

    sol::load_result LoadedScript = Lua.load(ScriptSource, LoadedScriptPath);
    if (!LoadedScript.valid())
    {
        sol::error Error = LoadedScript;
        printf("[Lua Input Load Error] %s: %s\n", LoadedScriptPath.c_str(), Error.what());
        return false;
    }

    sol::protected_function ScriptFunction = LoadedScript;
    sol::set_environment(ScriptEnvironment, ScriptFunction);

    sol::protected_function_result Result = ScriptFunction();
    if (!Result.valid())
    {
        sol::error Error = Result;
        printf("[Lua Input Runtime Error] %s: %s\n", LoadedScriptPath.c_str(), Error.what());
        return false;
    }

    return true;
}

void FGameInputController::InstallBindings()
{
    ScriptEnvironment["Pawn"] = ControlledPawn;

    // When no Pawn-driven gameplay camera is available, expose the viewport free camera to Lua fallback scripts.
    // Pawn 및 카메라가 없는 경우 Lua 스크립트에서 뷰포트의 카메라에 직접 접근할 수 있도록 바인딩을 제공
    if (Camera)
    {
        sol::state_view Lua(ScriptEnvironment.lua_state());
        sol::table CameraTable = Lua.create_table();
        CameraTable.set_function("GetPosition", [this]()
        {
            return Camera ? Camera->GetLocation() : FVector::ZeroVector;
        });
        CameraTable.set_function("SetPosition", [this](float X, float Y, float Z)
        {
            if (Camera)
            {
                Camera->SetLocation(FVector(X, Y, Z));
            }
        });
        CameraTable.set_function("AddPosition", [this](float X, float Y, float Z)
        {
            if (Camera)
            {
                Camera->SetLocation(Camera->GetLocation() + FVector(X, Y, Z));
            }
        });
        CameraTable.set_function("GetRotation", [this]()
        {
            return Camera ? Camera->GetRotation().Euler() : FVector::ZeroVector;
        });
        CameraTable.set_function("SetRotation", [this](float X, float Y, float Z)
        {
            if (Camera)
            {
                Camera->SetRotation(FQuat::MakeFromEuler(FVector(X, Y, Z)));
            }
        });
        CameraTable.set_function("GetForwardVector", [this]()
        {
            return Camera ? Camera->GetForwardVector() : FVector(1.0f, 0.0f, 0.0f);
        });
        CameraTable.set_function("GetRightVector", [this]()
        {
            return Camera ? Camera->GetRightVector() : FVector(0.0f, 1.0f, 0.0f);
        });
        CameraTable.set_function("GetUpVector", [this]()
        {
            return Camera ? Camera->GetUpVector() : FVector(0.0f, 0.0f, 1.0f);
        });
        ScriptEnvironment["Camera"] = CameraTable;
    }

    ScriptEnvironment["IsCursorHidden"] = [this]()
    {
        return bCursorHidden;
    };

    ScriptEnvironment["IsMouseLocked"] = [this]()
    {
        return bMouseLocked;
    };

}

FWString FGameInputController::ResolveScriptPathWide(const FString& InScriptPath) const
{
    return FPaths::ToAbsolute(FPaths::ToWide(FPaths::Normalize(InScriptPath)));
}

FString FGameInputController::ResolveActiveControllerScriptPath()
{
    if (ControlledPawn)
    {
        const FString& PawnScriptPath = ControlledPawn->GetControllerScriptPath();
        if (!PawnScriptPath.empty())
        {
            return PawnScriptPath;
        }
    }

    return DefaultControllerScriptPath;
}

const char* FGameInputController::GetKeyName(int32 KeyCode) const
{
    switch (KeyCode)
    {
    case 'W': return "W";
    case 'A': return "A";
    case 'S': return "S";
    case 'D': return "D";
    case 'Q': return "Q";
    case 'E': return "E";
    default: return "Unknown";
    }
}

const char* FGameInputController::GetMouseButtonName(int32 KeyCode) const
{
    switch (KeyCode)
    {
    case VK_LBUTTON: return "LeftMouseButton";
    case VK_RBUTTON: return "RightMouseButton";
    case VK_MBUTTON: return "MiddleMouseButton";
    default: return "UnknownMouseButton";
    }
}
