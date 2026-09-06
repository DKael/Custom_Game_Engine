#include "Editor/Animation/AnimationSequenceNotifyValidation.h"

#include "Animation/AnimNotify.h"
#include "Animation/AnimNotifyPayloadParser.h"
#include "Animation/AnimData/AnimSequence.h"
#include "Editor/Animation/AnimationSequenceNotifyPayloadSchema.h"
#include "Editor/Animation/AnimationSequencePreviewController.h"
#include "Editor/Animation/AnimationSequenceViewerUtils.h"
#include "Object/ObjectFactory.h"
#include "Reflection/Class.h"

#include <algorithm>
#include <cctype>
#include <string>

namespace
{
    struct FAnimNotifyValidationLocation
    {
        int32 TrackIndex = -1;
        int32 EventIndex = -1;
        FGuid StableId;
    };

    // Generic validation helpers
    bool HasNonWhitespaceValue(const FString& Value)
    {
        return std::any_of(
            Value.begin(),
            Value.end(),
            [](char Character)
            {
                return !std::isspace(static_cast<unsigned char>(Character));
            });
    }

    const UClass* FindRegisteredClass(const FString& TypeName)
    {
        if (TypeName.empty())
        {
            return nullptr;
        }

        TArray<const UClass*> RegisteredClasses;
        FObjectFactory::Get().GetRegisteredClasses(RegisteredClasses);
        for (const UClass* Class : RegisteredClasses)
        {
            if (Class && Class->GetName() && TypeName == Class->GetName())
            {
                return Class;
            }
        }

        return nullptr;
    }

    bool IsNotifyClassCompatible(const UClass* Class, EAnimNotifyEventType EventType)
    {
        if (!Class)
        {
            return false;
        }

        if (EventType == EAnimNotifyEventType::NotifyState)
        {
            return Class->IsA(UAnimNotifyState::StaticClass());
        }

        return Class->IsA(UAnimNotify::StaticClass()) && !Class->IsA(UAnimNotifyState::StaticClass());
    }

    FString GetNotifyTypeLabel(EAnimNotifyEventType EventType)
    {
        return EventType == EAnimNotifyEventType::NotifyState ? "Notify State" : "Notify";
    }

    const FAnimNotifyPayloadFieldDefinition* FindSchemaField(
        const FAnimNotifyPayloadSchema& Schema,
        EAnimNotifySemanticFieldId SemanticId)
    {
        return FindAnimNotifyPayloadFieldBySemanticId(Schema, SemanticId);
    }

    TArray<FString> GetSchemaFieldLookupKeys(
        const FAnimNotifyPayloadSchema& Schema,
        EAnimNotifySemanticFieldId SemanticId)
    {
        const FAnimNotifyPayloadFieldDefinition* Field = FindSchemaField(Schema, SemanticId);
        return Field ? GetAnimNotifyPayloadFieldLookupKeys(*Field) : TArray<FString>();
    }

    bool TryGetSchemaFloatValue(
        const FAnimNotifyPayloadSchema& Schema,
        const FAnimNotifyPayloadParser& Payload,
        EAnimNotifySemanticFieldId SemanticId,
        float& OutValue)
    {
        const TArray<FString> LookupKeys = GetSchemaFieldLookupKeys(Schema, SemanticId);
        return !LookupKeys.empty() && Payload.HasAnyValue(LookupKeys) && Payload.TryGetFloatAny(LookupKeys, OutValue);
    }

    bool TryGetSchemaNameValue(
        const FAnimNotifyPayloadSchema& Schema,
        const FAnimNotifyPayloadParser& Payload,
        EAnimNotifySemanticFieldId SemanticId,
        FName& OutValue)
    {
        const TArray<FString> LookupKeys = GetSchemaFieldLookupKeys(Schema, SemanticId);
        if (LookupKeys.empty())
        {
            return false;
        }

        OutValue = Payload.GetNameAny(LookupKeys);
        return true;
    }

    bool TryGetSchemaStringValue(
        const FAnimNotifyPayloadSchema& Schema,
        const FAnimNotifyPayloadParser& Payload,
        EAnimNotifySemanticFieldId SemanticId,
        FString& OutValue)
    {
        const TArray<FString> LookupKeys = GetSchemaFieldLookupKeys(Schema, SemanticId);
        if (LookupKeys.empty())
        {
            return false;
        }

        OutValue = Payload.GetStringAny(LookupKeys);
        return true;
    }

