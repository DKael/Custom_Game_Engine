#pragma once

#include "Core/CoreMinimal.h"
#include "Core/Guid.h"
#include "Math/Color.h"
#include "Object/FName.h"

#include <algorithm>
#include <cctype>

enum class EAnimNotifyEventType : uint8
{
    Notify = 0,
    NotifyState,
};

enum class EAnimNotifyTriggerPhase : uint8
{
    None = 0,
    Notify,
    Begin,
    Tick,
    End,
};

inline FString AnimNotifyEventTypeToString(EAnimNotifyEventType NotifyType)
{
    switch (NotifyType)
    {
    case EAnimNotifyEventType::NotifyState:
        return "NotifyState";
    case EAnimNotifyEventType::Notify:
    default:
        return "Notify";
    }
}

inline EAnimNotifyEventType AnimNotifyEventTypeFromString(FString Value)
{
    std::transform(
        Value.begin(),
        Value.end(),
        Value.begin(),
        [](unsigned char Character)
        {
            return static_cast<char>(std::tolower(Character));
        });

    if (Value == "notifystate" || Value == "state")
    {
        return EAnimNotifyEventType::NotifyState;
    }

    return EAnimNotifyEventType::Notify;
}

inline FString AnimNotifyTriggerPhaseToString(EAnimNotifyTriggerPhase Phase)
{
    switch (Phase)
    {
    case EAnimNotifyTriggerPhase::Notify:
        return "Notify";
    case EAnimNotifyTriggerPhase::Begin:
        return "Begin";
    case EAnimNotifyTriggerPhase::Tick:
        return "Tick";
    case EAnimNotifyTriggerPhase::End:
        return "End";
    case EAnimNotifyTriggerPhase::None:
    default:
        return "None";
    }
}

inline FString GetDefaultAnimNotifyClassName(EAnimNotifyEventType NotifyType)
{
    return NotifyType == EAnimNotifyEventType::NotifyState ? "UAnimNotifyState" : "UAnimNotify";
}

struct FAnimNotifyEvent
{
    FGuid StableId;
    FName Name;
    float Time = 0.0f;
    float Duration = 0.0f;
    FColor Color = FColor(0.9490196f, 0.61960787f, 0.23921569f, 1.0f);
    EAnimNotifyEventType EventType = EAnimNotifyEventType::Notify;
    FString NotifyClassName = GetDefaultAnimNotifyClassName(EAnimNotifyEventType::Notify);
    FString Payload;
    EAnimNotifyTriggerPhase TriggerPhase = EAnimNotifyTriggerPhase::None;
    // Runtime-only debug metadata used by recent-fired notify summaries.
    FName SourceTrackName;
    FString SourceSequencePath;
    FString SourceSequenceName;
    int32 SourceTrackIndex = -1;

    void EnsureStableId()
    {
        if (!StableId.IsValid())
        {
            StableId = FGuid::NewGuid();
        }
    }

    bool IsState() const
    {
        return EventType == EAnimNotifyEventType::NotifyState;
    }

    FString GetResolvedNotifyClassName() const
    {
        return NotifyClassName.empty() ? GetDefaultAnimNotifyClassName(EventType) : NotifyClassName;
    }
};

struct FAnimNotifyTrack
{
    FName TrackName;
    TArray<FAnimNotifyEvent> Events;
};
