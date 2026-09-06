#include "Core/EnginePCH.h"
#include "Engine/Component/Script/ScriptComponent.h"

#include "Engine/Core/CollisionTypes.h"
#include "Engine/Core/Paths.h"
#include "Engine/GameFramework/World.h"
#include "Engine/Runtime/Engine.h"
#include "Engine/Scripting/LuaScriptSubsystem.h"

#include <algorithm>

namespace
{
    // Fail-safe threshold for consecutive callback runtime failures.
    constexpr int32 MaxConsecutiveRuntimeFailures = 3;
}

DEFINE_CLASS(UScriptComponent, UActorComponent)
REGISTER_FACTORY(UScriptComponent)

void UScriptComponent::BeginPlay()
{
    bHasBegunPlay = true;
    // Reset runtime phase for every play session / PIE entry.
    RuntimeState = EScriptRuntimeState::Idle;
    ConsecutiveRuntimeErrorCount = 0;
    ClearCoroutines();

    // Keep runtime active state aligned with serialized toggle at PIE/Game start.
    // This prevents stale runtime-only bIsActive values from silently blocking updates.
    SetActive(bSerializedEnabled);

    if (!EnsureScriptInstance())
    {
        return;
    }

    if (IsActive())
    {
        StartScriptIfNeeded();
        if (IsActive())
        {
            NotifyScriptEnabled();
        }
    }
}

void UScriptComponent::EndPlay()
{
    NotifyScriptDestroyed();
    ClearCoroutines();

    if (ScriptInstance)
    {
        GEngine->GetLuaScriptSubsystem().DestroyScriptInstance(ScriptInstance);
        ScriptInstance.reset();
    }

    bHasBegunPlay = false;
    RuntimeState = EScriptRuntimeState::Idle;
    ConsecutiveRuntimeErrorCount = 0;
}

void UScriptComponent::TickComponent(float DeltaTime)
{
    // World lifecycle gate + activation gate.
    if (!CanDispatchCallbacks())
    {
        return;
    }

    StartScriptIfNeeded();
    if (!IsActive())
    {
        return;
    }

    bool bHadCallback = false;
    const bool bResult = TryCallPreferred({ "OnUpdate", "Tick" }, &bHadCallback, DeltaTime);
    ApplyRuntimeFailurePolicy(bHadCallback, bResult, "OnUpdate/Tick");
    if (!IsActive())
    {
        ClearCoroutines();
        return;
    }

    UWorld* World = GetOwner() ? GetOwner()->GetFocusedWorld() : nullptr;
    TickCoroutines(World ? World->GetUnscaledDeltaTime() : DeltaTime);
}

void UScriptComponent::Activate()
{
    UActorComponent::Activate();
    bSerializedEnabled = true;

    if (!bHasBegunPlay)
    {
        return;
    }

    if (!EnsureScriptInstance())
    {
        return;
    }

    StartScriptIfNeeded();
    if (IsActive())
    {
        NotifyScriptEnabled();
    }
}

void UScriptComponent::Deactivate()
{
    if (bHasBegunPlay)
    {
        NotifyScriptDisabled();
    }

    bSerializedEnabled = false;
    UActorComponent::Deactivate();
}

void UScriptComponent::Serialize(FArchive& Ar)
{
    UActorComponent::Serialize(Ar);
    Ar << "ScriptPath" << ScriptPath;
    Ar << "ScriptEnabled" << bSerializedEnabled;

    if (Ar.IsLoading())
    {
        ScriptPath = FPaths::Normalize(ScriptPath);
        SetActive(bSerializedEnabled);
    }
}

void UScriptComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    UActorComponent::GetEditableProperties(OutProps);
    OutProps.push_back({ "Script Path", EPropertyType::String, &ScriptPath });
    OutProps.push_back({ "Script Enabled", EPropertyType::Bool, &bSerializedEnabled });
}

void UScriptComponent::PostEditProperty(const char* PropertyName)
{
    UActorComponent::PostEditProperty(PropertyName);

    switch (PropertyNameId(PropertyName))
    {
    case PropertyNameIdConstexpr("Script Path"):
        ScriptPath = FPaths::Normalize(ScriptPath);
        if (bHasBegunPlay)
        {
            ReloadScript();
        }
        return;

    case PropertyNameIdConstexpr("Script Enabled"):
        SetActive(bSerializedEnabled);
        return;

    default:
        return;
    }
}

void UScriptComponent::OnOverlap(AActor* OtherActor)
{
    OnOverlapBegin(OtherActor);
}