    void AddNotifyClassIssues(
        FAnimNotifyValidationReport& Report,
        const FAnimNotifyEvent& NotifyEvent,
        const FString& NotifyClassName,
        const FAnimNotifyValidationLocation& Location)
    {
        const UClass* NotifyClass = FindRegisteredClass(NotifyClassName);
        if (!NotifyClass)
        {
            Report.AddIssue(
                EAnimNotifyValidationSeverity::Error,
                EAnimNotifyValidationField::NotifyClass,
                "Notify Class is not registered: " + NotifyClassName,
                "Choose a registered notify class from the dropdown.",
                Location.TrackIndex,
                Location.EventIndex,
                Location.StableId);
            return;
        }

        if (!IsNotifyClassCompatible(NotifyClass, NotifyEvent.EventType))
        {
            Report.AddIssue(
                EAnimNotifyValidationSeverity::Error,
                EAnimNotifyValidationField::NotifyClass,
                "Selected class does not match the " + GetNotifyTypeLabel(NotifyEvent.EventType) + " type.",
                "Use a class derived from " + GetDefaultAnimNotifyClassName(NotifyEvent.EventType) + ".",
                Location.TrackIndex,
                Location.EventIndex,
                Location.StableId);
        }
    }

    void AddNotifyTimingIssues(
        FAnimNotifyValidationReport& Report,
        const FAnimNotifyEvent& NotifyEvent,
        const FAnimNotifyValidationLocation& Location)
    {
        if (NotifyEvent.EventType == EAnimNotifyEventType::NotifyState && NotifyEvent.Duration <= 0.0f)
        {
            Report.AddIssue(
                EAnimNotifyValidationSeverity::Warning,
                EAnimNotifyValidationField::Duration,
                "Notify State duration is zero or negative. Begin/End will fire at the same time.",
                "Set a positive duration for state-style behavior.",
                Location.TrackIndex,
                Location.EventIndex,
                Location.StableId);
        }
        else if (NotifyEvent.EventType == EAnimNotifyEventType::Notify && NotifyEvent.Duration > 0.0f)
        {
            Report.AddIssue(
                EAnimNotifyValidationSeverity::Info,
                EAnimNotifyValidationField::Duration,
                "Duration is ignored for one-shot notifies.",
                "Use Notify State if you need a begin/end window.",
                Location.TrackIndex,
                Location.EventIndex,
                Location.StableId);
        }
    }

    void AddNotifyNameIssues(
        FAnimNotifyValidationReport& Report,
        const FAnimNotifyEvent& NotifyEvent,
        const FAnimNotifyValidationLocation& Location)
    {
        if (!NotifyEvent.Name.IsValid() || NotifyEvent.Name.ToString().empty())
        {
            Report.AddIssue(
                EAnimNotifyValidationSeverity::Info,
                EAnimNotifyValidationField::Name,
                "Notify name is empty.",
                "Set a name to make timeline markers and runtime logs easier to read.",
                Location.TrackIndex,
                Location.EventIndex,
                Location.StableId);
        }
    }

    void AddGenericEventIssues(
        FAnimNotifyValidationReport& Report,
        const FAnimNotifyEvent& NotifyEvent,
        const FString& NotifyClassName,
        const FAnimNotifyValidationLocation& Location)
    {
        AddNotifyClassIssues(Report, NotifyEvent, NotifyClassName, Location);
        AddNotifyTimingIssues(Report, NotifyEvent, Location);
        AddNotifyNameIssues(Report, NotifyEvent, Location);
    }

