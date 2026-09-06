#include "Editor/Animation/AnimationSequenceEditorWidget.h"

#include "Animation/AnimNotifyPayloadParser.h"
#include "Animation/AnimNotifySemanticFieldNames.h"
#include "Editor/Animation/AnimationSequenceNotifyPayloadSchema.h"
#include "Editor/Animation/AnimationSequenceEditorState.h"
#include "Editor/Animation/AnimationSequencePreviewController.h"
#include "Editor/Animation/AnimationSequenceSequencerLayout.h"

#include "ImGui/imgui.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#ifdef GetCurrentTime
#undef GetCurrentTime
#endif

namespace
{
    template <size_t BufferSize>
    void CopyStringToBuffer(const FString& Source, std::array<char, BufferSize>& Buffer)
    {
        Buffer.fill('\0');
        strncpy_s(Buffer.data(), Buffer.size(), Source.c_str(), _TRUNCATE);
    }

    template <size_t BufferSize>
    void CopyPayloadFieldToBuffer(
        const FAnimNotifyPayloadParser& Payload,
        const FAnimNotifyPayloadFieldDefinition& Field,
        std::array<char, BufferSize>& Buffer)
    {
        CopyStringToBuffer(Payload.GetStringAny(GetAnimNotifyPayloadFieldLookupKeys(Field)), Buffer);
    }

}

//----------------------------------------------------------------------------
// Binding and lifecycle
//----------------------------------------------------------------------------

void FAnimationSequenceEditorWidget::BindDocumentContext(
    FAnimationSequenceEditorDocument* InDocument,
    const FString& InSequencePath,
    UAnimSequence* InSequence,
    FAnimationSequencePreviewController* InPreviewController,
    FAnimationSequenceEditorState* InEditorState)
{
    Document = InDocument;
    SequencePath = InSequencePath;
    Sequence = InSequence;
    PreviewController = InPreviewController;
    EditorState = InEditorState;
    NotifyDetailsBoundStableId.clear();
    NotifyNameEditBuffer.fill('\0');
    ResetNotifyPayloadFieldBuffers();
    NotifyPayloadEditBuffer.fill('\0');
    TrackFilterEditBuffer.fill('\0');
    TimelineRangeEditBuffer.fill('\0');
    ActiveTimelineRangeEditPopupId.clear();
    ActiveNotifyValidationSeverityFilter = ENotifyValidationBrowserSeverityFilter::All;
    PlaybackSpeedCustomValue = InPreviewController ? std::fabs(InPreviewController->GetPlayRate()) : 1.0f;
    PreviewSkeletonTreeChildren.clear();
    PreviewSkeletonTreeCachedMesh = nullptr;
    PreviewSkeletonTreeSelectedBoneIndex = -1;
    PendingPreviewBoneTreeOpenStateRoot = -1;
    bPendingPreviewBoneTreeOpenStateValue = false;
    PreviewSkeletonTreeWidth = 240.0f;
    bSuppressEmbeddedPreviewViewportInput = false;
}

//----------------------------------------------------------------------------
// Top-level viewer orchestration
//----------------------------------------------------------------------------

void FAnimationSequenceEditorWidget::Render(float DeltaTime)
{
    (void)DeltaTime;

    // Keep the main widget focused on orchestration only. Preview, sequencer,
    // bottom strip, and lower details own their own implementation units; this
    // entrypoint just resolves the current layout and delegates to them.
    const TArray<FString> PreviewOverlayLines = BuildPreviewOverlayLines();
    const float AvailableHeight = std::max(ImGui::GetContentRegionAvail().y, 300.0f);
    const float MinLayoutHeight =
        FAnimationSequenceSequencerLayout::PreviewMinHeight +
        FAnimationSequenceSequencerLayout::SequencerMinHeight +
        FAnimationSequenceSequencerLayout::SectionSplitterHeight;
    const float SplitLayoutHeight = std::max(AvailableHeight, MinLayoutHeight);
    const float MaxPreviewHeight =
        std::max(
            FAnimationSequenceSequencerLayout::PreviewMinHeight,
            SplitLayoutHeight -
            FAnimationSequenceSequencerLayout::SequencerMinHeight -
            FAnimationSequenceSequencerLayout::SectionSplitterHeight);

    if (EditorState && EditorState->PreviewPaneHeight <= 0.0f)
    {
        EditorState->PreviewPaneHeight = SplitLayoutHeight * 0.58f;
    }

    const float PreviewHeight = EditorState
        ? std::clamp(EditorState->PreviewPaneHeight, FAnimationSequenceSequencerLayout::PreviewMinHeight, MaxPreviewHeight)
        : SplitLayoutHeight * 0.58f;
    const float SequencerHeight = std::max(
        FAnimationSequenceSequencerLayout::SequencerMinHeight,
        SplitLayoutHeight - PreviewHeight - FAnimationSequenceSequencerLayout::SectionSplitterHeight);

    if (EditorState)
    {
        EditorState->PreviewPaneHeight = PreviewHeight;
    }

    RenderPreviewPane(PreviewHeight, PreviewOverlayLines);

    if (EditorState)
    {
        RenderSequencerRegion(SequencerHeight, MaxPreviewHeight);
    }
}

