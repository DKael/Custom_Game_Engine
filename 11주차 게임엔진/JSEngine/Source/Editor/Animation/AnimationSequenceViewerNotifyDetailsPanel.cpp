#include "Editor/Animation/AnimationSequenceEditorWidget.h"

#include "Animation/AnimNotify.h"
#include "Animation/AnimNotifyPayloadParser.h"
#include "Editor/Animation/AnimationSequenceEditorDocument.h"
#include "Editor/Animation/AnimationSequenceEditorState.h"
#include "Editor/Animation/AnimationSequenceNotifyAssetPickerHelpers.h"
#include "Editor/Animation/AnimationSequenceNotifyPayloadSchema.h"
#include "Editor/Animation/AnimationSequenceNotifyValidation.h"
#include "Editor/Animation/AnimationSequencePreviewController.h"
#include "Editor/Animation/AnimationSequenceViewerUiHelpers.h"
#include "Object/ObjectFactory.h"
#include "Reflection/Class.h"

#include "ImGui/imgui.h"

#include <algorithm>
#include <cctype>
#include <cstring>

namespace
{
    constexpr float NotifyPanelSectionSpacing = 10.0f;
    constexpr float NotifyPanelSectionIndent = 6.0f;
    constexpr float NotifyPanelFieldMinWidth = 220.0f;
    constexpr float NotifyPanelFieldMaxWidth = 440.0f;
    constexpr float NotifyAssetPickerPopupHeight = 280.0f;

    FString ToLowerAscii(FString Value)
    {
        std::transform(
            Value.begin(),
            Value.end(),
            Value.begin(),
            [](unsigned char Ch)
            {
                return static_cast<char>(std::tolower(Ch));
            });
        return Value;
    }

    bool MatchesFilter(const FString& Candidate, const FString& Filter)
    {
        if (Filter.empty())
        {
            return true;
        }

        return ToLowerAscii(Candidate).find(ToLowerAscii(Filter)) != FString::npos;
    }

    void SetNotifyPanelFieldWidth()
    {
        AnimationSequenceViewerUi::SetClampedWidthItem(
            0.72f,
            NotifyPanelFieldMinWidth,
            NotifyPanelFieldMaxWidth);
    }

    TArray<FString> CollectNotifyClassOptions(EAnimNotifyEventType EventType)
    {
        TArray<const UClass*> RegisteredClasses;
        FObjectFactory::Get().GetRegisteredClasses(RegisteredClasses);

        const UClass* RequiredBaseClass =
            EventType == EAnimNotifyEventType::NotifyState ? UAnimNotifyState::StaticClass() : UAnimNotify::StaticClass();
        const UClass* ExcludedBaseClass =
            EventType == EAnimNotifyEventType::NotifyState ? nullptr : UAnimNotifyState::StaticClass();

        TArray<FString> Options;
        for (const UClass* Class : RegisteredClasses)
        {
            if (!Class || !Class->GetName() || !Class->IsA(RequiredBaseClass))
            {
                continue;
            }

            if (ExcludedBaseClass && Class->IsA(ExcludedBaseClass))
            {
                continue;
            }

            Options.push_back(Class->GetName());
        }

        std::sort(Options.begin(), Options.end());
        Options.erase(std::unique(Options.begin(), Options.end()), Options.end());
        return Options;
    }

    bool ContainsNotifyClassOption(const TArray<FString>& Options, const FString& Value)
    {
        return std::find(Options.begin(), Options.end(), Value) != Options.end();
    }

    const char* GetNotifyTypeLabel(EAnimNotifyEventType EventType)
    {
        return EventType == EAnimNotifyEventType::NotifyState ? "UAnimNotifyState" : "UAnimNotify";
    }

    ImVec4 GetValidationSeverityColor(EAnimNotifyValidationSeverity Severity)
    {
        switch (Severity)
        {
        case EAnimNotifyValidationSeverity::Error:
            return ImVec4(0.95f, 0.35f, 0.35f, 1.0f);
        case EAnimNotifyValidationSeverity::Warning:
            return ImVec4(0.98f, 0.76f, 0.32f, 1.0f);
        case EAnimNotifyValidationSeverity::Info:
        default:
            return ImVec4(0.58f, 0.72f, 0.96f, 1.0f);
        }
    }