    void AddRequiredPayloadIssues(
        FAnimNotifyValidationReport& Report,
        const FAnimNotifyPayloadSchema& Schema,
        const FAnimNotifyPayloadParser& Payload,
        const FAnimNotifyValidationLocation& Location)
    {
        const char* PayloadExample = GetAnimNotifyPayloadExample(Schema.NotifyClassName);
        const FString PayloadExampleHint = PayloadExample ? FString("Payload Example: ") + PayloadExample : FString();
        for (const FAnimNotifyPayloadFieldDefinition& Field : Schema.Fields)
        {
            if (!Field.bRequired)
            {
                continue;
            }

            const TArray<FString> LookupKeys = GetAnimNotifyPayloadFieldLookupKeys(Field);
            if (!Payload.HasAnyValue(LookupKeys) || !HasNonWhitespaceValue(Payload.GetStringAny(LookupKeys)))
            {
                Report.AddIssue(
                    EAnimNotifyValidationSeverity::Error,
                    EAnimNotifyValidationField::Payload,
                    Field.Label + " is required for " + Schema.NotifyClassName + ".",
                    PayloadExampleHint,
                    Location.TrackIndex,
                    Location.EventIndex,
                    Location.StableId);
            }
        }
    }

    void AddMalformedPayloadIssues(
        FAnimNotifyValidationReport& Report,
        const FAnimNotifyPayloadSchema& Schema,
        const FAnimNotifyPayloadParser& Payload,
        const FAnimNotifyValidationLocation& Location)
    {
        const char* PayloadExample = GetAnimNotifyPayloadExample(Schema.NotifyClassName);
        const FString PayloadExampleHint = PayloadExample ? FString("Payload Example: ") + PayloadExample : FString();
        for (const FAnimNotifyPayloadFieldDefinition& Field : Schema.Fields)
        {
            const TArray<FString> LookupKeys = GetAnimNotifyPayloadFieldLookupKeys(Field);
            if (Field.Type == EAnimNotifyPayloadFieldType::Float)
            {
                float ParsedValue = 0.0f;
                if (Payload.HasAnyValue(LookupKeys) && !Payload.TryGetFloatAny(LookupKeys, ParsedValue))
                {
                    Report.AddIssue(
                        EAnimNotifyValidationSeverity::Warning,
                        EAnimNotifyValidationField::Payload,
                        Field.Label + " value is invalid. Expected a numeric value.",
                        PayloadExampleHint,
                        Location.TrackIndex,
                        Location.EventIndex,
                        Location.StableId);
                }
            }
            else if (Field.Type == EAnimNotifyPayloadFieldType::Int)
            {
                int32 ParsedValue = 0;
                if (Payload.HasAnyValue(LookupKeys) && !Payload.TryGetIntAny(LookupKeys, ParsedValue))
                {
                    Report.AddIssue(
                        EAnimNotifyValidationSeverity::Warning,
                        EAnimNotifyValidationField::Payload,
                        Field.Label + " value is invalid. Expected an integer value.",
                        PayloadExampleHint,
                        Location.TrackIndex,
                        Location.EventIndex,
                        Location.StableId);
                }
            }
            else if (Field.Type == EAnimNotifyPayloadFieldType::Bool)
            {
                bool ParsedValue = false;
                if (Payload.HasAnyValue(LookupKeys) && !Payload.TryGetBoolAny(LookupKeys, ParsedValue))
                {
                    Report.AddIssue(
                        EAnimNotifyValidationSeverity::Warning,
                        EAnimNotifyValidationField::Payload,
                        Field.Label + " value is invalid. Expected true/false.",
                        PayloadExampleHint,
                        Location.TrackIndex,
                        Location.EventIndex,
                        Location.StableId);
                }
            }
        }
    }

    void AddGenericPayloadIssues(
        FAnimNotifyValidationReport& Report,
        const FAnimNotifyPayloadSchema& Schema,
        const FAnimNotifyPayloadParser& Payload,
        const FAnimNotifyValidationLocation& Location)
    {
        AddRequiredPayloadIssues(Report, Schema, Payload, Location);
        AddMalformedPayloadIssues(Report, Schema, Payload, Location);
    }

    // Preview/context-sensitive validation helpers
    void AddPreviewAttachTargetIssues(
        FAnimNotifyValidationReport& Report,
        const FAnimNotifyPayloadSchema& Schema,
        const FAnimNotifyPayloadParser& Payload,
        const FAnimNotifyValidationContext& Context,
        const FAnimNotifyValidationLocation& Location)
    {
        // Preview-aware checks are intentionally warnings only. They help authors
        // catch mismatches against the current preview mesh, but they must not
        // reject payloads that are still valid against a different runtime mesh.
        if (!Context.PreviewController || !Context.PreviewController->HasValidPreview())
        {
            return;
        }

        FName AttachTargetName = FName::None;
        if (TryGetSchemaNameValue(Schema, Payload, EAnimNotifySemanticFieldId::SocketName, AttachTargetName)
            && AttachTargetName != FName::None
            && !Context.PreviewController->HasPreviewSocket(AttachTargetName))
        {
            Report.AddIssue(
                EAnimNotifyValidationSeverity::Warning,
                EAnimNotifyValidationField::Payload,
                "Attach target \"" + AttachTargetName.ToString() + "\" is not present on the current preview mesh.",
                "Verify the socket/bone name or pick a different preview mesh.",
                Location.TrackIndex,
                Location.EventIndex,
                Location.StableId);
        }
    }

