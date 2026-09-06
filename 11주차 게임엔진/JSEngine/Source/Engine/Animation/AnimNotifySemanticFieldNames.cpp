#include "Animation/AnimNotifySemanticFieldNames.h"

namespace
{
    const TArray<FString>& EmptyKeys()
    {
        static const TArray<FString> Keys;
        return Keys;
    }

    const FString& EmptyKey()
    {
        static const FString Key;
        return Key;
    }
}

namespace AnimNotifySemanticFieldNames
{
    const FString& SoundCueKey()
    {
        static const FString Key = "SoundCue";
        return Key;
    }

    const FString& LegacySoundKey()
    {
        static const FString Key = "Sound";
        return Key;
    }

    const FString& SocketNameKey()
    {
        static const FString Key = "SocketName";
        return Key;
    }

    const FString& LegacySocketKey()
    {
        static const FString Key = "Socket";
        return Key;
    }

    const FString& VolumeMultiplierKey()
    {
        static const FString Key = "VolumeMultiplier";
        return Key;
    }

    const FString& LegacyVolumeKey()
    {
        static const FString Key = "Volume";
        return Key;
    }

    const FString& SpatializedKey()
    {
        static const FString Key = "Spatialized";
        return Key;
    }

    const FString& ComponentNameKey()
    {
        static const FString Key = "ComponentName";
        return Key;
    }

    const FString& LegacyComponentKey()
    {
        static const FString Key = "Component";
        return Key;
    }

    const FString& AttackIdKey()
    {
        static const FString Key = "AttackId";
        return Key;
    }

    const FString& EventNameKey()
    {
        static const FString Key = "EventName";
        return Key;
    }

    const FString& EndEventNameKey()
    {
        static const FString Key = "EndEventName";
        return Key;
    }

    const FString& PayloadKey()
    {
        static const FString Key = "Payload";
        return Key;
    }

    const FString& EffectKey()
    {
        static const FString Key = "Effect";
        return Key;
    }

    const FString& DecalKey()
    {
        static const FString Key = "Decal";
        return Key;
    }

    const FString& AttachedKey()
    {
        static const FString Key = "Attached";
        return Key;
    }

    const FString& ShakeKey()
    {
        static const FString Key = "Shake";
        return Key;
    }

    const FString& ScaleKey()
    {
        static const FString Key = "Scale";
        return Key;
    }

    const FString& TraceDistanceKey()
    {
        static const FString Key = "TraceDistance";
        return Key;
    }

    const FString& SizeKey()
    {
        static const FString Key = "Size";
        return Key;
    }

    const FString& LifetimeKey()
    {
        static const FString Key = "Lifetime";
        return Key;
    }

    const FString& AlignToHitNormalKey()
    {
        static const FString Key = "AlignToHitNormal";
        return Key;
    }

    EAnimNotifySemanticFieldId GetSemanticFieldId(const FString& CanonicalKey)
    {
        if (CanonicalKey == SoundCueKey())
        {
            return EAnimNotifySemanticFieldId::SoundCue;
        }

        if (CanonicalKey == SocketNameKey())
        {
            return EAnimNotifySemanticFieldId::SocketName;
        }

        if (CanonicalKey == VolumeMultiplierKey())
        {
            return EAnimNotifySemanticFieldId::VolumeMultiplier;
        }

        if (CanonicalKey == SpatializedKey())
        {
            return EAnimNotifySemanticFieldId::Spatialized;
        }

        if (CanonicalKey == ComponentNameKey())
        {
            return EAnimNotifySemanticFieldId::ComponentName;
        }

        if (CanonicalKey == AttackIdKey())
        {
            return EAnimNotifySemanticFieldId::AttackId;
        }

        if (CanonicalKey == EventNameKey())
        {
            return EAnimNotifySemanticFieldId::EventName;
        }

        if (CanonicalKey == EndEventNameKey())
        {
            return EAnimNotifySemanticFieldId::EndEventName;
        }

        if (CanonicalKey == PayloadKey())
        {
            return EAnimNotifySemanticFieldId::PayloadValue;
        }

        if (CanonicalKey == EffectKey())
        {
            return EAnimNotifySemanticFieldId::Effect;
        }

        if (CanonicalKey == DecalKey())
        {
            return EAnimNotifySemanticFieldId::Decal;
        }

        if (CanonicalKey == AttachedKey())
        {
            return EAnimNotifySemanticFieldId::Attached;
        }

        if (CanonicalKey == ShakeKey())
        {
            return EAnimNotifySemanticFieldId::Shake;
        }

        if (CanonicalKey == ScaleKey())
        {
            return EAnimNotifySemanticFieldId::Scale;
        }

        if (CanonicalKey == TraceDistanceKey())
        {
            return EAnimNotifySemanticFieldId::TraceDistance;
        }

        if (CanonicalKey == SizeKey())
        {
            return EAnimNotifySemanticFieldId::Size;
        }

        if (CanonicalKey == LifetimeKey())
        {
            return EAnimNotifySemanticFieldId::Lifetime;
        }

        if (CanonicalKey == AlignToHitNormalKey())
        {
            return EAnimNotifySemanticFieldId::AlignToHitNormal;
        }

        return EAnimNotifySemanticFieldId::None;
    }

