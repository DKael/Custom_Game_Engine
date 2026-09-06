#pragma once

#include <sol/sol.hpp>
#include "Core/Containers/String.h"
#include "Core/Logging/Log.h"
#include "Math/Vector.h"

class UWorld;
class APawn;
class UCameraComponent;
class FViewportCamera;
class FWindowsWindow;
class InputSystem;

class FGameInputController
{
public:
    void SetWindow(FWindowsWindow* InWindow) { Window = InWindow; }
    void SetCamera(FViewportCamera* InCamera);
    void SetWorld(UWorld* InWorld)
    {
        World = InWorld;
        ControlledPawn = nullptr;
        ControlledCameraComponent = nullptr;
    }
    void SetViewportRect(float InX, float InY, float InWidth, float InHeight);
    void SetDefaultControllerScriptPath(const FString& InScriptPath);
    const FString& GetDefaultControllerScriptPath() const { return DefaultControllerScriptPath; }
    void SetUIMode(bool bInUIMode);
    bool IsUIMode() const { return bUIMode; }

    void Tick(float DeltaTime);
    void Reset();

    bool IsCursorHidden() const { return bCursorHidden; }
    bool IsMouseLocked() const { return bMouseLocked; }

private:
    // 프로젝트의 전역 sol에 통합되어야 함..
    template <typename... Args>
    bool CallLuaFunction(const char* FunctionName, Args&&... ArgsToForward)
    {
        if (!bScriptLoaded)
        {
            UE_LOG("[Lua] Script not loaded");
            return false;
        }

        sol::object FunctionObject = ScriptEnvironment[FunctionName];
        if (!FunctionObject.valid() || FunctionObject.get_type() != sol::type::function)
        {
            UE_LOG("[Lua] Function '%s' not found in script '%s'", FunctionName, LoadedScriptPath.c_str());
            return false;
        }

        sol::protected_function Function = FunctionObject;
        sol::protected_function_result Result = Function(std::forward<Args>(ArgsToForward)...);
        if (!Result.valid())
        {
            sol::error Error = Result;
            UE_LOG("[Lua] Runtime Error in function '%s': %s", FunctionName, Error.what());
            return false;
        }

        return true;
    }

    void EnsureScriptLoaded();
    bool LoadScript();
    void InstallBindings();
    FWString ResolveScriptPathWide(const FString& InScriptPath) const;
    FString ResolveActiveControllerScriptPath();
    void RefreshControlledPawn();
    void SyncViewportCameraFromPawn();
    void TickLuaInput(InputSystem& Input);
    void ApplyUIModeState();

    const char* GetKeyName(int32 KeyCode) const;
    const char* GetMouseButtonName(int32 KeyCode) const;

private:
    FWindowsWindow* Window = nullptr;
    FViewportCamera* Camera = nullptr;
    UWorld* World = nullptr;

    float ViewportX = 0.0f;
    float ViewportY = 0.0f;
    float ViewportWidth = 0.0f;
    float ViewportHeight = 0.0f;
    float CachedLocalMouseX = 0.0f;
    float CachedLocalMouseY = 0.0f;
    APawn* ControlledPawn = nullptr;
    UCameraComponent* ControlledCameraComponent = nullptr;

    bool bUIMode = false;
    bool bCursorHidden = false;               // 실제 커서 숨김 상태입니다.
    bool bMouseLocked = false;                // 실제 마우스 고정 상태입니다.
    bool bScriptLoadAttempted = false;
    bool bScriptLoaded = false;

    FString DefaultControllerScriptPath = "Asset/Scripts/DefaultController.lua";
    FString ActiveControllerScriptPath;
    FString LoadedScriptPath;
    sol::environment ScriptEnvironment;
};