    void AddAttackWindowPreviewComponentIssues(
        FAnimNotifyValidationReport& Report,
        const FAnimNotifyPayloadSchema& Schema,
        const FAnimNotifyPayloadParser& Payload,
        const FAnimNotifyValidationContext& Context,
        const FAnimNotifyValidationLocation& Location)
    {
        // AttackWindow is the one built-in whose component name is resolved
        // against actor primitive components rather than attach targets, so its
        // preview check stays separate from generic attach-target validation.
        if (!Context.PreviewController || !Context.PreviewController->HasValidPreview())
        {
            return;
        }

        if (Schema.NotifyClassName != "UAnimNotifyState_AttackWindow")
        {
            return;
        }

        FString ComponentName;
        if (TryGetSchemaStringValue(Schema, Payload, EAnimNotifySemanticFieldId::ComponentName, ComponentName)
            && !ComponentName.empty()
            && !Context.PreviewController->HasPreviewPrimitiveComponent(ComponentName))
        {
            Report.AddIssue(
                EAnimNotifyValidationSeverity::Warning,
                EAnimNotifyValidationField::Payload,
                "Component \"" + ComponentName + "\" was not found on the current preview actor.",
                "AttackWindow payloads target primitive components by name.",
                Location.TrackIndex,
                Location.EventIndex,
                Location.StableId);
        }
    }

    void AddPreviewContextIssues(
        FAnimNotifyValidationReport& Report,
        const FAnimNotifyPayloadSchema& Schema,
        const FAnimNotifyPayloadParser& Payload,
        const FAnimNotifyValidationContext& Context,
        const FAnimNotifyValidationLocation& Location)
    {
        AddPreviewAttachTargetIssues(Report, Schema, Payload, Context, Location);
        AddAttackWindowPreviewComponentIssues(Report, Schema, Payload, Context, Location);
    }

    // Built-in notify-specific validation helpers
    void AddCameraShakePayloadIssues(
        FAnimNotifyValidationReport& Report,
        const FAnimNotifyPayloadSchema& Schema,
        const FAnimNotifyPayloadParser& Payload,
        const FAnimNotifyValidationLocation& Location)
    {
        float Scale = 0.0f;
        if (TryGetSchemaFloatValue(Schema, Payload, EAnimNotifySemanticFieldId::Scale, Scale) && Scale <= 0.0f)
        {
            Report.AddIssue(
                EAnimNotifyValidationSeverity::Warning,
                EAnimNotifyValidationField::Payload,
                "Scale should be greater than zero for camera shake playback.",
                "Use a positive scale such as 1.0 to preserve the authored shake strength.",
                Location.TrackIndex,
                Location.EventIndex,
                Location.StableId);
        }
    }

    void AddPlaySfxPayloadIssues(
        FAnimNotifyValidationReport& Report,
        const FAnimNotifyPayloadSchema& Schema,
        const FAnimNotifyPayloadParser& Payload,
        const FAnimNotifyValidationLocation& Location)
    {
        // PlaySFX adds no built-in-specific warnings today. Keep this hook so
        // future audio-only checks still have an obvious home in this section.
        (void)Report;
        (void)Schema;
        (void)Payload;
        (void)Location;
    }

    void AddPlayVfxPayloadIssues(
        FAnimNotifyValidationReport& Report,
        const FAnimNotifyPayloadSchema& Schema,
        const FAnimNotifyPayloadParser& Payload,
        const FAnimNotifyValidationLocation& Location)
    {
        float Scale = 0.0f;
        if (TryGetSchemaFloatValue(Schema, Payload, EAnimNotifySemanticFieldId::Scale, Scale) && Scale <= 0.0f)
        {
            Report.AddIssue(
                EAnimNotifyValidationSeverity::Warning,
                EAnimNotifyValidationField::Payload,
                "Scale should be greater than zero for VFX playback.",
                "Use a positive scale such as 1.0 to preserve the authored effect size.",
                Location.TrackIndex,
                Location.EventIndex,
                Location.StableId);
        }
    }

