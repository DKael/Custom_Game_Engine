#include "Editor/Animation/AnimationSequenceNotifyPayloadSchema.h"

#include "Animation/AnimNotifySemanticFieldNames.h"

#include <algorithm>

namespace
{
    FAnimNotifyPayloadFieldDefinition MakeField(
        const FString& Key,
        EAnimNotifySemanticFieldId SemanticId,
        const TArray<FString>& LegacyKeys,
        const FString& Label,
        EAnimNotifyPayloadFieldType Type,
        EAnimNotifyPayloadFieldEditorHint EditorHint,
        EAnimNotifyPayloadAssetKind AssetKind,
        bool bRequired,
        const FString& DefaultValue,
        const FString& HelpText)
    {
        FAnimNotifyPayloadFieldDefinition Field;
        Field.Key = Key;
        Field.SemanticId = SemanticId;
        Field.LegacyKeys = LegacyKeys;
        Field.Label = Label;
        Field.Type = Type;
        Field.EditorHint = EditorHint;
        Field.AssetKind = AssetKind;
        Field.bRequired = bRequired;
        Field.DefaultValue = DefaultValue;
        Field.HelpText = HelpText;
        return Field;
    }

    FAnimNotifyPayloadFieldDefinition MakeSemanticField(
        EAnimNotifySemanticFieldId SemanticId,
        const FString& Label,
        EAnimNotifyPayloadFieldType Type,
        EAnimNotifyPayloadFieldEditorHint EditorHint,
        EAnimNotifyPayloadAssetKind AssetKind,
        bool bRequired,
        const FString& DefaultValue,
        const FString& HelpText)
    {
        return MakeField(
            AnimNotifySemanticFieldNames::GetCanonicalKey(SemanticId),
            SemanticId,
            AnimNotifySemanticFieldNames::GetLegacyAliases(SemanticId),
            Label,
            Type,
            EditorHint,
            AssetKind,
            bRequired,
            DefaultValue,
            HelpText);
    }

    FAnimNotifyPayloadFieldDefinition MakeStringField(
        EAnimNotifySemanticFieldId SemanticId,
        const FString& Label,
        bool bRequired,
        const FString& DefaultValue,
        const FString& HelpText,
        EAnimNotifyPayloadFieldEditorHint EditorHint = EAnimNotifyPayloadFieldEditorHint::Default)
    {
        return MakeSemanticField(
            SemanticId,
            Label,
            EAnimNotifyPayloadFieldType::String,
            EditorHint,
            EAnimNotifyPayloadAssetKind::None,
            bRequired,
            DefaultValue,
            HelpText);
    }

    FAnimNotifyPayloadFieldDefinition MakeAssetPickerStringField(
        EAnimNotifySemanticFieldId SemanticId,
        const FString& Label,
        bool bRequired,
        const FString& DefaultValue,
        const FString& HelpText,
        EAnimNotifyPayloadAssetKind AssetKind)
    {
        return MakeSemanticField(
            SemanticId,
            Label,
            EAnimNotifyPayloadFieldType::String,
            EAnimNotifyPayloadFieldEditorHint::AssetPicker,
            AssetKind,
            bRequired,
            DefaultValue,
            HelpText);
    }

    FAnimNotifyPayloadFieldDefinition MakeNameField(
        EAnimNotifySemanticFieldId SemanticId,
        const FString& Label,
        bool bRequired,
        const FString& DefaultValue,
        const FString& HelpText,
        EAnimNotifyPayloadFieldEditorHint EditorHint = EAnimNotifyPayloadFieldEditorHint::Default)
    {
        return MakeSemanticField(
            SemanticId,
            Label,
            EAnimNotifyPayloadFieldType::Name,
            EditorHint,
            EAnimNotifyPayloadAssetKind::None,
            bRequired,
            DefaultValue,
            HelpText);
    }

    FAnimNotifyPayloadFieldDefinition MakeFloatField(
        EAnimNotifySemanticFieldId SemanticId,
        const FString& Label,
        bool bRequired,
        const FString& DefaultValue,
        const FString& HelpText)
    {
        return MakeSemanticField(
            SemanticId,
            Label,
            EAnimNotifyPayloadFieldType::Float,
            EAnimNotifyPayloadFieldEditorHint::Default,
            EAnimNotifyPayloadAssetKind::None,
            bRequired,
            DefaultValue,
            HelpText);
    }