//----------------------------------------------------------------------------
// Lower details/debug area
//----------------------------------------------------------------------------

void FAnimationSequenceEditorWidget::RenderSelectionDetailsPane(float Height)
{
    RenderNotifyLowerPanel(Height);
}

void FAnimationSequenceEditorWidget::RenderNotifyLowerPanel(float Height)
{
    ImGui::BeginChild("##AnimationSequenceSelectionDetails", ImVec2(0.0f, Height), true);
    RenderNotifyLowerPanelSplit();
    ImGui::EndChild();
}

void FAnimationSequenceEditorWidget::RenderNotifyLowerPanelSplit()
{
    if (ImGui::BeginTable("##AnimationSequenceSelectionDetailsTable", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable))
    {
        ImGui::TableSetupColumn("Selection", ImGuiTableColumnFlags_WidthStretch, 0.55f);
        ImGui::TableSetupColumn("Activity", ImGuiTableColumnFlags_WidthStretch, 0.45f);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        if (ImGui::BeginChild("##AnimationSequenceSelectionColumn", ImVec2(0.0f, 0.0f), false))
        {
            RenderSelectedNotifyEditorPanel();
            RenderCurveInspectionPanel();
        }
        ImGui::EndChild();

        ImGui::TableSetColumnIndex(1);
        if (ImGui::BeginChild("##AnimationSequenceNotifyDebugColumn", ImVec2(0.0f, 0.0f), false))
        {
            RenderNotifyDebugPanel();
        }
        ImGui::EndChild();
        ImGui::EndTable();
    }
}

//----------------------------------------------------------------------------
// Shared viewer helpers and notify editor buffer sync
//----------------------------------------------------------------------------

void FAnimationSequenceEditorWidget::ResetNotifyPayloadFieldBuffers()
{
    NotifySoundCueEditBuffer.fill('\0');
    NotifySocketNameEditBuffer.fill('\0');
    NotifyComponentNameEditBuffer.fill('\0');
    NotifyAttackIdEditBuffer.fill('\0');
    NotifyGameplayEventNameEditBuffer.fill('\0');
    NotifyGameplayEndEventNameEditBuffer.fill('\0');
    NotifyGameplayPayloadValueEditBuffer.fill('\0');
    NotifyVFXEffectEditBuffer.fill('\0');
    NotifyDecalEditBuffer.fill('\0');
    NotifyCameraShakeEditBuffer.fill('\0');
}