    const FString& GetCanonicalKey(EAnimNotifySemanticFieldId SemanticId)
    {
        switch (SemanticId)
        {
        case EAnimNotifySemanticFieldId::SoundCue:
            return SoundCueKey();
        case EAnimNotifySemanticFieldId::SocketName:
            return SocketNameKey();
        case EAnimNotifySemanticFieldId::VolumeMultiplier:
            return VolumeMultiplierKey();
        case EAnimNotifySemanticFieldId::Spatialized:
            return SpatializedKey();
        case EAnimNotifySemanticFieldId::ComponentName:
            return ComponentNameKey();
        case EAnimNotifySemanticFieldId::AttackId:
            return AttackIdKey();
        case EAnimNotifySemanticFieldId::EventName:
            return EventNameKey();
        case EAnimNotifySemanticFieldId::EndEventName:
            return EndEventNameKey();
        case EAnimNotifySemanticFieldId::PayloadValue:
            return PayloadKey();
        case EAnimNotifySemanticFieldId::Effect:
            return EffectKey();
        case EAnimNotifySemanticFieldId::Decal:
            return DecalKey();
        case EAnimNotifySemanticFieldId::Attached:
            return AttachedKey();
        case EAnimNotifySemanticFieldId::Shake:
            return ShakeKey();
        case EAnimNotifySemanticFieldId::Scale:
            return ScaleKey();
        case EAnimNotifySemanticFieldId::TraceDistance:
            return TraceDistanceKey();
        case EAnimNotifySemanticFieldId::Size:
            return SizeKey();
        case EAnimNotifySemanticFieldId::Lifetime:
            return LifetimeKey();
        case EAnimNotifySemanticFieldId::AlignToHitNormal:
            return AlignToHitNormalKey();
        case EAnimNotifySemanticFieldId::None:
        default:
            return EmptyKey();
        }
    }

    const TArray<FString>& GetLegacyAliases(EAnimNotifySemanticFieldId SemanticId)
    {
        static const TArray<FString> SoundCueAliases = { LegacySoundKey() };
        static const TArray<FString> SocketNameAliases = { LegacySocketKey() };
        static const TArray<FString> VolumeMultiplierAliases = { LegacyVolumeKey() };
        static const TArray<FString> ComponentNameAliases = { LegacyComponentKey() };

        switch (SemanticId)
        {
        case EAnimNotifySemanticFieldId::SoundCue:
            return SoundCueAliases;
        case EAnimNotifySemanticFieldId::SocketName:
            return SocketNameAliases;
        case EAnimNotifySemanticFieldId::VolumeMultiplier:
            return VolumeMultiplierAliases;
        case EAnimNotifySemanticFieldId::ComponentName:
            return ComponentNameAliases;
        case EAnimNotifySemanticFieldId::None:
        case EAnimNotifySemanticFieldId::Spatialized:
        case EAnimNotifySemanticFieldId::AttackId:
        case EAnimNotifySemanticFieldId::EventName:
        case EAnimNotifySemanticFieldId::EndEventName:
        case EAnimNotifySemanticFieldId::PayloadValue:
        case EAnimNotifySemanticFieldId::Effect:
        case EAnimNotifySemanticFieldId::Decal:
        case EAnimNotifySemanticFieldId::Attached:
        case EAnimNotifySemanticFieldId::Shake:
        case EAnimNotifySemanticFieldId::Scale:
        case EAnimNotifySemanticFieldId::TraceDistance:
        case EAnimNotifySemanticFieldId::Size:
        case EAnimNotifySemanticFieldId::Lifetime:
        case EAnimNotifySemanticFieldId::AlignToHitNormal:
        default:
            return EmptyKeys();
        }
    }

