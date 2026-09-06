#include "Core/EnginePCH.h"
#include "Engine/Component/Script/ScriptComponent.h"

#include "Engine/Core/CollisionTypes.h"
#include "Engine/Core/Paths.h"
#include "Engine/Runtime/Engine.h"
#include "Engine/Scripting/LuaScriptSubsystem.h"

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
    const bool bResult = TryCallPreferred({ "OnOverlapBegin", "OnOverlap" }, &bHadCallback, OtherActor);
    ApplyRuntimeFailurePolicy(bHadCallback, bResult, "OnOverlapBegin/OnOverlap");
}

void UScriptComponent::OnOverlapEnd(AActor* OtherActor)
{
    if (!ScriptInstance || !ScriptInstance->bLoaded)
    {
        return;
    }

    bool bHadCallback = false;
    const bool bResult = TryCallPreferred({ "OnOverlapEnd" }, &bHadCallback, OtherActor);
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
    const bool bResult = TryCallPreferred({ "OnDestroy" }, &bHadCallback);
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
        return LuaSubsystem.CallFunction(ScriptInstance, CallbackName, GetOwner(), std::forward<Args>(args)...);
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