    FAnimNotifyPayloadFieldDefinition MakeIntField(
        const FString& Key,
        EAnimNotifySemanticFieldId SemanticId,
        const TArray<FString>& LegacyKeys,
        const FString& Label,
        bool bRequired,
        const FString& DefaultValue,
        const FString& HelpText)
    {
        return MakeField(
            Key,
            SemanticId,
            LegacyKeys,
            Label,
            EAnimNotifyPayloadFieldType::Int,
            EAnimNotifyPayloadFieldEditorHint::Default,
            EAnimNotifyPayloadAssetKind::None,
            bRequired,
            DefaultValue,
            HelpText);
    }

    FAnimNotifyPayloadFieldDefinition MakeBoolField(
        EAnimNotifySemanticFieldId SemanticId,
        const FString& Label,
        bool bRequired,
        const FString& DefaultValue,
        const FString& HelpText)
    {
        return MakeSemanticField(
            SemanticId,
            Label,
            EAnimNotifyPayloadFieldType::Bool,
            EAnimNotifyPayloadFieldEditorHint::Default,
            EAnimNotifyPayloadAssetKind::None,
            bRequired,
            DefaultValue,
            HelpText);
    }

    const TArray<FAnimNotifyPayloadSchema>& GetAnimNotifyPayloadSchemas()
    {
        static const TArray<FAnimNotifyPayloadSchema> Schemas =
        {
            {
                "UAnimNotify_PlaySFX",
                "Audio",
                "SoundCue=Footstep_Stone;SocketName=foot_l;VolumeMultiplier=0.9;Spatialized=true",
                {
                    MakeAssetPickerStringField(EAnimNotifySemanticFieldId::SoundCue, "Sound Cue", true, FString(), "Required audio event or asset key.", EAnimNotifyPayloadAssetKind::SoundCue),
                    MakeNameField(EAnimNotifySemanticFieldId::SocketName, "Socket Name", false, FString(), "Optional playback socket on the preview mesh.", EAnimNotifyPayloadFieldEditorHint::SocketPicker),
                    MakeFloatField(EAnimNotifySemanticFieldId::VolumeMultiplier, "Volume Multiplier", false, "1.0", "Optional volume multiplier."),
                    MakeBoolField(EAnimNotifySemanticFieldId::Spatialized, "Spatialized", false, "false", "Play in 3D using the socket or mesh location.")
                }
            },
            {
                "UAnimNotify_GameplayEvent",
                "Gameplay Event",
                "EventName=Combo.Open;Payload=Light1",
                {
                    MakeStringField(EAnimNotifySemanticFieldId::EventName, "Event Name", true, FString(), "Required gameplay event name dispatched when the notify fires."),
                    MakeStringField(EAnimNotifySemanticFieldId::PayloadValue, "Payload", false, FString(), "Optional raw text payload forwarded unchanged to runtime and script consumers.")
                }
            },
            {
                "UAnimNotifyState_GameplayEventWindow",
                "Gameplay Event Window",
                "EventName=InvulnWindow;EndEventName=InvulnWindowEnd;Payload=Short",
                {
                    MakeStringField(EAnimNotifySemanticFieldId::EventName, "Event Name", true, FString(), "Required gameplay event name dispatched when the state begins. Scripts should inspect notifyEvent.TriggerPhase to distinguish begin/end when names are reused."),
                    MakeStringField(EAnimNotifySemanticFieldId::EndEventName, "End Event Name", false, FString(), "Optional gameplay event name dispatched when the state ends. If left empty, Event Name is reused and scripts should branch on notifyEvent.TriggerPhase."),
                    MakeStringField(EAnimNotifySemanticFieldId::PayloadValue, "Payload", false, FString(), "Optional event payload forwarded as raw text on both begin and end dispatches.")
                }
            },
            {
                "UAnimNotify_CameraShake",
                "Camera Shake",
                "Shake=HeavyHit;Scale=0.75",
                {
                    MakeAssetPickerStringField(EAnimNotifySemanticFieldId::Shake, "Shake", true, FString(), "Required camera shake key or preset name.", EAnimNotifyPayloadAssetKind::CameraShake),
                    MakeFloatField(EAnimNotifySemanticFieldId::Scale, "Scale", false, "1.0", "Optional camera shake intensity multiplier. The default value of 1.0 uses the authored shake strength.")
                }
            },
            {
                "UAnimNotify_PlayVFX",
                "VFX",
                "Effect=SlashArc;SocketName=weapon_tip;Attached=true;Scale=1.0",
                {
                    MakeAssetPickerStringField(EAnimNotifySemanticFieldId::Effect, "Effect", true, FString(), "Required VFX key or preset name.", EAnimNotifyPayloadAssetKind::Vfx),
                    MakeNameField(EAnimNotifySemanticFieldId::SocketName, "Socket Name", false, FString(), "Optional playback socket on the preview mesh. If omitted, the mesh or world location is used instead.", EAnimNotifyPayloadFieldEditorHint::SocketPicker),
                    MakeBoolField(EAnimNotifySemanticFieldId::Attached, "Attached", false, "false", "Whether the spawned effect should stay attached to the socket or mesh after it is triggered."),
                    MakeFloatField(EAnimNotifySemanticFieldId::Scale, "Scale", false, "1.0", "Optional VFX scale multiplier. The default value of 1.0 uses the authored effect size.")
                }
            },
            {
                "UAnimNotify_SpawnDecal",
                "Decal",
                "Decal=FootDust;SocketName=foot_l;TraceDistance=50.0;Size=1.0;Lifetime=2.0;AlignToHitNormal=true",
                {
                    MakeStringField(EAnimNotifySemanticFieldId::Decal, "Decal", true, FString(), "Required decal key or preset name."),
                    MakeNameField(EAnimNotifySemanticFieldId::SocketName, "Socket Name", false, FString(), "Optional socket used as the decal spawn origin on the preview mesh. If omitted, the mesh or world location is used instead.", EAnimNotifyPayloadFieldEditorHint::SocketPicker),
                    MakeFloatField(EAnimNotifySemanticFieldId::TraceDistance, "Trace Distance", false, "50.0", "Optional downward trace distance used to find a placement surface. If the trace misses, spawning falls back to the origin location."),
                    MakeFloatField(EAnimNotifySemanticFieldId::Size, "Size", false, "1.0", "Optional decal size multiplier. The default value of 1.0 uses the authored decal size."),
                    MakeFloatField(EAnimNotifySemanticFieldId::Lifetime, "Lifetime", false, "0.0", "Optional decal lifetime in seconds. A value of 0.0 leaves lifetime handling to the runtime consumer."),
                    MakeBoolField(EAnimNotifySemanticFieldId::AlignToHitNormal, "Align To Hit Normal", false, "true", "Whether the decal orientation should align to the traced surface normal. If disabled or no hit is found, the runtime uses a stable fallback orientation.")
                }
            },
            {
                "UAnimNotify_FootstepSurfaceEvent",
                "Footstep Surface",
                "EventName=Footstep;SocketName=foot_l;TraceDistance=25.0;Payload=Run",
                {
                    MakeStringField(EAnimNotifySemanticFieldId::EventName, "Event Name", true, FString(), "Required footstep event name to dispatch."),
                    MakeNameField(EAnimNotifySemanticFieldId::SocketName, "Socket Name", true, FString(), "Required socket used as the footstep trace origin on the preview mesh.", EAnimNotifyPayloadFieldEditorHint::SocketPicker),
                    MakeFloatField(EAnimNotifySemanticFieldId::TraceDistance, "Trace Distance", false, "25.0", "Optional downward trace distance used to resolve the surface beneath the socket. If the trace misses, runtime falls back to the Default surface."),
                    MakeStringField(EAnimNotifySemanticFieldId::PayloadValue, "Payload", false, FString(), "Optional raw payload forwarded with the footstep event.")
                }
            },
            {
                "UAnimNotifyState_PlayLoopingSFX",
                "Audio",
                "SoundCue=SwordHum;SocketName=weapon_tip;VolumeMultiplier=1.0;Spatialized=true",
                {
                    MakeStringField(EAnimNotifySemanticFieldId::SoundCue, "Sound Cue", true, FString(), "Required looping audio event or asset key."),
                    MakeNameField(EAnimNotifySemanticFieldId::SocketName, "Socket Name", false, FString(), "Optional playback socket on the preview mesh.", EAnimNotifyPayloadFieldEditorHint::SocketPicker),
                    MakeFloatField(EAnimNotifySemanticFieldId::VolumeMultiplier, "Volume Multiplier", false, "1.0", "Optional volume multiplier."),
                    MakeBoolField(EAnimNotifySemanticFieldId::Spatialized, "Spatialized", false, "false", "Track the socket or mesh location while the state is active.")
                }
            },
            {
                "UAnimNotifyState_AttackWindow",
                "Collision Window",
                "ComponentName=WeaponHitbox;AttackId=Light1;Priority=1",
                {
                    MakeStringField(EAnimNotifySemanticFieldId::ComponentName, "Component Name", true, FString(), "Primitive component name to toggle overlap events on.", EAnimNotifyPayloadFieldEditorHint::ComponentPicker),
                    MakeStringField(EAnimNotifySemanticFieldId::AttackId, "AttackId", false, FString(), "Optional attack tag suffix applied as AttackId:<value>."),
                    MakeIntField("Priority", EAnimNotifySemanticFieldId::None, {}, "Priority", false, "0", "Optional editor-facing integer field for ordering or custom tooling.")
                }
            }
        };
        return Schemas;
    }
}

