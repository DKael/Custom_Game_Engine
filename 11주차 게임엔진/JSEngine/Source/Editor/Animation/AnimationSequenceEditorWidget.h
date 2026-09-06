#pragma once

#include "Asset/SkeletalMeshTypes.h"
#include "Editor/Animation/AnimationSequenceNotifyValidation.h"
#include "Editor/Animation/AnimationSequenceCurveTrackWidget.h"
#include "Editor/Animation/AnimationSequenceCurveFilter.h"
#include "Editor/Animation/AnimationSequenceNotifyLaneWidget.h"
#include "Editor/Animation/AnimationSequenceSequencerVisibleLayout.h"
#include "Editor/Animation/AnimationSequenceTrackOutlinerWidget.h"
#include "Editor/Animation/AnimationSequenceTimelineWidget.h"
#include "Editor/UI/EditorWidget.h"

#include <array>

class UAnimSequence;
class UTexture;
class FAnimationSequenceEditorState;
class FAnimationSequencePreviewController;
class FAnimationSequenceEditorDocument;
class FAnimNotifyPayloadParser;
enum class EAnimNotifySemanticFieldId : uint8;
struct FAnimNotifyEvent;
struct FAnimationSequenceTimelineGeometry;
struct ImVec2;

class FAnimationSequenceEditorWidget : public FEditorWidget
{
public:
    void Render(float DeltaTime) override;
    int32 GetSelectedPreviewBoneIndex() const { return PreviewSkeletonTreeSelectedBoneIndex; }

    void BindDocumentContext(
        FAnimationSequenceEditorDocument* InDocument,
        const FString& InSequencePath,
        UAnimSequence* InSequence,
        FAnimationSequencePreviewController* InPreviewController,
        FAnimationSequenceEditorState* InEditorState);

private:
    enum class ENotifyValidationBrowserSeverityFilter : uint8
    {
        All = 0,
        Errors,
        Warnings,
        Info,
    };

    // Preview area
    TArray<FString> BuildPreviewOverlayLines() const;
    void RenderPreviewPane(float PreviewHeight, const TArray<FString>& PreviewOverlayLines);
    void RenderPreviewSkeletonTree(float Height);
    void RenderPreviewViewportToolbar();
    void RenderPreviewImage(
        const ImVec2& PreviewSize,
        const TArray<FString>& PreviewOverlayLines,
        bool bBlockViewportInputThisFrame);
    void RenderPreviewFallback(const ImVec2& PreviewSize, const TArray<FString>& PreviewOverlayLines) const;
    void SyncEmbeddedViewportRectAndFocus(
        const ImVec2& Min,
        const ImVec2& Max,
        bool bViewportClicked,
        bool bBlockViewportInputThisFrame);
    void RenderPreviewToolbarOverlay(const ImVec2& Min, const ImVec2& Max) const;
    void DrawPreviewSkeletonBoneNode(
        int32 BoneIndex,
        const TArray<FBoneInfo>& Bones,
        const TArray<TArray<int32>>& Children);
    void RebuildPreviewSkeletonTreeCaches(const FSkeletalMesh* MeshData);
    void QueuePreviewBoneSubtreeOpenState(int32 BoneIndex, bool bOpen);
    void ApplyPendingPreviewBoneTreeOpenState(const FSkeletalMesh* MeshData);
    void SetPreviewBoneSubtreeOpenState(
        int32 BoneIndex,
        const TArray<TArray<int32>>& Children,
        bool bOpen);

    // Sequencer area
    void RenderSequencerRegion(float SequencerHeight, float MaxPreviewHeight);
    void RenderSequencerSplitPane(
        float SplitPaneHeight,
        float MaxOutlinerWidth,
        const TArray<FAnimationSequenceCurveViewGroup>& CurveGroups,
        const TArray<FAnimationSequenceCurveViewGroup>& AttributeCurveGroups);
    FAnimationSequenceSequencerVisibleLayout BuildSequencerVisibleLayout(
        const TArray<FAnimationSequenceCurveViewGroup>& CurveGroups,
        const TArray<FAnimationSequenceCurveViewGroup>& AttributeCurveGroups) const;
    void RenderTrackOutlinerPane(
        float Width,
        float Height,
        const FAnimationSequenceSequencerVisibleLayout& VisibleLayout);
    void RenderTimelineCanvasPane(
        float Width,
        float Height,
        const FAnimationSequenceSequencerVisibleLayout& VisibleLayout);
    void DrawTimelineRowBackgrounds(
        const FAnimationSequenceTimelineGeometry& Geometry,
        const FAnimationSequenceSequencerVisibleLayout& VisibleLayout,
        float TimelineRowOriginY) const;

    // Bottom control strip and playback controls
    void RenderBottomControlStrip(float Height, float LeftPaneWidth, float RightPaneWidth);
    void RenderTransportControls(bool bCanTimelineControl, bool bCanPlaybackControl, float StripHeight);
    void RenderTimelineRangeControls(float StripHeight);
    void RenderPlaybackSpeedPopup();
    void EnsurePlaybackControlIconsLoaded();
    bool DrawPlaybackControlButton(
        const char* ButtonId,
        UTexture* IconTexture,
        const char* FallbackLabel,
        const char* Tooltip,
        const ImVec2& Size,
        bool bSelected = false) const;

    // Lower details/debug area orchestration
    void RenderSelectionDetailsPane(float Height);
    void RenderNotifyLowerPanel(float Height);
    void RenderNotifyLowerPanelSplit();