void UScriptComponent::OnOverlapBegin(AActor* OtherActor)
{
    if (!ScriptInstance || !ScriptInstance->bLoaded)
    {
        return;
    }

    bool bHadCallback = false;
    const bool bResult = TryCallPreferred({ "OnOverlapBegin", "OnBeginOverlap", "OnOverlap" }, &bHadCallback, OtherActor);
    ApplyRuntimeFailurePolicy(bHadCallback, bResult, "OnOverlapBegin/OnOverlap");
}

void UScriptComponent::OnOverlapEnd(AActor* OtherActor)
{
    if (!ScriptInstance || !ScriptInstance->bLoaded)
    {
        return;
    }

    bool bHadCallback = false;
    const bool bResult = TryCallPreferred({ "OnOverlapEnd", "OnEndOverlap" }, &bHadCallback, OtherActor);
    ApplyRuntimeFailurePolicy(bHadCallback, bResult, "OnOverlapEnd");
}

void UScriptComponent::OnHit(AActor* OtherActor)
{
    FHitResult EmptyHitInfo;
    OnHit(OtherActor, EmptyHitInfo);
}

void UScriptComponent::OnHit(AActor* OtherActor, const FHitResult& HitInfo)
{
    if (!ScriptInstance || !ScriptInstance->bLoaded)
    {
        return;
    }

    bool bHadCallback = false;
    const bool bResult = TryCallPreferred({ "OnHit" }, &bHadCallback, OtherActor, HitInfo);
    ApplyRuntimeFailurePolicy(bHadCallback, bResult, "OnHit");
}

bool UScriptComponent::ReloadScript()
{
    if (!ScriptInstance)
    {
        return EnsureScriptInstance();
    }

    NotifyScriptDestroyed();
    ClearCoroutines();

    const bool bReloaded = GEngine->GetLuaScriptSubsystem().ReloadScript(ScriptInstance);
    RuntimeState = EScriptRuntimeState::Idle;
    ConsecutiveRuntimeErrorCount = 0;

    if (!bReloaded)
    {
        DisableScriptAfterFatalError();
        return false;
    }

    if (bHasBegunPlay)
    {
        // Re-sync active state from serialized toggle for deterministic reload behavior.
        SetActive(bSerializedEnabled);
        if (IsActive())
        {
            StartScriptIfNeeded();
            NotifyScriptEnabled();
        }
    }

    return true;
}

void UScriptComponent::StartCoroutine(const sol::function& Function)
{
    if (!ScriptInstance || !ScriptInstance->bLoaded || !Function.valid())
    {
        return;
    }

    lua_State* MainState = ScriptInstance->Env.lua_state();
    if (MainState == nullptr)
    {
        return;
    }

    lua_State* ThreadState = lua_newthread(MainState);
    if (ThreadState == nullptr)
    {
        lua_pop(MainState, 1);
        return;
    }

    const int ThreadRegistryRef = luaL_ref(MainState, LUA_REGISTRYINDEX);
    sol::stack::push(MainState, Function);
    lua_xmove(MainState, ThreadState, 1);

    FLuaCoroutineState CoroutineState;
    CoroutineState.Thread = ThreadState;
    CoroutineState.ThreadRegistryRef = ThreadRegistryRef;
    CoroutineState.WaitRemaining = 0.0f;

    if (ResumeCoroutine(CoroutineState))
    {
        if (bTickingCoroutines)
        {
            PendingCoroutines.push_back(CoroutineState);
        }
        else
        {
            ActiveCoroutines.push_back(CoroutineState);
        }
        return;
    }

    luaL_unref(MainState, LUA_REGISTRYINDEX, ThreadRegistryRef);
}

void UScriptComponent::SetScriptPath(const FString& InPath)
{
    ScriptPath = FPaths::Normalize(InPath);
}

bool UScriptComponent::IsScriptLoaded() const
{
    return ScriptInstance && ScriptInstance->bLoaded;
}

bool UScriptComponent::CanDispatchCallbacks() const
{
    return bHasBegunPlay && bIsActive && ScriptInstance && ScriptInstance->bLoaded;
}

bool UScriptComponent::EnsureScriptInstance()
{
    if (ScriptPath.empty())
    {
        return false;
    }

    if (ScriptInstance)
    {
        return ScriptInstance->bLoaded;
    }

    // One ScriptComponent owns one instance; same lua file on different actors stays isolated.
    ScriptInstance = GEngine->GetLuaScriptSubsystem().CreateScriptInstance(GetOwner(), this, ScriptPath);
    if (!ScriptInstance || !ScriptInstance->bLoaded)
    {
        DisableScriptAfterFatalError();
        return false;
    }

    return true;
}