    const char* GetValidationSeverityLabel(EAnimNotifyValidationSeverity Severity)
    {
        switch (Severity)
        {
        case EAnimNotifyValidationSeverity::Error:
            return "Error";
        case EAnimNotifyValidationSeverity::Warning:
            return "Warning";
        case EAnimNotifyValidationSeverity::Info:
        default:
            return "Info";
        }
    }

    template <size_t BufferSize>
    void CopyStringToBuffer(const FString& Source, std::array<char, BufferSize>& Buffer)
    {
        Buffer.fill('\0');
        strncpy_s(Buffer.data(), Buffer.size(), Source.c_str(), _TRUNCATE);
    }

    float GetSchemaFloatValue(const FAnimNotifyPayloadParser& Payload, const FAnimNotifyPayloadFieldDefinition& Field)
    {
        const FAnimNotifyPayloadParser DefaultPayload(Field.DefaultValue.empty() ? FString() : Field.Key + "=" + Field.DefaultValue);
        return Payload.GetFloatAny(
            GetAnimNotifyPayloadFieldLookupKeys(Field),
            DefaultPayload.GetFloat(Field.Key, 0.0f));
    }

    int32 GetSchemaIntValue(const FAnimNotifyPayloadParser& Payload, const FAnimNotifyPayloadFieldDefinition& Field)
    {
        const FAnimNotifyPayloadParser DefaultPayload(Field.DefaultValue.empty() ? FString() : Field.Key + "=" + Field.DefaultValue);
        return Payload.GetIntAny(
            GetAnimNotifyPayloadFieldLookupKeys(Field),
            DefaultPayload.GetInt(Field.Key, 0));
    }

    bool GetSchemaBoolValue(const FAnimNotifyPayloadParser& Payload, const FAnimNotifyPayloadFieldDefinition& Field)
    {
        const FAnimNotifyPayloadParser DefaultPayload(Field.DefaultValue.empty() ? FString() : Field.Key + "=" + Field.DefaultValue);
        return Payload.GetBoolAny(
            GetAnimNotifyPayloadFieldLookupKeys(Field),
            DefaultPayload.GetBool(Field.Key, false));
    }

    const char* GetPickerUnavailableMessage(EAnimNotifyPayloadFieldEditorHint EditorHint)
    {
        switch (EditorHint)
        {
        case EAnimNotifyPayloadFieldEditorHint::SocketPicker:
            return "No preview sockets available. Enter a manual value.";
        case EAnimNotifyPayloadFieldEditorHint::ComponentPicker:
            return "No preview primitive components available. Enter a manual value.";
        case EAnimNotifyPayloadFieldEditorHint::AssetPicker:
            return "No picker entries are available. Use Advanced Raw Payload for a manual override.";
        case EAnimNotifyPayloadFieldEditorHint::Default:
        default:
            return nullptr;
        }
    }

    struct FSelectedNotifyEditorContext
    {
        bool bValid = false;
        FAnimNotifyEvent NotifySnapshot;
        FString NotifyClassName;
        FAnimNotifyValidationReport ValidationReport;
    };

    FSelectedNotifyEditorContext BuildSelectedNotifyEditorContext(const FAnimationSequenceEditorDocument* Document)
    {
        FSelectedNotifyEditorContext Context;
        if (!Document || !Document->TryGetSelectedNotifySnapshot(Context.NotifySnapshot))
        {
            return Context;
        }

        Context.bValid = true;
        Context.NotifyClassName = Document->GetSelectedNotifyResolvedClassName();
        Context.ValidationReport = Document->BuildSelectedNotifyValidationReport();
        return Context;
    }
}

void FAnimationSequenceEditorWidget::RenderSelectedNotifyEditorPanel()
{
    const FSelectedNotifyEditorContext Context = BuildSelectedNotifyEditorContext(Document);
    const bool bHasSelection = Context.bValid;
    const int32 DefaultTrackIndex = EditorState ? std::max(EditorState->SelectedNotifyTrackIndex, 0) : 0;

    RenderSelectedNotifyToolbar(bHasSelection, DefaultTrackIndex);

    if (!bHasSelection)
    {
        AnimationSequenceViewerUi::DrawCompactEmptyState(
            "Select a notify to inspect details.",
            "The lower panel will show notify timing, payload, and debug information here.");
        return;
    }

    const FAnimNotifyEvent& NotifySnapshot = Context.NotifySnapshot;
    const FAnimNotifyValidationReport& SelectedValidationReport = Context.ValidationReport;
    // The details panel edits shared text buffers, not the snapshot directly.
    // Refresh those buffers from the stable selected notify before rendering any
    // payload/name fields so loaded .sequence data shows up immediately.
    SyncNotifyDetailsBuffers(NotifySnapshot);

    RenderSelectedNotifyBasicSection(NotifySnapshot, SelectedValidationReport);
    RenderSelectedNotifyTimingSection(NotifySnapshot, SelectedValidationReport);
    RenderSelectedNotifyPayloadSection(Context.NotifyClassName, SelectedValidationReport);
    RenderSelectedNotifyVisualSection(NotifySnapshot);
    RenderSelectedNotifyInlineValidationSection(SelectedValidationReport);
    RenderSelectedNotifyMetaFooter(NotifySnapshot);
}