    // Selected notify details panel
    void RenderSelectedNotifyEditorPanel();
    void RenderSelectedNotifyToolbar(bool bHasSelection, int32 DefaultTrackIndex);
    void RenderSelectedNotifyBasicSection(
        const FAnimNotifyEvent& NotifySnapshot,
        const FAnimNotifyValidationReport& SelectedValidationReport);
    void RenderSelectedNotifyTimingSection(
        const FAnimNotifyEvent& NotifySnapshot,
        const FAnimNotifyValidationReport& SelectedValidationReport);
    void RenderSelectedNotifyPayloadSection(
        const FString& CurrentNotifyClassName,
        const FAnimNotifyValidationReport& SelectedValidationReport);
    void RenderSelectedNotifyVisualSection(const FAnimNotifyEvent& NotifySnapshot);
    void RenderSelectedNotifyInlineValidationSection(const FAnimNotifyValidationReport& SelectedValidationReport) const;
    void RenderSelectedNotifyMetaFooter(const FAnimNotifyEvent& NotifySnapshot) const;

    // Shared payload/selected validation helpers
    void RenderNotifyValidationSummary(const FAnimNotifyValidationReport& Report) const;
    void RenderNotifyValidationIssues(
        const FAnimNotifyValidationReport& Report,
        EAnimNotifyValidationField Field) const;
    void RenderStructuredNotifyPayloadEditor(
        const FString& NotifyClassName,
        const FAnimNotifyValidationReport& Report);
    void RenderRawNotifyPayloadEditor(
        const FString& NotifyClassName,
        const FAnimNotifyValidationReport& Report,
        bool bRenderValidation);

    // Notify debug panel
    void RenderNotifyDebugPanel();
    void RenderNotifyDebugTabs();
    void RenderNotifyValidationTab();
    void RenderNotifyValidationSummaryBlock(const FAnimNotifyValidationReport& Report) const;
    void RenderNotifyValidationFilters();
    void RenderNotifyValidationMessageList(const FAnimNotifyValidationReport& Report);
    void RenderRecentFiredNotifyTab() const;
    void RenderNotifyEventLogTab() const;

    // Curve inspection panel
    void RenderCurveInspectionPanel();

    // Notify editor buffers/state helpers
    void SyncNotifyDetailsBuffers(const FAnimNotifyEvent& NotifyEvent);
    void ResetNotifyPayloadFieldBuffers();
    void RefreshNotifyPayloadFieldBuffers(const FAnimNotifyPayloadParser& Payload);
    std::array<char, 256>* GetNotifyTextFieldBuffer(EAnimNotifySemanticFieldId SemanticId);

private:
    FAnimationSequenceEditorDocument* Document = nullptr;
    FString SequencePath;
    UAnimSequence* Sequence = nullptr;
    FAnimationSequencePreviewController* PreviewController = nullptr;
    FAnimationSequenceEditorState* EditorState = nullptr;
    FAnimationSequenceTimelineWidget TimelineWidget;
    FAnimationSequenceNotifyLaneWidget NotifyLaneWidget;
    FAnimationSequenceTrackOutlinerWidget TrackOutlinerWidget;
    FAnimationSequenceCurveTrackWidget CurveTrackWidget;
    struct FPlaybackControlIconCache
    {
        bool bAttemptedLoad = false;
        UTexture* JumpToStart = nullptr;
        UTexture* StepPrevious = nullptr;
        UTexture* PlayReverse = nullptr;
        UTexture* Record = nullptr;
        UTexture* PlayForward = nullptr;
        UTexture* Pause = nullptr;
        UTexture* Stop = nullptr;
        UTexture* StepNext = nullptr;
        UTexture* JumpToEnd = nullptr;
        UTexture* LoopEnabled = nullptr;
        UTexture* LoopDisabled = nullptr;
    } PlaybackControlIcons;
    FString NotifyDetailsBoundStableId;
    std::array<char, 256> NotifyNameEditBuffer = {};
    std::array<char, 256> NotifySoundCueEditBuffer = {};
    std::array<char, 256> NotifySocketNameEditBuffer = {};
    std::array<char, 256> NotifyComponentNameEditBuffer = {};
    std::array<char, 256> NotifyAttackIdEditBuffer = {};
    std::array<char, 256> NotifyGameplayEventNameEditBuffer = {};
    std::array<char, 256> NotifyGameplayEndEventNameEditBuffer = {};
    std::array<char, 256> NotifyGameplayPayloadValueEditBuffer = {};
    std::array<char, 256> NotifyVFXEffectEditBuffer = {};
    std::array<char, 256> NotifyDecalEditBuffer = {};
    std::array<char, 256> NotifyCameraShakeEditBuffer = {};
    std::array<char, 512> NotifyPayloadEditBuffer = {};
    std::array<char, 128> NotifyAssetPickerSearchBuffer = {};
    std::array<char, 128> TrackFilterEditBuffer = {};
    std::array<char, 64> TimelineRangeEditBuffer = {};
    FString ActiveTimelineRangeEditPopupId;
    FString ActiveNotifyAssetPickerPopupId;
    ENotifyValidationBrowserSeverityFilter ActiveNotifyValidationSeverityFilter =
        ENotifyValidationBrowserSeverityFilter::All;
    float PlaybackSpeedCustomValue = 1.0f;
    TArray<TArray<int32>> PreviewSkeletonTreeChildren;
    const FSkeletalMesh* PreviewSkeletonTreeCachedMesh = nullptr;
    int32 PreviewSkeletonTreeSelectedBoneIndex = -1;
    int32 PendingPreviewBoneTreeOpenStateRoot = -1;
    bool bPendingPreviewBoneTreeOpenStateValue = false;
    float PreviewSkeletonTreeWidth = 240.0f;
    bool bSuppressEmbeddedPreviewViewportInput = false;
};
