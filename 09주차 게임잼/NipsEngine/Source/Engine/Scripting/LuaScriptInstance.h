#pragma once

#include <sol/sol.hpp>

class AActor;
class UScriptComponent;

/**
 * @brief Runtime Lua script instance owned by one ScriptComponent.
 * @ingroup LuaScriptingRuntime
 *
 * Design notes:
 * - The Lua VM (`sol::state`) is shared by subsystem.
 * - Each instance has its own `sol::environment` to isolate script globals per actor.
 * - This struct stores only runtime state; it is not serialized.
 */
struct FLuaScriptInstance
{
    /** Script owner actor used as first callback argument and validity gate. */
    AActor* Owner = nullptr;
    /** Back-reference to owning ScriptComponent for lifecycle/reload integration. */
    UScriptComponent* OwnerComponent = nullptr;
    /** Assigned script path (normalized, resolved by subsystem on load). */
    FString ScriptPath;

    /** Per-instance Lua environment (isolated global namespace for this actor). */
    sol::environment Env;

    /** True when script file loaded and initial chunk executed successfully. */
    bool bLoaded = false;

    /**
     * @brief Creates isolated environment from subsystem shared Lua state.
     * @param Lua Shared Lua VM from FLuaScriptSubsystem.
     */
    FLuaScriptInstance(sol::state& Lua)
        : Env(Lua, sol::create, Lua.globals())
    {
    }
};