void FAnimationSequenceEditorWidget::RefreshNotifyPayloadFieldBuffers(const FAnimNotifyPayloadParser& Payload)
{
    const FAnimNotifyPayloadFieldDefinition SoundCueField =
    {
        AnimNotifySemanticFieldNames::SoundCueKey(),
        EAnimNotifySemanticFieldId::SoundCue,
        AnimNotifySemanticFieldNames::GetLegacyAliases(AnimNotifySemanticFieldNames::SoundCueKey())
    };
    const FAnimNotifyPayloadFieldDefinition SocketNameField =
    {
        AnimNotifySemanticFieldNames::SocketNameKey(),
        EAnimNotifySemanticFieldId::SocketName,
        AnimNotifySemanticFieldNames::GetLegacyAliases(AnimNotifySemanticFieldNames::SocketNameKey())
    };
    const FAnimNotifyPayloadFieldDefinition ComponentNameField =
    {
        AnimNotifySemanticFieldNames::ComponentNameKey(),
        EAnimNotifySemanticFieldId::ComponentName,
        AnimNotifySemanticFieldNames::GetLegacyAliases(AnimNotifySemanticFieldNames::ComponentNameKey())
    };
    const FAnimNotifyPayloadFieldDefinition AttackIdField =
    {
        AnimNotifySemanticFieldNames::AttackIdKey(),
        EAnimNotifySemanticFieldId::AttackId,
        AnimNotifySemanticFieldNames::GetLegacyAliases(AnimNotifySemanticFieldNames::AttackIdKey())
    };
    const FAnimNotifyPayloadFieldDefinition EventNameField =
    {
        AnimNotifySemanticFieldNames::EventNameKey(),
        EAnimNotifySemanticFieldId::EventName,
        AnimNotifySemanticFieldNames::GetLegacyAliases(AnimNotifySemanticFieldNames::EventNameKey())
    };
    const FAnimNotifyPayloadFieldDefinition PayloadField =
    {
        AnimNotifySemanticFieldNames::PayloadKey(),
        EAnimNotifySemanticFieldId::PayloadValue,
        AnimNotifySemanticFieldNames::GetLegacyAliases(AnimNotifySemanticFieldNames::PayloadKey())
    };
    const FAnimNotifyPayloadFieldDefinition EndEventNameField =
    {
        AnimNotifySemanticFieldNames::EndEventNameKey(),
        EAnimNotifySemanticFieldId::EndEventName,
        AnimNotifySemanticFieldNames::GetLegacyAliases(AnimNotifySemanticFieldNames::EndEventNameKey())
    };
    const FAnimNotifyPayloadFieldDefinition ShakeField =
    {
        AnimNotifySemanticFieldNames::ShakeKey(),
        EAnimNotifySemanticFieldId::Shake,
        AnimNotifySemanticFieldNames::GetLegacyAliases(AnimNotifySemanticFieldNames::ShakeKey())
    };
    const FAnimNotifyPayloadFieldDefinition EffectField =
    {
        AnimNotifySemanticFieldNames::EffectKey(),
        EAnimNotifySemanticFieldId::Effect,
        AnimNotifySemanticFieldNames::GetLegacyAliases(AnimNotifySemanticFieldNames::EffectKey())
    };
    const FAnimNotifyPayloadFieldDefinition DecalField =
    {
        AnimNotifySemanticFieldNames::DecalKey(),
        EAnimNotifySemanticFieldId::Decal,
        AnimNotifySemanticFieldNames::GetLegacyAliases(AnimNotifySemanticFieldNames::DecalKey())
    };

    CopyPayloadFieldToBuffer(Payload, SoundCueField, NotifySoundCueEditBuffer);
    CopyPayloadFieldToBuffer(Payload, SocketNameField, NotifySocketNameEditBuffer);
    CopyPayloadFieldToBuffer(Payload, ComponentNameField, NotifyComponentNameEditBuffer);
    CopyPayloadFieldToBuffer(Payload, AttackIdField, NotifyAttackIdEditBuffer);
    CopyPayloadFieldToBuffer(Payload, EventNameField, NotifyGameplayEventNameEditBuffer);
    CopyPayloadFieldToBuffer(Payload, EndEventNameField, NotifyGameplayEndEventNameEditBuffer);
    CopyPayloadFieldToBuffer(Payload, PayloadField, NotifyGameplayPayloadValueEditBuffer);
    CopyPayloadFieldToBuffer(Payload, EffectField, NotifyVFXEffectEditBuffer);
    CopyPayloadFieldToBuffer(Payload, DecalField, NotifyDecalEditBuffer);
    CopyPayloadFieldToBuffer(Payload, ShakeField, NotifyCameraShakeEditBuffer);
}

std::array<char, 256>* FAnimationSequenceEditorWidget::GetNotifyTextFieldBuffer(EAnimNotifySemanticFieldId SemanticId)
{
    switch (SemanticId)
    {
    case EAnimNotifySemanticFieldId::SoundCue:
        return &NotifySoundCueEditBuffer;
    case EAnimNotifySemanticFieldId::SocketName:
        return &NotifySocketNameEditBuffer;
    case EAnimNotifySemanticFieldId::ComponentName:
        return &NotifyComponentNameEditBuffer;
    case EAnimNotifySemanticFieldId::AttackId:
        return &NotifyAttackIdEditBuffer;
    case EAnimNotifySemanticFieldId::EventName:
        return &NotifyGameplayEventNameEditBuffer;
    case EAnimNotifySemanticFieldId::EndEventName:
        return &NotifyGameplayEndEventNameEditBuffer;
    case EAnimNotifySemanticFieldId::PayloadValue:
        return &NotifyGameplayPayloadValueEditBuffer;
    case EAnimNotifySemanticFieldId::Effect:
        return &NotifyVFXEffectEditBuffer;
    case EAnimNotifySemanticFieldId::Decal:
        return &NotifyDecalEditBuffer;
    case EAnimNotifySemanticFieldId::Shake:
        return &NotifyCameraShakeEditBuffer;
    default:
        return nullptr;
    }
}

void FAnimationSequenceEditorWidget::SyncNotifyDetailsBuffers(const FAnimNotifyEvent& NotifyEvent)
{
    const FString StableId = NotifyEvent.StableId.ToString();
    if (NotifyDetailsBoundStableId == StableId)
    {
        return;
    }

    // Rebind the editable ImGui buffers only when the selected notify identity
    // changes. This avoids clobbering in-progress typing every frame while still
    // refreshing correctly when selection moves to a different notify.
    NotifyDetailsBoundStableId = StableId;
    const FString NotifyName = NotifyEvent.Name.IsValid() ? NotifyEvent.Name.ToString() : FString();
    CopyStringToBuffer(NotifyName, NotifyNameEditBuffer);
    CopyStringToBuffer(NotifyEvent.Payload, NotifyPayloadEditBuffer);

    const FAnimNotifyPayloadParser Payload(NotifyEvent.Payload);
    RefreshNotifyPayloadFieldBuffers(Payload);
}
