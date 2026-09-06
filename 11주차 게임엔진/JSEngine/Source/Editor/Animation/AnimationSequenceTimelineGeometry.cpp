#include "Editor/Animation/AnimationSequenceTimelineGeometry.h"

#include "Editor/Animation/AnimationSequenceEditorState.h"

#include <algorithm>
#include <cmath>

FAnimationSequenceTimelineGeometry FAnimationSequenceTimelineGeometry::BuildTimelineGeometry(
    const ImVec2& InCanvasPos,
    const ImVec2& InCanvasSize,
    float RulerHeight)
{
    FAnimationSequenceTimelineGeometry Geometry;
    Geometry.CanvasPos = InCanvasPos;
    Geometry.CanvasSize = InCanvasSize;
    Geometry.CanvasEnd = ImVec2(InCanvasPos.x + InCanvasSize.x, InCanvasPos.y + InCanvasSize.y);
    Geometry.TimelineMinX = InCanvasPos.x + HorizontalPadding;
    Geometry.TimelineMaxX = Geometry.CanvasEnd.x - HorizontalPadding;
    Geometry.TimelineWidth = std::max(1.0f, Geometry.TimelineMaxX - Geometry.TimelineMinX);
    Geometry.RulerTop = InCanvasPos.y + VerticalPadding;
    Geometry.TrackTop = Geometry.RulerTop + RulerHeight;
    Geometry.LaneTop = Geometry.RulerTop;
    Geometry.LaneBottom = Geometry.CanvasEnd.y - VerticalPadding;
    return Geometry;
}

FAnimationSequenceTimelineGeometry FAnimationSequenceTimelineGeometry::BuildSequencerCanvasGeometry(
    const ImVec2& InCanvasPos,
    const ImVec2& InCanvasSize,
    float RulerHeight)
{
    return BuildTimelineGeometry(InCanvasPos, InCanvasSize, RulerHeight);
}

FAnimationSequenceTimelineGeometry FAnimationSequenceTimelineGeometry::BuildLaneGeometry(
    const ImVec2& InCanvasPos,
    const ImVec2& InCanvasSize)
{
    FAnimationSequenceTimelineGeometry Geometry;
    Geometry.CanvasPos = InCanvasPos;
    Geometry.CanvasSize = InCanvasSize;
    Geometry.CanvasEnd = ImVec2(InCanvasPos.x + InCanvasSize.x, InCanvasPos.y + InCanvasSize.y);
    Geometry.TimelineMinX = InCanvasPos.x + HorizontalPadding;
    Geometry.TimelineMaxX = Geometry.CanvasEnd.x - HorizontalPadding;
    Geometry.TimelineWidth = std::max(1.0f, Geometry.TimelineMaxX - Geometry.TimelineMinX);
    Geometry.LaneTop = InCanvasPos.y + VerticalPadding;
    Geometry.LaneBottom = Geometry.CanvasEnd.y - VerticalPadding;
    Geometry.RulerTop = Geometry.LaneTop;
    Geometry.TrackTop = Geometry.LaneTop;
    return Geometry;
}

float FAnimationSequenceTimelineGeometry::TimeToX(const FAnimationSequenceEditorState& State, float Time) const
{
    return TimelineMinX + ((Time - State.VisibleTimeStart) / State.GetVisibleRange()) * TimelineWidth;
}

float FAnimationSequenceTimelineGeometry::XToTime(const FAnimationSequenceEditorState& State, float X) const
{
    const float Alpha = (X - TimelineMinX) / TimelineWidth;
    return State.VisibleTimeStart + Alpha * State.GetVisibleRange();
}

float FAnimationSequenceTimelineGeometry::GetClampedPlayheadX(const FAnimationSequenceEditorState& State) const
{
    return std::clamp(TimeToX(State, State.CurrentTime), TimelineMinX, TimelineMaxX);
}

float FAnimationSequenceTimelineGeometry::ClampX(float X) const
{
    return std::clamp(X, TimelineMinX, TimelineMaxX);
}

int32 FAnimationSequenceTimelineGeometry::GetStartFrame(const FAnimationSequenceEditorState& State) const
{
    return std::max(0, static_cast<int32>(std::floor(State.VisibleTimeStart * State.GetDisplayFPS())));
}

int32 FAnimationSequenceTimelineGeometry::GetEndFrame(const FAnimationSequenceEditorState& State) const
{
    return std::max(GetStartFrame(State), static_cast<int32>(std::ceil(State.VisibleTimeEnd * State.GetDisplayFPS())));
}

int32 FAnimationSequenceTimelineGeometry::GetFirstMinorFrame(
    const FAnimationSequenceEditorState& State,
    int32 MinorStep) const
{
    const int32 StartFrame = GetStartFrame(State);
    return MinorStep > 0 ? (StartFrame / MinorStep) * MinorStep : StartFrame;
}
