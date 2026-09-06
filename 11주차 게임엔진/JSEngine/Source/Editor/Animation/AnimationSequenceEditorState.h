#pragma once

#include "Animation/AnimData/FrameRate.h"
#include "Editor/Animation/AnimationSequenceSequencerVisibleLayout.h"
#include "Core/CoreMinimal.h"

class UAnimSequence;

enum class EAnimNotifyDragMode : uint8
{
    None = 0,
    Move,
    ResizeEnd,
};

class FAnimationSequenceEditorState
{
public:
    void Reset();
    void InitializeFromSequence(const UAnimSequence* Sequence, float InSequenceLength, bool bInLoop, float InPlayRate);

    double GetDisplayFPS() const;
    bool HasTimelineData() const;
    float GetMaxTime() const;
    float GetMinimumVisibleRange() const;
    float GetVisibleRange() const;
    float GetTimelineRangeStart() const { return TimelineRangeStart; }
    float GetTimelineRangeEnd() const { return TimelineRangeEnd; }
    float GetPlaybackRangeStart() const { return PlaybackRangeStart; }
    float GetPlaybackRangeEnd() const { return PlaybackRangeEnd; }
    float GetPlaybackRangeLength() const;

    float ClampTime(float InTime) const;
    float SnapTimeToFrame(float InTime) const;
    float ClampOrSnapTime(float InTime) const;

    int32 TimeToFrameIndex(float InTime) const;
    float FrameToTime(int32 FrameIndex) const;

    void SetCurrentTime(float InTime, bool bApplySnap);
    void SetTimelineRange(float InStartTime, float InEndTime);
    void SetPlaybackRange(float InStartTime, float InEndTime);
    void SetVisibleRange(float InStartTime, float InEndTime);
    void PanVisibleRange(float DeltaTime);
    void ZoomVisibleRange(float AnchorTime, float ZoomFactor);

    int32 ChooseMajorFrameStep(float TimelinePixelWidth) const;
    int32 ChooseMinorFrameStep(float TimelinePixelWidth) const;
    void SetHoveredSequencerRow(const FAnimationSequenceSequencerRowId& RowId);
    void ClearHoveredSequencerRow();
    void SetFocusedSequencerRow(const FAnimationSequenceSequencerRowId& RowId);
    void ClearFocusedSequencerRow();
    bool IsHoveredSequencerRow(const FAnimationSequenceSequencerRowId& RowId) const;
    bool IsFocusedSequencerRow(const FAnimationSequenceSequencerRowId& RowId) const;
    bool HasSelectedNotify() const
    {
        return SelectedNotifyTrackIndex >= 0 && SelectedNotifyEventIndex >= 0;
    }

public:
    float CurrentTime = 0.0f;
    float SequenceLength = 0.0f;
    FFrameRate DisplayFrameRate = FFrameRate();
    int32 TotalFrames = 0;
    bool bLoop = true;
    float PlayRate = 1.0f;
    bool bSnapToFrames = true;
    float VisibleTimeStart = 0.0f;
    float VisibleTimeEnd = 1.0f;
    float TimelineRangeStart = 0.0f;
    float TimelineRangeEnd = 1.0f;
    float PlaybackRangeStart = 0.0f;
    float PlaybackRangeEnd = 1.0f;
    float PreviewPaneHeight = 0.0f;
    float SequencerDetailsPaneHeight = 0.0f;
    float TrackOutlinerWidth = 0.0f;
    float SequencerScrollY = 0.0f;
    bool bNotifiesExpanded = true;
    bool bCurvesExpanded = true;
    bool bAdditiveLayerTracksExpanded = false;
    bool bAttributesExpanded = true;
    bool bShowMorphTargetCurves = true;
    bool bShowMaterialCurves = true;
    bool bShowAttributeCurves = true;
    bool bShowUnknownCurves = false;
    bool bShowNotifyValidationDetails = true;
    int32 SelectedNotifyTrackIndex = -1;
    int32 SelectedNotifyEventIndex = -1;
    int32 HoveredNotifyTrackIndex = -1;
    int32 HoveredNotifyEventIndex = -1;
    int32 DraggedNotifyTrackIndex = -1;
    int32 DraggedNotifyEventIndex = -1;
    int32 SelectedCurveIndex = -1;
    int32 HoveredCurveIndex = -1;
    bool bHasHoveredCurveSample = false;
    float HoveredCurveSampleTime = 0.0f;
    float HoveredCurveSampleValue = 0.0f;
    int32 HoveredCurveNearestKeyIndex = -1;
    FAnimationSequenceSequencerRowId HoveredSequencerRowId;
    FAnimationSequenceSequencerRowId FocusedSequencerRowId;
    bool bDraggingNotify = false;
    EAnimNotifyDragMode ActiveNotifyDragMode = EAnimNotifyDragMode::None;
    float DraggedNotifyGrabOffsetTime = 0.0f;
};
