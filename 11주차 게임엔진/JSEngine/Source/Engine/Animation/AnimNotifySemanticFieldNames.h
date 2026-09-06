#pragma once

#include "Core/CoreMinimal.h"

// Semantic field ids are the shared identity layer above raw payload strings.
// Runtime, validation, schema, and editor helpers all converge here when they
// need canonical keys, legacy aliases, or lookup-key sets for one logical field.
enum class EAnimNotifySemanticFieldId : uint8
{
    None = 0,
    SoundCue,
    SocketName,
    VolumeMultiplier,
    Spatialized,
    ComponentName,
    AttackId,
    EventName,
    EndEventName,
    PayloadValue,
    Effect,
    Decal,
    Attached,
    Shake,
    Scale,
    TraceDistance,
    Size,
    Lifetime,
    AlignToHitNormal,
};

namespace AnimNotifySemanticFieldNames
{
    // Canonical key accessors are the source of truth used when new payload text
    // is written back out. Legacy aliases are only for tolerant reads.
    const FString& SoundCueKey();
    const FString& LegacySoundKey();

    const FString& SocketNameKey();
    const FString& LegacySocketKey();

    const FString& VolumeMultiplierKey();
    const FString& LegacyVolumeKey();

    const FString& SpatializedKey();

    const FString& ComponentNameKey();
    const FString& LegacyComponentKey();

    const FString& AttackIdKey();
    const FString& EventNameKey();
    const FString& EndEventNameKey();
    const FString& PayloadKey();
    const FString& EffectKey();
    const FString& DecalKey();
    const FString& AttachedKey();
    const FString& ShakeKey();
    const FString& ScaleKey();
    const FString& TraceDistanceKey();
    const FString& SizeKey();
    const FString& LifetimeKey();
    const FString& AlignToHitNormalKey();

    // Semantic-id lookup stays keyed by canonical field names only. Callers that
    // need tolerant reads should use GetLookupKeys(...), not this mapping.
    EAnimNotifySemanticFieldId GetSemanticFieldId(const FString& CanonicalKey);
    const FString& GetCanonicalKey(EAnimNotifySemanticFieldId SemanticId);
    const TArray<FString>& GetLegacyAliases(EAnimNotifySemanticFieldId SemanticId);
    const TArray<FString>& GetLookupKeys(EAnimNotifySemanticFieldId SemanticId);

    // String-key overloads are convenience helpers for schema/editor code that
    // already starts from a canonical field name instead of a semantic id.
    const TArray<FString>& GetLegacyAliases(const FString& CanonicalKey);
    const TArray<FString>& GetLookupKeys(const FString& CanonicalKey);
}
