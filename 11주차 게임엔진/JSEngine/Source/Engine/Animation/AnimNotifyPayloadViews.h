#pragma once

#include "Core/CoreMinimal.h"
#include "Object/FName.h"

class FAnimNotifyPayloadParser;

// Typed payload views sit above the raw parser and below runtime/validation
// consumers. They expose notify-specific interpretation without owning
// serialization, editor metadata, or runtime dispatch behavior.
struct FPlaySfxPayloadView
{
    // These defaults intentionally mirror longstanding runtime behavior so old
    // authored payloads keep the same meaning when optional fields are absent.
    FString SoundCue;
    FName SocketName = FName::None;
    float VolumeMultiplier = 1.0f;
    bool bSpatialized = false;
};

struct FPlayVfxPayloadView
{
    // Attached/scale defaults stay here so runtime callers can consume one
    // semantic object instead of repeating raw-parser fallback rules.
    FString Effect;
    FName SocketName = FName::None;
    bool bAttached = false;
    float Scale = 1.0f;
};

struct FCameraShakePayloadView
{
    FString Shake;
    float Scale = 1.0f;
};

// Builders consume semantic lookup aliases and collapse them into one notify-
// specific view. They do not validate required fields; callers still decide
// whether missing values are errors, warnings, or runtime no-ops.
FPlaySfxPayloadView BuildPlaySfxPayloadView(const FAnimNotifyPayloadParser& Payload);
FPlayVfxPayloadView BuildPlayVfxPayloadView(const FAnimNotifyPayloadParser& Payload);
FCameraShakePayloadView BuildCameraShakePayloadView(const FAnimNotifyPayloadParser& Payload);
