#pragma once

#include "Core/Containers/String.h"
#include "Math/Vector.h"

#include <sol/sol.hpp>

/**
 * @brief Centralized Lua binding registration helpers.
 * @ingroup LuaScriptingBinding
 *
 * This module owns only type/global registration logic.
 * Runtime instance ownership and script load/reload remain in FLuaScriptSubsystem.
 */
namespace LuaBinder
{
    void SetUIMode(bool bEnabled);
    bool IsUIMode();
    void ResetDriftSalvageStats();
    void RequestDriftSalvageGameOver();
    bool ConsumeDriftSalvageGameOverRequest();
    void ApplyDriftSalvageDamage(int32 Damage);
    void ApplyDriftSalvagePickup(const FString& ActorTag);
    bool TryApplyDriftSalvagePickup(const FString& ActorTag);
    float GetDriftSalvagePickupWeight(const FString& ActorTag);
    bool CanApplyDriftSalvagePickup(const FString& ActorTag, float ReservedWeight = 0.0f);
    int32 GetDriftSalvageHealth();
    int32 GetDriftSalvageMoney();
    float GetDriftSalvageWeight();
    float GetDriftSalvageWeightCapacity();

    /**
     * @brief Registers engine-facing userdata/types (Vec3, HitInfo, Component, Actor).
     * @param Lua Shared Lua state used by the scripting subsystem.
     */
    void BindEngineTypes(sol::state& Lua);

    /**
     * @brief Registers global helper functions (Log, Warning, Error, Time, FrameCount).
     * @param Lua Shared Lua state used by the scripting subsystem.
     */
    void BindGlobalFunctions(sol::state& Lua);
}