void FAnimationSequenceEditorWidget::RenderSelectedNotifyToolbar(bool bHasSelection, int32 DefaultTrackIndex)
{
    const bool bCanAddNotify = Document != nullptr;
    if (!bCanAddNotify) { ImGui::BeginDisabled(); }
    if (ImGui::Button("Add Notify") && bCanAddNotify)
    {
        Document->AddNotifyAtTime(
            DefaultTrackIndex,
            EditorState ? EditorState->CurrentTime : 0.0f);
        NotifyDetailsBoundStableId.clear();
    }
    if (!bCanAddNotify) { ImGui::EndDisabled(); }

    ImGui::SameLine();

    if (!bHasSelection) { ImGui::BeginDisabled(); }
    if (ImGui::Button("Delete Selected") && bHasSelection)
    {
        Document->DeleteSelectedNotify();
        NotifyDetailsBoundStableId.clear();
    }
    if (!bHasSelection) { ImGui::EndDisabled(); }
}

void FAnimationSequenceEditorWidget::RenderSelectedNotifyBasicSection(
    const FAnimNotifyEvent& NotifySnapshot,
    const FAnimNotifyValidationReport& SelectedValidationReport)
{
    AnimationSequenceViewerUi::DrawSectionHeader(
        "Basic",
        NotifyPanelSectionSpacing * 0.35f,
        NotifyPanelSectionSpacing * 0.25f);

    SetNotifyPanelFieldWidth();
    if (ImGui::InputText("Name", NotifyNameEditBuffer.data(), NotifyNameEditBuffer.size()))
    {
        Document->SetSelectedNotifyName(FName(NotifyNameEditBuffer.data()));
    }
    RenderNotifyValidationIssues(SelectedValidationReport, EAnimNotifyValidationField::Name);

    const EAnimNotifyEventType CurrentNotifyType = NotifySnapshot.EventType;
    const char* NotifyTypePreviewLabel = GetNotifyTypeLabel(CurrentNotifyType);
    SetNotifyPanelFieldWidth();
    if (ImGui::BeginCombo("Notify Type", NotifyTypePreviewLabel))
    {
        const EAnimNotifyEventType NotifyTypeOptions[] =
        {
            EAnimNotifyEventType::Notify,
            EAnimNotifyEventType::NotifyState
        };

        for (const EAnimNotifyEventType NotifyTypeOption : NotifyTypeOptions)
        {
            const bool bSelected = NotifyTypeOption == CurrentNotifyType;
            if (ImGui::Selectable(GetNotifyTypeLabel(NotifyTypeOption), bSelected))
            {
                Document->SetSelectedNotifyType(NotifyTypeOption);
                NotifyDetailsBoundStableId.clear();
            }

            if (bSelected)
            {
                ImGui::SetItemDefaultFocus();
            }
        }

        ImGui::EndCombo();
    }

    const TArray<FString> NotifyClassOptions = CollectNotifyClassOptions(NotifySnapshot.EventType);
    const FString CurrentNotifyClassName = Document ? Document->GetSelectedNotifyResolvedClassName() : FString();
    const bool bHasRegisteredNotifyClass = ContainsNotifyClassOption(NotifyClassOptions, CurrentNotifyClassName);
    const FString NotifyClassPreviewLabel =
        CurrentNotifyClassName.empty()
            ? FString("<None>")
            : (bHasRegisteredNotifyClass ? CurrentNotifyClassName : CurrentNotifyClassName + " (missing)");

    SetNotifyPanelFieldWidth();
    if (ImGui::BeginCombo("Notify Class", NotifyClassPreviewLabel.c_str()))
    {
        for (const FString& NotifyClassOption : NotifyClassOptions)
        {
            const bool bSelected = NotifyClassOption == CurrentNotifyClassName;
            if (ImGui::Selectable(NotifyClassOption.c_str(), bSelected))
            {
                Document->SetSelectedNotifyClassName(NotifyClassOption);
                NotifyDetailsBoundStableId.clear();
            }
            if (bSelected)
            {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    if (!bHasRegisteredNotifyClass && !CurrentNotifyClassName.empty())
    {
        ImGui::TextDisabled("Registered class not found: %s", CurrentNotifyClassName.c_str());
    }
    RenderNotifyValidationIssues(SelectedValidationReport, EAnimNotifyValidationField::NotifyClass);

    if (!NotifyClassOptions.empty())
    {
        ImGui::Indent(NotifyPanelSectionIndent);
        ImGui::TextDisabled("Test classes: %s / %s", "UAnimNotifyLog", "UAnimNotifyStateLog");
        ImGui::Unindent(NotifyPanelSectionIndent);
    }
}

void FAnimationSequenceEditorWidget::RenderSelectedNotifyTimingSection(
    const FAnimNotifyEvent& NotifySnapshot,
    const FAnimNotifyValidationReport& SelectedValidationReport)
{
    AnimationSequenceViewerUi::DrawSectionHeader(
        "Timing",
        NotifyPanelSectionSpacing * 0.35f,
        NotifyPanelSectionSpacing * 0.25f);

    ImGui::Indent(NotifyPanelSectionIndent);
    ImGui::LabelText("Track", "%d", EditorState->SelectedNotifyTrackIndex + 1);
    ImGui::Unindent(NotifyPanelSectionIndent);

    float NotifyTime = NotifySnapshot.Time;
    const float MaxTime = std::max(EditorState->SequenceLength, 0.0f);
    SetNotifyPanelFieldWidth();
    if (ImGui::DragFloat("Time", &NotifyTime, 0.001f, 0.0f, MaxTime, "%.3f s"))
    {
        Document->SetSelectedNotifyTime(NotifyTime, EditorState->bSnapToFrames);
    }

    float NotifyDuration = NotifySnapshot.Duration;
    SetNotifyPanelFieldWidth();
    if (ImGui::DragFloat("Duration", &NotifyDuration, 0.001f, 0.0f, MaxTime, "%.3f s"))
    {
        Document->SetSelectedNotifyDuration(NotifyDuration);
    }
    RenderNotifyValidationIssues(SelectedValidationReport, EAnimNotifyValidationField::Duration);
}

void FAnimationSequenceEditorWidget::RenderSelectedNotifyPayloadSection(
    const FString& CurrentNotifyClassName,
    const FAnimNotifyValidationReport& SelectedValidationReport)
{
    AnimationSequenceViewerUi::DrawSectionHeader(
        "Payload",
        NotifyPanelSectionSpacing * 0.35f,
        NotifyPanelSectionSpacing * 0.25f);

    if (HasAnimNotifyPayloadSchema(CurrentNotifyClassName))
    {
        RenderStructuredNotifyPayloadEditor(CurrentNotifyClassName, SelectedValidationReport);
    }
    else
    {
        RenderRawNotifyPayloadEditor(CurrentNotifyClassName, SelectedValidationReport, true);
    }
}

void FAnimationSequenceEditorWidget::RenderSelectedNotifyVisualSection(const FAnimNotifyEvent& NotifySnapshot)
{
    AnimationSequenceViewerUi::DrawSectionHeader(
        "Visual",
        NotifyPanelSectionSpacing * 0.35f,
        NotifyPanelSectionSpacing * 0.25f);

    float Color[4] =
    {
        NotifySnapshot.Color.R,
        NotifySnapshot.Color.G,
        NotifySnapshot.Color.B,
        NotifySnapshot.Color.A
    };
    SetNotifyPanelFieldWidth();
    if (ImGui::ColorEdit4("Color", Color))
    {
        Document->SetSelectedNotifyColor(FColor(
            std::clamp(Color[0], 0.0f, 1.0f),
            std::clamp(Color[1], 0.0f, 1.0f),
            std::clamp(Color[2], 0.0f, 1.0f),
            std::clamp(Color[3], 0.0f, 1.0f)));
    }
}

void FAnimationSequenceEditorWidget::RenderSelectedNotifyInlineValidationSection(
    const FAnimNotifyValidationReport& SelectedValidationReport) const
{
    AnimationSequenceViewerUi::DrawSectionHeader(
        "Validation",
        NotifyPanelSectionSpacing * 0.35f,
        NotifyPanelSectionSpacing * 0.25f);
    RenderNotifyValidationSummary(SelectedValidationReport);
    ImGui::Dummy(ImVec2(0.0f, NotifyPanelSectionSpacing * 0.25f));
    ImGui::Checkbox("Show Validation Details", &EditorState->bShowNotifyValidationDetails);
    ImGui::Dummy(ImVec2(0.0f, NotifyPanelSectionSpacing * 0.25f));
    RenderNotifyValidationIssues(SelectedValidationReport, EAnimNotifyValidationField::General);
}

void FAnimationSequenceEditorWidget::RenderSelectedNotifyMetaFooter(const FAnimNotifyEvent& NotifySnapshot) const
{
    ImGui::Dummy(ImVec2(0.0f, NotifyPanelSectionSpacing * 0.6f));
    ImGui::TextDisabled(
        "Track %d  StableId %s",
        EditorState->SelectedNotifyTrackIndex + 1,
        NotifySnapshot.StableId.ToString().c_str());
}

void FAnimationSequenceEditorWidget::RenderNotifyValidationSummary(const FAnimNotifyValidationReport& Report) const
{
    const ImVec4 SummaryColor = GetValidationSeverityColor(
        Report.ErrorCount > 0
            ? EAnimNotifyValidationSeverity::Error
            : (Report.WarningCount > 0
                ? EAnimNotifyValidationSeverity::Warning
                : EAnimNotifyValidationSeverity::Info));

    ImGui::TextColored(
        SummaryColor,
        "Validation: %s",
        FormatAnimNotifyValidationSummary(Report).c_str());
}

void FAnimationSequenceEditorWidget::RenderNotifyValidationIssues(
    const FAnimNotifyValidationReport& Report,
    EAnimNotifyValidationField Field) const
{
    if (!(EditorState && EditorState->bShowNotifyValidationDetails))
    {
        return;
    }

    for (const FAnimNotifyValidationIssue& Issue : Report.Issues)
    {
        if (Issue.Field != Field)
        {
            continue;
        }

        const ImVec4 SeverityColor = GetValidationSeverityColor(Issue.Severity);
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextColored(
            SeverityColor,
            "%s: %s",
            GetValidationSeverityLabel(Issue.Severity),
            Issue.Message.c_str());
        if (!Issue.Hint.empty())
        {
            ImGui::TextColored(
                ImVec4(SeverityColor.x, SeverityColor.y, SeverityColor.z, 0.82f),
                "Hint: %s",
                Issue.Hint.c_str());
        }
        ImGui::PopTextWrapPos();
    }
}

void FAnimationSequenceEditorWidget::RenderStructuredNotifyPayloadEditor(
    const FString& NotifyClassName,
    const FAnimNotifyValidationReport& Report)
{
    if (!Document)
    {
        return;
    }

    const FAnimNotifyPayloadSchema* Schema = FindAnimNotifyPayloadSchema(NotifyClassName);
    if (!Schema)
    {
        RenderRawNotifyPayloadEditor(NotifyClassName, Report, true);
        return;
    }

    const FAnimNotifyEvent* CurrentNotify = Document->GetSelectedNotify();
    const FAnimNotifyPayloadParser Payload = Document->GetSelectedNotifyPayloadParser();

    auto RefreshPayloadBuffers = [this]()
    {
        // Structured field editors still serialize back into the canonical raw
        // payload string. After any mutation, refresh both the raw payload text
        // buffer and the per-field edit buffers from the document's current value.
        if (const FAnimNotifyEvent* UpdatedNotify = Document ? Document->GetSelectedNotify() : nullptr)
        {
            CopyStringToBuffer(UpdatedNotify->Payload, NotifyPayloadEditBuffer);
            const FAnimNotifyPayloadParser UpdatedPayload(UpdatedNotify->Payload);
            RefreshNotifyPayloadFieldBuffers(UpdatedPayload);
        }
    };

    auto ApplyTextFieldValue = [this](const FAnimNotifyPayloadFieldDefinition& Field, const FString& Value)
    {
        return Field.Type == EAnimNotifyPayloadFieldType::Name
            ? (Value.empty()
                ? Document->ClearSelectedNotifyPayloadValue(Field.Key)
                : Document->SetSelectedNotifyPayloadNameValue(Field.Key, FName(Value)))
            : (Value.empty()
                ? Document->ClearSelectedNotifyPayloadValue(Field.Key)
                : Document->SetSelectedNotifyPayloadStringValue(Field.Key, Value));
    };

    // Asset picker rendering owns editor-side picker UI only. Stored value,
    // display label, and fallback-label semantics stay delegated to the asset
    // picker helper layer so this panel remains an orchestration shell.
    auto RenderAssetPickerField = [this, &RefreshPayloadBuffers, &ApplyTextFieldValue](
        const FAnimNotifyPayloadFieldDefinition& Field,
        std::array<char, 256>& TargetBuffer)
    {
        // Picker-first UX is editor sugar only. The stored payload value
        // remains the same canonical string so serialization and runtime
        // notify behavior stay unchanged.
        const TArray<FNotifyAssetPickerEntry> PickerEntries =
            BuildNotifyAssetPickerEntries(Field.AssetKind);
        const FString CurrentValue = TargetBuffer.data();
        const FString CurrentDisplay = ResolveNotifyAssetPickerDisplayName(
            Field.AssetKind,
            CurrentValue,
            PickerEntries);
        const FString PopupId = "##NotifyAssetPicker_" + Field.Key;

        ImGui::Text("%s", Field.Label.c_str());
        ImGui::Indent(NotifyPanelSectionIndent);
        ImGui::TextWrapped(
            "%s",
            CurrentDisplay.empty() ? "(None)" : CurrentDisplay.c_str());
        if (!CurrentValue.empty())
        {
            ImGui::TextDisabled("%s", CurrentValue.c_str());
        }

        const FString PickButtonId = "Pick##" + Field.Key;
        const FString ClearButtonId = "Clear##" + Field.Key;
        const bool bOpenPopup = ImGui::Button(PickButtonId.c_str());
        ImGui::SameLine();
        const bool bClearRequested = ImGui::Button(ClearButtonId.c_str());
        if (bClearRequested && ApplyTextFieldValue(Field, FString()))
        {
            RefreshPayloadBuffers();
        }

        if (bOpenPopup)
        {
            if (ActiveNotifyAssetPickerPopupId != PopupId)
            {
                NotifyAssetPickerSearchBuffer.fill('\0');
                ActiveNotifyAssetPickerPopupId = PopupId;
            }
            ImGui::OpenPopup(PopupId.c_str());
        }

        if (ImGui::BeginPopup(PopupId.c_str()))
        {
            if (bOpenPopup)
            {
                ImGui::SetKeyboardFocusHere();
            }

            ImGui::InputText(
                "Search",
                NotifyAssetPickerSearchBuffer.data(),
                NotifyAssetPickerSearchBuffer.size());
            ImGui::Separator();

            const FString SearchFilter = NotifyAssetPickerSearchBuffer.data();
            ImGui::BeginChild(
                ("##NotifyAssetPickerList_" + Field.Key).c_str(),
                ImVec2(0.0f, NotifyAssetPickerPopupHeight),
                true);

            bool bHasVisibleEntries = false;
            if (ImGui::Selectable("(None)", CurrentValue.empty()))
            {
                if (ApplyTextFieldValue(Field, FString()))
                {
                    RefreshPayloadBuffers();
                }
                ImGui::CloseCurrentPopup();
            }

            for (const FNotifyAssetPickerEntry& Entry : PickerEntries)
            {
                if (!MatchesFilter(Entry.DisplayName, SearchFilter) &&
                    !MatchesFilter(Entry.StoredValue, SearchFilter))
                {
                    continue;
                }

                bHasVisibleEntries = true;
                const bool bIsSelected = CurrentValue == Entry.StoredValue;
                const FString SelectableLabel =
                    Entry.DisplayName + "##" + Entry.StoredValue;
                if (ImGui::Selectable(SelectableLabel.c_str(), bIsSelected))
                {
                    if (ApplyTextFieldValue(Field, Entry.StoredValue))
                    {
                        RefreshPayloadBuffers();
                    }
                    ImGui::CloseCurrentPopup();
                }

                if (ImGui::IsItemHovered() && Entry.DisplayName != Entry.StoredValue)
                {
                    ImGui::SetTooltip("%s", Entry.StoredValue.c_str());
                }

                if (bIsSelected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }

            if (!bHasVisibleEntries && !PickerEntries.empty())
            {
                AnimationSequenceViewerUi::DrawCompactEmptyState(
                    "No assets match the current search.");
            }
            else if (PickerEntries.empty())
            {
                AnimationSequenceViewerUi::DrawCompactEmptyState(
                    GetNotifyAssetPickerEmptyMessage(Field.AssetKind));
            }

            ImGui::EndChild();
            ImGui::EndPopup();
        }

        ImGui::TextDisabled("%s", GetNotifyAssetPickerManualFallbackMessage(Field.AssetKind));
        ImGui::Unindent(NotifyPanelSectionIndent);
    };

    // Preview-aware pickers pull candidate data from the preview controller,
    // but still write back through the same structured payload mutation path as
    // plain text fields.
    auto CollectPreviewPickerOptions = [this](const FAnimNotifyPayloadFieldDefinition& Field)
    {
        if (!PreviewController)
        {
            return TArray<FString>();
        }

        if (Field.EditorHint == EAnimNotifyPayloadFieldEditorHint::SocketPicker)
        {
            return PreviewController->GetPreviewSocketNames();
        }

        if (Field.EditorHint == EAnimNotifyPayloadFieldEditorHint::ComponentPicker)
        {
            return PreviewController->GetPreviewPrimitiveComponentNames();
        }

        return TArray<FString>();
    };

    auto RenderPreviewPickerField = [this, &RefreshPayloadBuffers, &ApplyTextFieldValue, &CollectPreviewPickerOptions](
        const FAnimNotifyPayloadFieldDefinition& Field,
        std::array<char, 256>& TargetBuffer)
    {
        const TArray<FString> PickerOptions = CollectPreviewPickerOptions(Field);
        if (PickerOptions.empty())
        {
            return false;
        }

        const FString CurrentValue = TargetBuffer.data();
        const char* PreviewValue = CurrentValue.empty() ? "(None)" : CurrentValue.c_str();
        SetNotifyPanelFieldWidth();
        if (ImGui::BeginCombo(Field.Label.c_str(), PreviewValue))
        {
            if (ImGui::Selectable("(None)", CurrentValue.empty()))
            {
                if (ApplyTextFieldValue(Field, FString()))
                {
                    RefreshPayloadBuffers();
                }
            }

            for (const FString& Option : PickerOptions)
            {
                const bool bIsSelected = CurrentValue == Option;
                if (ImGui::Selectable(Option.c_str(), bIsSelected))
                {
                    if (ApplyTextFieldValue(Field, Option))
                    {
                        RefreshPayloadBuffers();
                    }
                }

                if (bIsSelected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }

            ImGui::EndCombo();
        }

        const char* PickerHelpText =
            Field.EditorHint == EAnimNotifyPayloadFieldEditorHint::ComponentPicker
                ? "Pick from the active preview primitive components. Use Advanced Raw Payload for a manual override."
                : "Pick from the active preview attach targets. Use Advanced Raw Payload for a manual override.";
        ImGui::TextDisabled("%s", PickerHelpText);
        return true;
    };

    // Primitive field renderers own draw/apply sequencing, while raw payload
    // serialization and buffer rebinding stay centralized in the shared helpers above.
    auto RenderTextPayloadField = [this, &RefreshPayloadBuffers, &ApplyTextFieldValue, &RenderAssetPickerField, &RenderPreviewPickerField](
        const FAnimNotifyPayloadFieldDefinition& Field)
    {
        std::array<char, 256>* TargetBuffer = GetNotifyTextFieldBuffer(Field.SemanticId);
        if (!TargetBuffer)
        {
            return;
        }

        if (Field.EditorHint == EAnimNotifyPayloadFieldEditorHint::AssetPicker)
        {
            RenderAssetPickerField(Field, *TargetBuffer);
            return;
        }

        if (RenderPreviewPickerField(Field, *TargetBuffer))
        {
            return;
        }

        SetNotifyPanelFieldWidth();
        if (ImGui::InputText(Field.Label.c_str(), TargetBuffer->data(), TargetBuffer->size()))
        {
            const FString Value = TargetBuffer->data();
            if (ApplyTextFieldValue(Field, Value))
            {
                RefreshPayloadBuffers();
            }
        }

        if (const char* UnavailableMessage = GetPickerUnavailableMessage(Field.EditorHint))
        {
            ImGui::TextDisabled("%s", UnavailableMessage);
        }
    };

    auto RenderFloatPayloadField = [this, &Payload, &RefreshPayloadBuffers](
        const FAnimNotifyPayloadFieldDefinition& Field)
    {
        float FieldValue = GetSchemaFloatValue(Payload, Field);
        SetNotifyPanelFieldWidth();
        if (ImGui::DragFloat(Field.Label.c_str(), &FieldValue, 0.01f, 0.0f, 100.0f, "%.3f"))
        {
            if (Document->SetSelectedNotifyPayloadFloatValue(Field.Key, FieldValue))
            {
                RefreshPayloadBuffers();
            }
        }
    };

    auto RenderIntPayloadField = [this, &Payload, &RefreshPayloadBuffers](
        const FAnimNotifyPayloadFieldDefinition& Field)
    {
        int32 FieldValue = GetSchemaIntValue(Payload, Field);
        SetNotifyPanelFieldWidth();
        if (ImGui::InputInt(Field.Label.c_str(), &FieldValue))
        {
            FAnimNotifyPayloadParser UpdatedPayload = Document->GetSelectedNotifyPayloadParser();
            UpdatedPayload.RemoveAny(Field.LegacyKeys);
            UpdatedPayload.SetInt(Field.Key, FieldValue);
            if (Document->SetSelectedNotifyPayload(UpdatedPayload.SerializeCanonical()))
            {
                RefreshPayloadBuffers();
            }
        }
    };

    auto RenderBoolPayloadField = [this, &Payload, &RefreshPayloadBuffers](
        const FAnimNotifyPayloadFieldDefinition& Field)
    {
        bool FieldValue = GetSchemaBoolValue(Payload, Field);
        if (ImGui::Checkbox(Field.Label.c_str(), &FieldValue))
        {
            if (Document->SetSelectedNotifyPayloadBoolValue(Field.Key, FieldValue))
            {
                RefreshPayloadBuffers();
            }
        }
    };

    ImGui::TextDisabled("%s Payload", Schema->SectionLabel.c_str());
    for (const FAnimNotifyPayloadFieldDefinition& Field : Schema->Fields)
    {
        switch (Field.Type)
        {
        case EAnimNotifyPayloadFieldType::String:
        case EAnimNotifyPayloadFieldType::Name:
            RenderTextPayloadField(Field);
            break;

        case EAnimNotifyPayloadFieldType::Float:
            RenderFloatPayloadField(Field);
            break;

        case EAnimNotifyPayloadFieldType::Int:
            RenderIntPayloadField(Field);
            break;

        case EAnimNotifyPayloadFieldType::Bool:
            RenderBoolPayloadField(Field);
            break;

        default:
            break;
        }

        if (!Field.HelpText.empty())
        {
            ImGui::Indent(NotifyPanelSectionIndent);
            ImGui::TextDisabled("%s", Field.HelpText.c_str());
            ImGui::Unindent(NotifyPanelSectionIndent);
        }
    }

    const FString PayloadPreview = CurrentNotify
        ? FAnimNotifyPayloadParser(CurrentNotify->Payload).SerializeCanonical()
        : Payload.SerializeCanonical();
    ImGui::TextWrapped(
        "Payload Preview: %s",
        PayloadPreview.empty() ? "(Empty)" : PayloadPreview.c_str());

    RenderNotifyValidationIssues(Report, EAnimNotifyValidationField::Payload);

    if (ImGui::CollapsingHeader("Advanced Raw Payload"))
    {
        RenderRawNotifyPayloadEditor(NotifyClassName, Report, false);
    }
}

void FAnimationSequenceEditorWidget::RenderRawNotifyPayloadEditor(
    const FString& NotifyClassName,
    const FAnimNotifyValidationReport& Report,
    bool bRenderValidation)
{
    if (!Document)
    {
        return;
    }

    SetNotifyPanelFieldWidth();
    if (ImGui::InputText("Payload", NotifyPayloadEditBuffer.data(), NotifyPayloadEditBuffer.size()))
    {
        // Raw payload editing is the source of truth fallback. Keep the
        // structured field buffers synchronized so both editing paths stay
        // consistent when a user overrides the payload manually.
        Document->SetSelectedNotifyPayload(NotifyPayloadEditBuffer.data());
        const FAnimNotifyPayloadParser Payload(NotifyPayloadEditBuffer.data());
        RefreshNotifyPayloadFieldBuffers(Payload);
    }

    if (const char* PayloadExample = GetAnimNotifyPayloadExample(NotifyClassName))
    {
        ImGui::TextWrapped("Payload Example: %s", PayloadExample);
    }

    if (bRenderValidation)
    {
        RenderNotifyValidationIssues(Report, EAnimNotifyValidationField::Payload);
    }
}