void UScriptComponent::StartScriptIfNeeded()
{
    // Start callback is one-shot per runtime cycle.
    if (RuntimeState != EScriptRuntimeState::Idle || !ScriptInstance || !ScriptInstance->bLoaded)
    {
        return;
    }

    bool bHadCallback = false;
    const bool bResult = TryCallPreferred({ "OnStart", "BeginPlay" }, &bHadCallback);
    if (!bResult && bHadCallback)
    {
        DisableScriptAfterFatalError();
        return;
    }

    RuntimeState = EScriptRuntimeState::Started;
}

void UScriptComponent::NotifyScriptEnabled()
{
    // Failed/Destroyed are terminal states for this runtime cycle.
    if (RuntimeState == EScriptRuntimeState::Enabled
        || RuntimeState == EScriptRuntimeState::Failed
        || RuntimeState == EScriptRuntimeState::Destroyed
        || !ScriptInstance
        || !ScriptInstance->bLoaded)
    {
        return;
    }

    bool bHadCallback = false;
    const bool bResult = TryCallPreferred({ "OnEnable" }, &bHadCallback);
    if (!bResult && bHadCallback)
    {
        ApplyRuntimeFailurePolicy(true, false, "OnEnable");
        return;
    }

    ApplyRuntimeFailurePolicy(bHadCallback, true, "OnEnable");
    RuntimeState = EScriptRuntimeState::Enabled;
}

void UScriptComponent::NotifyScriptDisabled()
{
    if (RuntimeState != EScriptRuntimeState::Enabled || !ScriptInstance || !ScriptInstance->bLoaded)
    {
        return;
    }

    bool bHadCallback = false;
    const bool bResult = TryCallPreferred({ "OnDisable" }, &bHadCallback);
    if (!bResult && bHadCallback)
    {
        ApplyRuntimeFailurePolicy(true, false, "OnDisable");
        return;
    }

    ApplyRuntimeFailurePolicy(bHadCallback, true, "OnDisable");
    RuntimeState = EScriptRuntimeState::Started;
}

void UScriptComponent::NotifyScriptDestroyed()
{
    // Destroy callback is delivered once, and disable notification is sent first when needed.
    if (RuntimeState == EScriptRuntimeState::Destroyed || !ScriptInstance || !ScriptInstance->bLoaded)
    {
        return;
    }

    NotifyScriptDisabled();

    bool bHadCallback = false;
    const bool bResult = TryCallPreferred({ "OnDestroy", "EndPlay" }, &bHadCallback);
    if (!bResult && bHadCallback)
    {
        ApplyRuntimeFailurePolicy(true, false, "OnDestroy");
        return;
    }

    ApplyRuntimeFailurePolicy(bHadCallback, true, "OnDestroy");
    RuntimeState = EScriptRuntimeState::Destroyed;
}

template<typename... Args>
bool UScriptComponent::TryCallPreferred(
    const std::initializer_list<const char*>& CallbackNames,
    bool* bOutHadCallback,
    Args&&... args)
{
    if (!ScriptInstance || !ScriptInstance->bLoaded)
    {
        if (bOutHadCallback)
        {
            *bOutHadCallback = false;
        }
        return false;
    }

    if (bOutHadCallback)
    {
        *bOutHadCallback = false;
    }

    FLuaScriptSubsystem& LuaSubsystem = GEngine->GetLuaScriptSubsystem();
    // Choose first callback that exists. Missing callback is an allowed no-op.
    for (const char* CallbackName : CallbackNames)
    {
        if (!LuaSubsystem.HasFunction(ScriptInstance, CallbackName))
        {
            continue;
        }

        if (bOutHadCallback)
        {
            *bOutHadCallback = true;
        }
        return LuaSubsystem.CallFunction(ScriptInstance, CallbackName, std::forward<Args>(args)...);
    }

    return true;
}

void UScriptComponent::DisableScriptAfterFatalError()
{
    bIsActive = false;
    bSerializedEnabled = false;
    bCanEverTick = false;
    RuntimeState = EScriptRuntimeState::Failed;
}

void UScriptComponent::ApplyRuntimeFailurePolicy(bool bHadCallback, bool bSucceeded, const char* CallbackContext)
{
    if (ShouldDisableAfterRuntimeFailure(bHadCallback, bSucceeded))
    {
        UE_LOG("[Lua] Disabling script after %d consecutive callback failures: %s\n",
            ConsecutiveRuntimeErrorCount,
            ScriptPath.c_str());
        if (CallbackContext && CallbackContext[0] != '\0')
        {
            UE_LOG("[Lua] Last failed callback context: %s\n", CallbackContext);
        }
        DisableScriptAfterFatalError();
        if (!bTickingCoroutines)
        {
            ClearCoroutines();
        }
    }
}

