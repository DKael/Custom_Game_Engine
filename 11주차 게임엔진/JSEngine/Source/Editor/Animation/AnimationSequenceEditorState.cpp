#include "Editor/Animation/AnimationSequenceEditorState.h"

#include "Animation/AnimData/AnimSequence.h"
#include "Editor/Animation/AnimationSequenceSequencerLayout.h"
#include "Editor/Animation/AnimationSequenceViewerUtils.h"

#include <algorithm>
#include <array>
#include <cmath>

void FAnimationSequenceEditorState::Reset()
{
    CurrentTime = 0.0f;
    SequenceLength = 0.0f;
    DisplayFrameRate = FFrameRate();
    TotalFrames = 0;
    bLoop = true;
    PlayRate = 1.0f;
    bSnapToFrames = true;
    VisibleTimeStart = 0.0f;
    VisibleTimeEnd = 1.0f;
    TimelineRangeStart = 0.0f;
    TimelineRangeEnd = 1.0f;
    PlaybackRangeStart = 0.0f;
    PlaybackRangeEnd = 1.0f;
    PreviewPaneHeight = 0.0f;
    SequencerDetailsPaneHeight = 0.0f;
    TrackOutlinerWidth = FAnimationSequenceSequencerLayout::OutlinerDefaultWidth;
    SequencerScrollY = 0.0f;
    bNotifiesExpanded = true;
    bCurvesExpanded = true;
    bAdditiveLayerTracksExpanded = false;
    bAttributesExpanded = true;
    bShowMorphTargetCurves = true;
    bShowMaterialCurves = true;
    bShowAttributeCurves = true;
    bShowUnknownCurves = false;
    bShowNotifyValidationDetails = true;
    SelectedNotifyTrackIndex = -1;
    SelectedNotifyEventIndex = -1;
    HoveredNotifyTrackIndex = -1;
    HoveredNotifyEventIndex = -1;
    DraggedNotifyTrackIndex = -1;
    DraggedNotifyEventIndex = -1;
    SelectedCurveIndex = -1;
    HoveredCurveIndex = -1;
    bHasHoveredCurveSample = false;
    HoveredCurveSampleTime = 0.0f;
    HoveredCurveSampleValue = 0.0f;
    HoveredCurveNearestKeyIndex = -1;
    HoveredSequencerRowId = FAnimationSequenceSequencerRowId();
    FocusedSequencerRowId = FAnimationSequenceSequencerRowId();
    bDraggingNotify = false;
    ActiveNotifyDragMode = EAnimNotifyDragMode::None;
    DraggedNotifyGrabOffsetTime = 0.0f;
}

void FAnimationSequenceEditorState::InitializeFromSequence(
    const UAnimSequence* Sequence,
    float InSequenceLength,
    bool bInLoop,
    float InPlayRate)
{
    Reset();

    SequenceLength = std::max(0.0f, InSequenceLength);
    bLoop = bInLoop;
    PlayRate = InPlayRate;

    const UAnimDataModel* DataModel = AnimationSequenceViewer::GetValidAnimDataModel(Sequence);
    if (DataModel)
    {
        DisplayFrameRate = DataModel->FrameRate;
        TotalFrames = std::max(DataModel->NumberOfFrames, DataModel->NumberOfKeys);
    }

    if (SequenceLength <= 0.0f && TotalFrames > 1 && GetDisplayFPS() > 0.0)
    {
        SequenceLength = FrameToTime(TotalFrames - 1);
    }

    const float MaxTime = GetMaxTime();
    const float InitialEnd = MaxTime > 0.0f ? MaxTime : std::max(1.0f, GetMinimumVisibleRange());
    SetTimelineRange(0.0f, InitialEnd);
    SetPlaybackRange(TimelineRangeStart, TimelineRangeEnd);
    SetVisibleRange(TimelineRangeStart, TimelineRangeEnd);
}

double FAnimationSequenceEditorState::GetDisplayFPS() const
{
    return DisplayFrameRate.AsDecimal();
}

bool FAnimationSequenceEditorState::HasTimelineData() const
{
    return TotalFrames > 0 || SequenceLength > 0.0f;
}

float FAnimationSequenceEditorState::GetMaxTime() const
{
    if (TotalFrames > 1 && GetDisplayFPS() > 0.0)
    {
        return FrameToTime(TotalFrames - 1);
    }

    return std::max(0.0f, SequenceLength);
}

float FAnimationSequenceEditorState::GetMinimumVisibleRange() const
{
    const double FPS = GetDisplayFPS();
    if (FPS > 0.0)
    {
        return std::max(1.0f / static_cast<float>(FPS), 1.0f / 240.0f);
    }

    return 1.0f / 30.0f;
}

float FAnimationSequenceEditorState::GetVisibleRange() const
{
    return std::max(VisibleTimeEnd - VisibleTimeStart, GetMinimumVisibleRange());
}