    const TArray<FString>& GetLookupKeys(EAnimNotifySemanticFieldId SemanticId)
    {
        static const TArray<FString> SoundCueKeys = { SoundCueKey(), LegacySoundKey() };
        static const TArray<FString> SocketNameKeys = { SocketNameKey(), LegacySocketKey() };
        static const TArray<FString> VolumeMultiplierKeys = { VolumeMultiplierKey(), LegacyVolumeKey() };
        static const TArray<FString> SpatializedKeys = { SpatializedKey() };
        static const TArray<FString> ComponentNameKeys = { ComponentNameKey(), LegacyComponentKey() };
        static const TArray<FString> AttackIdKeys = { AttackIdKey() };
        static const TArray<FString> EventNameKeys = { EventNameKey() };
        static const TArray<FString> EndEventNameKeys = { EndEventNameKey() };
        static const TArray<FString> PayloadKeys = { PayloadKey() };
        static const TArray<FString> EffectKeys = { EffectKey() };
        static const TArray<FString> DecalKeys = { DecalKey() };
        static const TArray<FString> AttachedKeys = { AttachedKey() };
        static const TArray<FString> ShakeKeys = { ShakeKey() };
        static const TArray<FString> ScaleKeys = { ScaleKey() };
        static const TArray<FString> TraceDistanceKeys = { TraceDistanceKey() };
        static const TArray<FString> SizeKeys = { SizeKey() };
        static const TArray<FString> LifetimeKeys = { LifetimeKey() };
        static const TArray<FString> AlignToHitNormalKeys = { AlignToHitNormalKey() };

        switch (SemanticId)
        {
        case EAnimNotifySemanticFieldId::SoundCue:
            return SoundCueKeys;
        case EAnimNotifySemanticFieldId::SocketName:
            return SocketNameKeys;
        case EAnimNotifySemanticFieldId::VolumeMultiplier:
            return VolumeMultiplierKeys;
        case EAnimNotifySemanticFieldId::Spatialized:
            return SpatializedKeys;
        case EAnimNotifySemanticFieldId::ComponentName:
            return ComponentNameKeys;
        case EAnimNotifySemanticFieldId::AttackId:
            return AttackIdKeys;
        case EAnimNotifySemanticFieldId::EventName:
            return EventNameKeys;
        case EAnimNotifySemanticFieldId::EndEventName:
            return EndEventNameKeys;
        case EAnimNotifySemanticFieldId::PayloadValue:
            return PayloadKeys;
        case EAnimNotifySemanticFieldId::Effect:
            return EffectKeys;
        case EAnimNotifySemanticFieldId::Decal:
            return DecalKeys;
        case EAnimNotifySemanticFieldId::Attached:
            return AttachedKeys;
        case EAnimNotifySemanticFieldId::Shake:
            return ShakeKeys;
        case EAnimNotifySemanticFieldId::Scale:
            return ScaleKeys;
        case EAnimNotifySemanticFieldId::TraceDistance:
            return TraceDistanceKeys;
        case EAnimNotifySemanticFieldId::Size:
            return SizeKeys;
        case EAnimNotifySemanticFieldId::Lifetime:
            return LifetimeKeys;
        case EAnimNotifySemanticFieldId::AlignToHitNormal:
            return AlignToHitNormalKeys;
        case EAnimNotifySemanticFieldId::None:
        default:
            return EmptyKeys();
        }
    }

    const TArray<FString>& GetLegacyAliases(const FString& CanonicalKey)
    {
        return GetLegacyAliases(GetSemanticFieldId(CanonicalKey));
    }

    const TArray<FString>& GetLookupKeys(const FString& CanonicalKey)
    {
        return GetLookupKeys(GetSemanticFieldId(CanonicalKey));
    }
}
