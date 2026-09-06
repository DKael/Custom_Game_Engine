#pragma once

#include "Editor/Animation/AnimationSequenceNotifyPayloadSchema.h"

struct FNotifyAssetPickerEntry
{
    FString DisplayName;
    // Canonical payload value written into the notify payload string.
    // This stays stable even when the picker chooses to show a friendlier label.
    FString StoredValue;
};

TArray<FNotifyAssetPickerEntry> BuildNotifyAssetPickerEntries(EAnimNotifyPayloadAssetKind AssetKind);
FString ResolveNotifyAssetPickerDisplayName(
    EAnimNotifyPayloadAssetKind AssetKind,
    const FString& StoredValue,
    const TArray<FNotifyAssetPickerEntry>& Entries);
const char* GetNotifyAssetPickerEmptyMessage(EAnimNotifyPayloadAssetKind AssetKind);
const char* GetNotifyAssetPickerManualFallbackMessage(EAnimNotifyPayloadAssetKind AssetKind);