float FAnimationSequenceEditorState::GetPlaybackRangeLength() const
{
    return std::max(PlaybackRangeEnd - PlaybackRangeStart, GetMinimumVisibleRange());
}

float FAnimationSequenceEditorState::ClampTime(float InTime) const
{
    return std::clamp(InTime, 0.0f, GetMaxTime());
}

float FAnimationSequenceEditorState::SnapTimeToFrame(float InTime) const
{
    return FrameToTime(TimeToFrameIndex(InTime));
}

float FAnimationSequenceEditorState::ClampOrSnapTime(float InTime) const
{
    return bSnapToFrames ? SnapTimeToFrame(InTime) : ClampTime(InTime);
}

int32 FAnimationSequenceEditorState::TimeToFrameIndex(float InTime) const
{
    if (TotalFrames <= 0)
    {
        return 0;
    }

    const double FPS = GetDisplayFPS();
    if (FPS <= 0.0)
    {
        return std::clamp(static_cast<int32>(std::round(InTime)), 0, std::max(0, TotalFrames - 1));
    }

    const int32 FrameIndex = static_cast<int32>(std::lround(static_cast<double>(ClampTime(InTime)) * FPS));
    return std::clamp(FrameIndex, 0, std::max(0, TotalFrames - 1));
}

float FAnimationSequenceEditorState::FrameToTime(int32 FrameIndex) const
{
    if (FrameIndex <= 0)
    {
        return 0.0f;
    }

    const double FPS = GetDisplayFPS();
    if (FPS <= 0.0)
    {
        return static_cast<float>(FrameIndex);
    }

    return static_cast<float>(static_cast<double>(FrameIndex) / FPS);
}

void FAnimationSequenceEditorState::SetCurrentTime(float InTime, bool bApplySnap)
{
    CurrentTime = bApplySnap ? ClampOrSnapTime(InTime) : ClampTime(InTime);
}

void FAnimationSequenceEditorState::SetTimelineRange(float InStartTime, float InEndTime)
{
    const float MaxTime = GetMaxTime();
    const float MinimumRange = GetMinimumVisibleRange();
    if (MaxTime <= 0.0f)
    {
        TimelineRangeStart = 0.0f;
        TimelineRangeEnd = std::max(1.0f, MinimumRange);
        PlaybackRangeStart = TimelineRangeStart;
        PlaybackRangeEnd = TimelineRangeEnd;
        return;
    }

    float StartTime = std::min(InStartTime, InEndTime);
    float EndTime = std::max(InStartTime, InEndTime);
    StartTime = std::clamp(StartTime, 0.0f, MaxTime);
    EndTime = std::clamp(EndTime, StartTime + MinimumRange, MaxTime);
    if (EndTime - StartTime < MinimumRange)
    {
        EndTime = std::min(MaxTime, StartTime + MinimumRange);
        StartTime = std::max(0.0f, EndTime - MinimumRange);
    }

    TimelineRangeStart = StartTime;
    TimelineRangeEnd = std::max(EndTime, TimelineRangeStart + MinimumRange);
    SetPlaybackRange(PlaybackRangeStart, PlaybackRangeEnd);
    SetVisibleRange(VisibleTimeStart, VisibleTimeEnd);
}

void FAnimationSequenceEditorState::SetPlaybackRange(float InStartTime, float InEndTime)
{
    const float MaxTime = GetMaxTime();
    const float MinimumRange = GetMinimumVisibleRange();
    const float RangeStart = std::clamp(TimelineRangeStart, 0.0f, MaxTime);
    const float RangeEnd =
        MaxTime > 0.0f
            ? std::clamp(TimelineRangeEnd, RangeStart + MinimumRange, MaxTime)
            : std::max(1.0f, MinimumRange);

    float StartTime = std::min(InStartTime, InEndTime);
    float EndTime = std::max(InStartTime, InEndTime);

    if (MaxTime <= 0.0f)
    {
        PlaybackRangeStart = 0.0f;
        PlaybackRangeEnd = std::max(1.0f, MinimumRange);
        return;
    }

    StartTime = std::clamp(StartTime, RangeStart, RangeEnd);
    EndTime = std::clamp(EndTime, StartTime + MinimumRange, RangeEnd);
    if (EndTime - StartTime < MinimumRange)
    {
        EndTime = std::min(RangeEnd, StartTime + MinimumRange);
        StartTime = std::max(RangeStart, EndTime - MinimumRange);
    }

    PlaybackRangeStart = StartTime;
    PlaybackRangeEnd = std::max(EndTime, PlaybackRangeStart + MinimumRange);
}

