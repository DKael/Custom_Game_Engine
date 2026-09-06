#pragma once

#include <sol/sol.hpp>
#include "Engine/Scripting/LuaScriptInstance.h"

class AActor;
class UScriptComponent;

/**
 * @brief Central Lua runtime subsystem for gameplay scripting.
 * @ingroup LuaScriptingRuntime
 *
 * Responsibilities:
 * - Owns shared `sol::state`.
 * - Creates/destroys per-component script instances (`sol::environment`).
 * - Binds engine-facing API to Lua.
 * - Loads/reloads scripts safely and reports runtime errors.
 * - Maintains cached script path list used by editor UI.
 */
class FLuaScriptSubsystem
{
public:
    /** @brief Opens Lua libs, binds engine APIs, and builds script path cache. */
    void Initialize();
    /** @brief Clears runtime instances and cached script metadata. */
    void Shutdown();

    /**
     * @brief Creates one isolated script instance for an actor/component pair.
     * @param Owner Actor that receives lifecycle callbacks as first argument.
     * @param OwnerComponent Script component that owns the runtime instance.
     * @param ScriptPath Relative or absolute script path to load.
     * @return Newly created script instance; load success is reflected in `bLoaded`.
     */
    std::shared_ptr<FLuaScriptInstance> CreateScriptInstance(
        AActor* Owner,
        UScriptComponent* OwnerComponent,
        const FString& ScriptPath);

    /**
     * @brief Loads script file into instance environment.
     * @param Instance Target script instance.
     * @return True on load/execute success.
     */
    bool LoadScript(std::shared_ptr<FLuaScriptInstance> Instance);
    /**
     * @brief Recreates environment and reloads script for an instance.
     * @param Instance Target script instance.
     * @return True when reload succeeds.
     */
    bool ReloadScript(std::shared_ptr<FLuaScriptInstance> Instance);
    /** @brief Removes script instance from subsystem ownership list. */
    void DestroyScriptInstance(const std::shared_ptr<FLuaScriptInstance>& Instance);
    /** @brief Reloads all currently valid script instances. */
    void ReloadAllScripts();
    /** @brief Returns cached script path list. Rebuilds cache when requested or empty. */
    const TArray<FString>& GetAvailableScriptPaths(bool bForceRefresh = false);
    /** @brief Forces script path cache rebuild. */
    void RefreshAvailableScriptPaths();
    /**
     * @brief Returns true when function exists in instance environment.
     * @param Instance Target script instance.
     * @param FunctionName Lua callback name to check.
     */
    bool HasFunction(std::shared_ptr<FLuaScriptInstance> Instance, const FString& FunctionName) const;

    template <typename... Args>
    bool CallFunction(
        std::shared_ptr<FLuaScriptInstance> Instance,
        const FString& FunctionName,
        Args&&... args)
    {
        if (!CanInvoke(Instance))
        {
            return false;
        }

        sol::object FuncObject = Instance->Env[FunctionName];

        if (!FuncObject.valid() || FuncObject.get_type() != sol::type::function)
        {
            // Missing callback is allowed. Example: script that does not implement OnHit.
            return false;
        }

        sol::protected_function Func = FuncObject;

        sol::protected_function_result Result =
            Func(std::forward<Args>(args)...);

        if (!Result.valid())
        {
            sol::error Error = Result;
            LogFunctionError(FunctionName, Instance->ScriptPath, Error.what());
            return false;
        }

        return true;
    }

    sol::state& GetLuaState() { return Lua; }

private:
    /** @brief Delegates type binding registration to LuaBinder. */
    void BindEngineTypes();
    /** @brief Delegates global function registration to LuaBinder. */
    void BindGlobalFunctions();
    /** @brief Scans Asset/Scripts and rebuilds cached `.lua` path list. */
    void RebuildScriptPathCache();
    /** @brief Validity gate before invoking Lua callbacks. */
    bool CanInvoke(const std::shared_ptr<FLuaScriptInstance>& Instance) const;
    /** @brief Resolves project-relative script path to absolute normalized wide path. */
    FWString ResolveScriptPathWide(const FString& ScriptPath) const;
    /** @brief Logs callback runtime errors in a consistent format. */
    void LogFunctionError(const FString& FunctionName, const FString& ScriptPath, const char* ErrorMessage) const;

private:
    /** Shared Lua VM for all script instances (state isolation is environment-based). */
    sol::state Lua;

    /** Live script instances owned by active components. */
    TArray<std::shared_ptr<FLuaScriptInstance>> ScriptInstances;
    /** Cached relative script paths for editor dropdowns. */
    TArray<FString> AvailableScriptPaths;
};