    void AddFootstepSurfacePayloadIssues(
        FAnimNotifyValidationReport& Report,
        const FAnimNotifyPayloadSchema& Schema,
        const FAnimNotifyPayloadParser& Payload,
        const FAnimNotifyValidationLocation& Location)
    {
        float TraceDistance = 0.0f;
        if (TryGetSchemaFloatValue(Schema, Payload, EAnimNotifySemanticFieldId::TraceDistance, TraceDistance) && TraceDistance <= 0.0f)
        {
            Report.AddIssue(
                EAnimNotifyValidationSeverity::Warning,
                EAnimNotifyValidationField::Payload,
                "Trace Distance should be greater than zero for footstep surface resolution.",
                "Use a positive downward trace distance such as 25.0.",
                Location.TrackIndex,
                Location.EventIndex,
                Location.StableId);
        }
    }

    void AddSpawnDecalPayloadIssues(
        FAnimNotifyValidationReport& Report,
        const FAnimNotifyPayloadSchema& Schema,
        const FAnimNotifyPayloadParser& Payload,
        const FAnimNotifyValidationLocation& Location)
    {
        float TraceDistance = 0.0f;
        if (TryGetSchemaFloatValue(Schema, Payload, EAnimNotifySemanticFieldId::TraceDistance, TraceDistance) && TraceDistance <= 0.0f)
        {
            Report.AddIssue(
                EAnimNotifyValidationSeverity::Warning,
                EAnimNotifyValidationField::Payload,
                "Trace Distance should be greater than zero for decal surface resolution.",
                "Use a positive downward trace distance such as 50.0.",
                Location.TrackIndex,
                Location.EventIndex,
                Location.StableId);
        }

        float Size = 0.0f;
        if (TryGetSchemaFloatValue(Schema, Payload, EAnimNotifySemanticFieldId::Size, Size) && Size <= 0.0f)
        {
            Report.AddIssue(
                EAnimNotifyValidationSeverity::Warning,
                EAnimNotifyValidationField::Payload,
                "Size should be greater than zero for decal spawning.",
                "Use a positive decal size such as 1.0.",
                Location.TrackIndex,
                Location.EventIndex,
                Location.StableId);
        }

        float Lifetime = 0.0f;
        if (TryGetSchemaFloatValue(Schema, Payload, EAnimNotifySemanticFieldId::Lifetime, Lifetime) && Lifetime < 0.0f)
        {
            Report.AddIssue(
                EAnimNotifyValidationSeverity::Warning,
                EAnimNotifyValidationField::Payload,
                "Lifetime should not be negative for decal spawning.",
                "Use zero for the default lifetime or a positive duration such as 2.0.",
                Location.TrackIndex,
                Location.EventIndex,
                Location.StableId);
        }
    }

    void AddAttackWindowPayloadIssues(
        FAnimNotifyValidationReport& Report,
        const FAnimNotifyPayloadSchema& Schema,
        const FAnimNotifyPayloadParser& Payload,
        const FAnimNotifyValidationLocation& Location)
    {
        // AttackWindow-specific editor checks currently live in the preview-aware
        // component validation path. Keep this hook so the built-in-specific
        // section still makes ownership explicit without widening warning scope.
        (void)Report;
        (void)Schema;
        (void)Payload;
        (void)Location;
    }

    void AddGameplayEventPayloadIssues(
        FAnimNotifyValidationReport& Report,
        const FAnimNotifyPayloadSchema& Schema,
        const FAnimNotifyPayloadParser& Payload,
        const FAnimNotifyValidationLocation& Location)
    {
        // GameplayEvent adds no built-in-specific warnings today.
        (void)Report;
        (void)Schema;
        (void)Payload;
        (void)Location;
    }

    void AddGameplayEventWindowPayloadIssues(
        FAnimNotifyValidationReport& Report,
        const FAnimNotifyPayloadSchema& Schema,
        const FAnimNotifyPayloadParser& Payload,
        const FAnimNotifyValidationLocation& Location)
    {
        // GameplayEventWindow adds no built-in-specific warnings today.
        (void)Report;
        (void)Schema;
        (void)Payload;
        (void)Location;
    }

