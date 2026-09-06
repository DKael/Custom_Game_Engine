#pragma once

#include "Engine/Component/ActorComponent.h"

#include <sol/sol.hpp>

struct FHitResult;
struct FLuaScriptInstance;

/**
 * @brief Component that owns and drives one Lua gameplay script instance.
 * @ingroup LuaScriptingComponent
 *
 * Runtime goal:
 * - One ScriptComponent == one FLuaScriptInstance.
 * - Lifecycle callbacks are optional in Lua.
 * - Runtime errors are isolated and can disable only this component.
 */
class UScriptComponent : public UActorComponent
{
    struct FLuaCoroutineState;

public:
    DECLARE_CLASS(UScriptComponent, UActorComponent)

    /**
     * @brief High-level runtime phase for this script component.
     */
    enum class EScriptRuntimeState : uint8
    {
        /** Script not started yet or reset state after reload/endplay. */
        Idle,
        /** Start callback completed; script runtime is initialized. */
        Started,
        /** Enable callback completed; active tick/event delivery is allowed. */
        Enabled,
        /** Destroy callback completed once. */
        Destroyed,
        /** Fatal policy state; component is disabled. */
        Failed
    };

    /** @brief Initializes script runtime for play. */
    void BeginPlay() override;
    /** @brief Sends destroy callback and releases script instance. */
    void EndPlay() override;
    /**
     * @brief Delivers per-frame update callback while active.
     * @param DeltaTime Frame delta time in seconds.
     */
    void TickComponent(float DeltaTime) override;
    /** @brief Activates script runtime and sends enable callback. */
    void Activate() override;
    /** @brief Deactivates script runtime and sends disable callback. */
    void Deactivate() override;
    /**
     * @brief Saves/loads script path and enabled flag.
     * @param Ar Archive used for serialization.
     */
    void Serialize(FArchive& Ar) override;
    /**
     * @brief Exposes editable properties for editor details panel.
     * @param OutProps Output property descriptor list.
     */
    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
    /**
     * @brief Handles side effects after editor property changes.
     * @param PropertyName Name of edited property.
     */
    void PostEditProperty(const char* PropertyName) override;

    /** @brief Backward-compatible overlap entry point. */
    void OnOverlap(AActor* OtherActor);
    /** @brief Overlap begin callback forwarder to Lua. */
    void OnOverlapBegin(AActor* OtherActor);
    /** @brief Overlap end callback forwarder to Lua. */
    void OnOverlapEnd(AActor* OtherActor);
    /** @brief Hit callback forwarder without hit payload. */
    void OnHit(AActor* OtherActor);
    /** @brief Hit callback forwarder with hit payload. */
    void OnHit(AActor* OtherActor, const FHitResult& HitInfo);

    /** @brief Rebuilds script environment and reruns startup flow. */
    bool ReloadScript();
    void StartCoroutine(const sol::function& Function);

    /** @brief Sets script path (normalized). */
    void SetScriptPath(const FString& InPath);
    /** @brief Returns script path currently assigned to this component. */
    const FString& GetScriptPath() const { return ScriptPath; }
    /** @brief Returns true when Lua script is loaded successfully. */
    bool IsScriptLoaded() const;

private:
    /**
     * @brief Returns true when runtime is in a state where Lua callbacks can be dispatched.
     */
    bool CanDispatchCallbacks() const;

    /** @brief Ensures this component owns a valid script instance. */
    bool EnsureScriptInstance();
    /** @brief Executes startup callback once per script lifecycle. */
    void StartScriptIfNeeded();
    /** @brief Sends enable callback once per activation cycle. */
    void NotifyScriptEnabled();
    /** @brief Sends disable callback when leaving enabled state. */
    void NotifyScriptDisabled();
    /** @brief Sends destroy callback once before teardown/reload. */
    void NotifyScriptDestroyed();
    /** @brief Applies fatal disable state to this component. */
    void DisableScriptAfterFatalError();
    /** @brief Evaluates failure policy after each optional callback invocation. */
    void ApplyRuntimeFailurePolicy(bool bHadCallback, bool bSucceeded, const char* CallbackContext);
    /** @brief Returns true when failure policy should disable this script. */
    bool ShouldDisableAfterRuntimeFailure(bool bHadCallback, bool bSucceeded);
    void TickCoroutines(float UnscaledDeltaTime);
    bool ResumeCoroutine(FLuaCoroutineState& CoroutineState);
    void ClearCoroutines();

    /**
     * @brief Calls the first existing Lua function from CallbackNames.
     *
     * Owner actor is always available through globals such as `obj`.
     * Legacy callbacks that explicitly declare a leading `self`/owner parameter still receive it.
     * Additional arguments are appended in order after any injected owner argument.
     * `bOutHadCallback` indicates whether any candidate callback existed in Lua.
     *
     * @return true when callback is missing (allowed) or call succeeded, false on runtime failure.
     */
    template<typename... Args>
    bool TryCallPreferred(
        const std::initializer_list<const char*>& CallbackNames,
        bool* bOutHadCallback,
        Args&&... args);

private:
    struct FLuaCoroutineState
    {
        lua_State* Thread = nullptr;
        int ThreadRegistryRef = LUA_NOREF;
        float WaitRemaining = 0.0f;
    };

    /** Relative script path under project root (usually Asset/Scripts/... ). */
    FString ScriptPath;
    /** Serialized checkbox state mirrored to SetActive. */
    bool bSerializedEnabled = true;
    /** World lifecycle gate: true only between BeginPlay and EndPlay. */
    bool bHasBegunPlay = false;
    /** Runtime phase state machine for callback order and one-shot guards. */
    EScriptRuntimeState RuntimeState = EScriptRuntimeState::Idle;
    /** Consecutive runtime callback failure counter for fail-safe policy. */
    int32 ConsecutiveRuntimeErrorCount = 0;
    /** Per-component Lua script instance (environment is not shared with other actors). */
    std::shared_ptr<FLuaScriptInstance> ScriptInstance;
    TArray<FLuaCoroutineState> ActiveCoroutines;
    TArray<FLuaCoroutineState> PendingCoroutines;
    bool bTickingCoroutines = false;
};