bool UScriptComponent::ShouldDisableAfterRuntimeFailure(bool bHadCallback, bool bSucceeded)
{
    // Optional callbacks should not be treated as failures when absent.
    if (!bHadCallback)
    {
        return false;
    }

    if (bSucceeded)
    {
        ConsecutiveRuntimeErrorCount = 0;
        return false;
    }

    ++ConsecutiveRuntimeErrorCount;
    return ConsecutiveRuntimeErrorCount >= MaxConsecutiveRuntimeFailures;
}

void UScriptComponent::TickCoroutines(float UnscaledDeltaTime)
{
    if (ActiveCoroutines.empty())
    {
        return;
    }

    bTickingCoroutines = true;
    for (int32 CoroutineIndex = static_cast<int32>(ActiveCoroutines.size()) - 1; CoroutineIndex >= 0; --CoroutineIndex)
    {
        FLuaCoroutineState& CoroutineState = ActiveCoroutines[CoroutineIndex];
        if (CoroutineState.Thread == nullptr)
        {
            ActiveCoroutines.erase(ActiveCoroutines.begin() + CoroutineIndex);
            continue;
        }

        if (CoroutineState.WaitRemaining > 0.0f)
        {
            CoroutineState.WaitRemaining = std::max(0.0f, CoroutineState.WaitRemaining - UnscaledDeltaTime);
            if (CoroutineState.WaitRemaining > 0.0f)
            {
                continue;
            }
        }

        if (ResumeCoroutine(CoroutineState))
        {
            continue;
        }

        if (!IsActive())
        {
            ClearCoroutines();
            return;
        }

        if (ScriptInstance)
        {
            luaL_unref(ScriptInstance->Env.lua_state(), LUA_REGISTRYINDEX, CoroutineState.ThreadRegistryRef);
        }
        ActiveCoroutines.erase(ActiveCoroutines.begin() + CoroutineIndex);
    }
    bTickingCoroutines = false;

    if (!PendingCoroutines.empty())
    {
        ActiveCoroutines.insert(ActiveCoroutines.end(), PendingCoroutines.begin(), PendingCoroutines.end());
        PendingCoroutines.clear();
    }
}

bool UScriptComponent::ResumeCoroutine(FLuaCoroutineState& CoroutineState)
{
    if (CoroutineState.Thread == nullptr)
    {
        return false;
    }

    int ResultCount = 0;
    const int ResumeStatus = lua_resume(CoroutineState.Thread, nullptr, 0, &ResultCount);
    if (ResumeStatus == LUA_YIELD)
    {
        float WaitSeconds = 0.0f;
        if (ResultCount > 0 && lua_isnumber(CoroutineState.Thread, -1))
        {
            WaitSeconds = static_cast<float>(lua_tonumber(CoroutineState.Thread, -1));
        }

        lua_settop(CoroutineState.Thread, 0);
        CoroutineState.WaitRemaining = std::max(0.0f, WaitSeconds);
        return true;
    }

    if (ResumeStatus == LUA_OK)
    {
        lua_settop(CoroutineState.Thread, 0);
        return false;
    }

    const char* ErrorMessage = lua_tostring(CoroutineState.Thread, -1);
    UE_LOG("[Lua Coroutine Error] %s (%s)\n",
        ErrorMessage ? ErrorMessage : "Unknown coroutine failure",
        ScriptPath.c_str());
    lua_settop(CoroutineState.Thread, 0);
    ApplyRuntimeFailurePolicy(true, false, "Coroutine");
    return false;
}

void UScriptComponent::ClearCoroutines()
{
    if (ScriptInstance)
    {
        lua_State* MainState = ScriptInstance->Env.lua_state();
        if (MainState != nullptr)
        {
            for (FLuaCoroutineState& CoroutineState : ActiveCoroutines)
            {
                if (CoroutineState.ThreadRegistryRef != LUA_NOREF)
                {
                    luaL_unref(MainState, LUA_REGISTRYINDEX, CoroutineState.ThreadRegistryRef);
                }
            }

            for (FLuaCoroutineState& CoroutineState : PendingCoroutines)
            {
                if (CoroutineState.ThreadRegistryRef != LUA_NOREF)
                {
                    luaL_unref(MainState, LUA_REGISTRYINDEX, CoroutineState.ThreadRegistryRef);
                }
            }
        }
    }

    ActiveCoroutines.clear();
    PendingCoroutines.clear();
    bTickingCoroutines = false;
}