    void AddNotifySpecificPayloadIssues(
        FAnimNotifyValidationReport& Report,
        const FAnimNotifyPayloadSchema& Schema,
        const FAnimNotifyPayloadParser& Payload,
        const FAnimNotifyValidationLocation& Location)
    {
        if (Schema.NotifyClassName == "UAnimNotify_PlaySFX")
        {
            AddPlaySfxPayloadIssues(Report, Schema, Payload, Location);
        }
        else if (Schema.NotifyClassName == "UAnimNotify_CameraShake")
        {
            AddCameraShakePayloadIssues(Report, Schema, Payload, Location);
        }
        else if (Schema.NotifyClassName == "UAnimNotify_PlayVFX")
        {
            AddPlayVfxPayloadIssues(Report, Schema, Payload, Location);
        }
        else if (Schema.NotifyClassName == "UAnimNotify_GameplayEvent")
        {
            AddGameplayEventPayloadIssues(Report, Schema, Payload, Location);
        }
        else if (Schema.NotifyClassName == "UAnimNotifyState_GameplayEventWindow")
        {
            AddGameplayEventWindowPayloadIssues(Report, Schema, Payload, Location);
        }
        if (Schema.NotifyClassName == "UAnimNotifyState_AttackWindow")
        {
            AddAttackWindowPayloadIssues(Report, Schema, Payload, Location);
        }
        else if (Schema.NotifyClassName == "UAnimNotify_FootstepSurfaceEvent")
        {
            AddFootstepSurfacePayloadIssues(Report, Schema, Payload, Location);
        }
        else if (Schema.NotifyClassName == "UAnimNotify_SpawnDecal")
        {
            AddSpawnDecalPayloadIssues(Report, Schema, Payload, Location);
        }
    }
}

void FAnimNotifyValidationReport::AddIssue(const FAnimNotifyValidationIssue& Issue)
{
    Issues.push_back(Issue);

    switch (Issue.Severity)
    {
    case EAnimNotifyValidationSeverity::Error:
        ++ErrorCount;
        break;
    case EAnimNotifyValidationSeverity::Warning:
        ++WarningCount;
        break;
    case EAnimNotifyValidationSeverity::Info:
    default:
        ++InfoCount;
        break;
    }
}

void FAnimNotifyValidationReport::AddIssue(
    EAnimNotifyValidationSeverity Severity,
    EAnimNotifyValidationField Field,
    const FString& Message,
    const FString& Hint,
    int32 TrackIndex,
    int32 EventIndex,
    const FGuid& StableId)
{
    FAnimNotifyValidationIssue Issue;
    Issue.Severity = Severity;
    Issue.Field = Field;
    Issue.Message = Message;
    Issue.Hint = Hint;
    Issue.TrackIndex = TrackIndex;
    Issue.EventIndex = EventIndex;
    Issue.StableId = StableId;
    AddIssue(Issue);
}

void FAnimNotifyValidationReport::Append(const FAnimNotifyValidationReport& Other)
{
    for (const FAnimNotifyValidationIssue& Issue : Other.Issues)
    {
        AddIssue(Issue);
    }
}

int32 FAnimNotifyValidationReport::GetCount(EAnimNotifyValidationSeverity Severity) const
{
    switch (Severity)
    {
    case EAnimNotifyValidationSeverity::Error:
        return ErrorCount;
    case EAnimNotifyValidationSeverity::Warning:
        return WarningCount;
    case EAnimNotifyValidationSeverity::Info:
    default:
        return InfoCount;
    }
}

bool FAnimNotifyValidationReport::HasAnyIssues() const
{
    return !Issues.empty();
}

bool FAnimNotifyValidationReport::HasIssuesForField(EAnimNotifyValidationField Field) const
{
    return std::any_of(
        Issues.begin(),
        Issues.end(),
        [Field](const FAnimNotifyValidationIssue& Issue)
        {
            return Issue.Field == Field;
        });
}