void FAnimationSequenceEditorState::SetVisibleRange(float InStartTime, float InEndTime)
{
    float StartTime = std::min(InStartTime, InEndTime);
    float EndTime = std::max(InStartTime, InEndTime);

    const float MaxTime = GetMaxTime();
    const float MinimumRange = GetMinimumVisibleRange();
    const float RangeStart = std::clamp(TimelineRangeStart, 0.0f, MaxTime);
    const float RangeEnd =
        MaxTime > 0.0f
            ? std::clamp(TimelineRangeEnd, RangeStart + MinimumRange, MaxTime)
            : std::max(1.0f, MinimumRange);
    const float MaximumRange = MaxTime > 0.0f ? std::max(RangeEnd - RangeStart, MinimumRange) : std::max(1.0f, MinimumRange);

    float Range = std::max(EndTime - StartTime, MinimumRange);
    Range = std::min(Range, MaximumRange);

    if (MaxTime <= 0.0f)
    {
        VisibleTimeStart = 0.0f;
        VisibleTimeEnd = Range;
        return;
    }

    StartTime = std::clamp(StartTime, RangeStart, RangeEnd);
    EndTime = StartTime + Range;
    if (EndTime > RangeEnd)
    {
        EndTime = RangeEnd;
        StartTime = std::max(RangeStart, EndTime - Range);
    }

    VisibleTimeStart = StartTime;
    VisibleTimeEnd = std::max(EndTime, VisibleTimeStart + MinimumRange);
}

void FAnimationSequenceEditorState::PanVisibleRange(float DeltaTime)
{
    SetVisibleRange(VisibleTimeStart + DeltaTime, VisibleTimeEnd + DeltaTime);
}

void FAnimationSequenceEditorState::ZoomVisibleRange(float AnchorTime, float ZoomFactor)
{
    const float CurrentRange = GetVisibleRange();
    const float MinimumRange = GetMinimumVisibleRange();
    const float MaximumRange = std::max(GetTimelineRangeEnd() - GetTimelineRangeStart(), MinimumRange);
    const float NewRange = std::clamp(CurrentRange * ZoomFactor, MinimumRange, MaximumRange);
    const float AnchorAlpha = CurrentRange > 0.0f
        ? std::clamp((AnchorTime - VisibleTimeStart) / CurrentRange, 0.0f, 1.0f)
        : 0.0f;
    const float NewStart = AnchorTime - NewRange * AnchorAlpha;
    SetVisibleRange(NewStart, NewStart + NewRange);
}

int32 FAnimationSequenceEditorState::ChooseMajorFrameStep(float TimelinePixelWidth) const
{
    if (TimelinePixelWidth <= 0.0f)
    {
        return 1;
    }

    const double FPS = GetDisplayFPS();
    if (FPS <= 0.0 || TotalFrames <= 1)
    {
        return 1;
    }

    const double VisibleFrameRange = std::max(1.0, static_cast<double>(GetVisibleRange()) * FPS);
    constexpr std::array<int32, 15> Candidates = { 1, 2, 5, 10, 15, 30, 60, 90, 120, 180, 240, 360, 480, 600, 900 };
    for (const int32 Candidate : Candidates)
    {
        const double PixelSpacing = (static_cast<double>(Candidate) / VisibleFrameRange) * TimelinePixelWidth;
        if (PixelSpacing >= 56.0)
        {
            return Candidate;
        }
    }

    return Candidates.back();
}

int32 FAnimationSequenceEditorState::ChooseMinorFrameStep(float TimelinePixelWidth) const
{
    const int32 MajorFrameStep = ChooseMajorFrameStep(TimelinePixelWidth);
    if (MajorFrameStep <= 1)
    {
        return 1;
    }

    if (MajorFrameStep % 2 == 0)
    {
        return std::max(1, MajorFrameStep / 2);
    }

    if (MajorFrameStep % 5 == 0)
    {
        return std::max(1, MajorFrameStep / 5);
    }

    return 1;
}

void FAnimationSequenceEditorState::SetHoveredSequencerRow(const FAnimationSequenceSequencerRowId& RowId)
{
    HoveredSequencerRowId = RowId;
}

void FAnimationSequenceEditorState::ClearHoveredSequencerRow()
{
    HoveredSequencerRowId = FAnimationSequenceSequencerRowId();
}

void FAnimationSequenceEditorState::SetFocusedSequencerRow(const FAnimationSequenceSequencerRowId& RowId)
{
    FocusedSequencerRowId = RowId;
}

void FAnimationSequenceEditorState::ClearFocusedSequencerRow()
{
    FocusedSequencerRowId = FAnimationSequenceSequencerRowId();
}

bool FAnimationSequenceEditorState::IsHoveredSequencerRow(const FAnimationSequenceSequencerRowId& RowId) const
{
    return HoveredSequencerRowId.IsValid() && HoveredSequencerRowId == RowId;
}

bool FAnimationSequenceEditorState::IsFocusedSequencerRow(const FAnimationSequenceSequencerRowId& RowId) const
{
    return FocusedSequencerRowId.IsValid() && FocusedSequencerRowId == RowId;
}