const FAnimNotifyPayloadSchema* FindAnimNotifyPayloadSchema(const FString& NotifyClassName)
{
    const TArray<FAnimNotifyPayloadSchema>& Schemas = GetAnimNotifyPayloadSchemas();
    const auto Iterator = std::find_if(
        Schemas.begin(),
        Schemas.end(),
        [&NotifyClassName](const FAnimNotifyPayloadSchema& Schema)
        {
            return Schema.NotifyClassName == NotifyClassName;
        });
    return Iterator != Schemas.end() ? &(*Iterator) : nullptr;
}

bool HasAnimNotifyPayloadSchema(const FString& NotifyClassName)
{
    return FindAnimNotifyPayloadSchema(NotifyClassName) != nullptr;
}

const char* GetAnimNotifyPayloadExample(const FString& NotifyClassName)
{
    const FAnimNotifyPayloadSchema* Schema = FindAnimNotifyPayloadSchema(NotifyClassName);
    return (Schema && !Schema->PayloadExample.empty()) ? Schema->PayloadExample.c_str() : nullptr;
}

TArray<FString> GetAnimNotifyPayloadFieldLookupKeys(const FAnimNotifyPayloadFieldDefinition& Field)
{
    if (Field.SemanticId != EAnimNotifySemanticFieldId::None)
    {
        const TArray<FString>& SemanticLookupKeys = AnimNotifySemanticFieldNames::GetLookupKeys(Field.SemanticId);
        return TArray<FString>(SemanticLookupKeys.begin(), SemanticLookupKeys.end());
    }

    TArray<FString> Keys;
    if (!Field.Key.empty())
    {
        Keys.push_back(Field.Key);
    }

    for (const FString& LegacyKey : Field.LegacyKeys)
    {
        if (!LegacyKey.empty())
        {
            Keys.push_back(LegacyKey);
        }
    }

    return Keys;
}

const FAnimNotifyPayloadFieldDefinition* FindAnimNotifyPayloadFieldBySemanticId(
    const FAnimNotifyPayloadSchema& Schema,
    EAnimNotifySemanticFieldId SemanticId)
{
    const auto Iterator = std::find_if(
        Schema.Fields.begin(),
        Schema.Fields.end(),
        [SemanticId](const FAnimNotifyPayloadFieldDefinition& Field)
        {
            return Field.SemanticId == SemanticId;
        });
    return Iterator != Schema.Fields.end() ? &(*Iterator) : nullptr;
}