bool FAnimNotifyValidationReport::HasIssuesOfSeverity(EAnimNotifyValidationSeverity Severity) const
{
    return std::any_of(
        Issues.begin(),
        Issues.end(),
        [Severity](const FAnimNotifyValidationIssue& Issue)
        {
            return Issue.Severity == Severity;
        });
}

FAnimNotifyValidationReport ValidateAnimNotifyEvent(
    const FAnimNotifyEvent& NotifyEvent,
    int32 TrackIndex,
    int32 EventIndex,
    const FAnimNotifyValidationContext& Context)
{
    FAnimNotifyValidationReport Report;
    const FAnimNotifyValidationLocation Location = { TrackIndex, EventIndex, NotifyEvent.StableId };
    const FString NotifyClassName = NotifyEvent.GetResolvedNotifyClassName();
    AddGenericEventIssues(Report, NotifyEvent, NotifyClassName, Location);

    if (const FAnimNotifyPayloadSchema* Schema = FindAnimNotifyPayloadSchema(NotifyClassName))
    {
        const FAnimNotifyPayloadParser Payload(NotifyEvent.Payload);
        AddGenericPayloadIssues(Report, *Schema, Payload, Location);
        AddPreviewContextIssues(Report, *Schema, Payload, Context, Location);
        AddNotifySpecificPayloadIssues(Report, *Schema, Payload, Location);
    }

    return Report;
}

FAnimNotifyValidationReport ValidateAnimNotifyDocument(
    const UAnimSequence* Sequence,
    const FAnimNotifyValidationContext& Context)
{
    FAnimNotifyValidationReport Report;
    const TArray<FAnimNotifyTrack>* Tracks = AnimationSequenceViewer::GetSequenceNotifyTracks(Sequence);
    if (!Tracks)
    {
        return Report;
    }

    TMap<FString, int32> StableIdCounts;
    for (const FAnimNotifyTrack& Track : *Tracks)
    {
        for (const FAnimNotifyEvent& NotifyEvent : Track.Events)
        {
            if (NotifyEvent.StableId.IsValid())
            {
                StableIdCounts[NotifyEvent.StableId.ToString()]++;
            }
        }
    }

    for (int32 TrackIndex = 0; TrackIndex < static_cast<int32>(Tracks->size()); ++TrackIndex)
    {
        const FAnimNotifyTrack& Track = (*Tracks)[TrackIndex];
        for (int32 EventIndex = 0; EventIndex < static_cast<int32>(Track.Events.size()); ++EventIndex)
        {
            const FAnimNotifyEvent& NotifyEvent = Track.Events[EventIndex];
            Report.Append(ValidateAnimNotifyEvent(NotifyEvent, TrackIndex, EventIndex, Context));

            if (NotifyEvent.StableId.IsValid())
            {
                const auto CountIterator = StableIdCounts.find(NotifyEvent.StableId.ToString());
                if (CountIterator != StableIdCounts.end() && CountIterator->second > 1)
                {
                    Report.AddIssue(
                        EAnimNotifyValidationSeverity::Error,
                        EAnimNotifyValidationField::General,
                        "Duplicate StableId detected: " + NotifyEvent.StableId.ToString(),
                        "Each notify event should keep a unique StableId for reliable selection and save behavior.",
                        TrackIndex,
                        EventIndex,
                        NotifyEvent.StableId);
                }
            }
        }
    }

    return Report;
}

FString FormatAnimNotifyValidationSummary(const FAnimNotifyValidationReport& Report)
{
    if (Report.ErrorCount <= 0 && Report.WarningCount <= 0 && Report.InfoCount <= 0)
    {
        return "No notify validation issues.";
    }

    FString Summary;
    if (Report.ErrorCount > 0)
    {
        Summary += std::to_string(Report.ErrorCount) + " error";
        if (Report.ErrorCount != 1)
        {
            Summary += "s";
        }
    }

    if (Report.WarningCount > 0)
    {
        if (!Summary.empty())
        {
            Summary += ", ";
        }
        Summary += std::to_string(Report.WarningCount) + " warning";
        if (Report.WarningCount != 1)
        {
            Summary += "s";
        }
    }

    if (Report.InfoCount > 0)
    {
        if (!Summary.empty())
        {
            Summary += ", ";
        }
        Summary += std::to_string(Report.InfoCount) + " info";
        if (Report.InfoCount != 1)
        {
            Summary += "s";
        }
    }

    return Summary;
}
