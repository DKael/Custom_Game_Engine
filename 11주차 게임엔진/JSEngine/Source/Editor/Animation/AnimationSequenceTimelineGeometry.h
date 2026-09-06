#pragma once

#include "Core/CoreMinimal.h"

#include "ImGui/imgui.h"

class FAnimationSequenceEditorState;

struct FAnimationSequenceTimelineGeometry
{
    static constexpr float HorizontalPadding = 14.0f;
    static constexpr float VerticalPadding = 8.0f;

    ImVec2 CanvasPos = {};
    ImVec2 CanvasSize = {};
    ImVec2 CanvasEnd = {};
    float TimelineMinX = 0.0f;
    float TimelineMaxX = 0.0f;
    float TimelineWidth = 1.0f;
    float LaneTop = 0.0f;
    float LaneBottom = 0.0f;
    float RulerTop = 0.0f;
    float TrackTop = 0.0f;

    static FAnimationSequenceTimelineGeometry BuildTimelineGeometry(
        const ImVec2& CanvasPos,
        const ImVec2& CanvasSize,
        float RulerHeight);
    static FAnimationSequenceTimelineGeometry BuildSequencerCanvasGeometry(
        const ImVec2& CanvasPos,
        const ImVec2& CanvasSize,
        float RulerHeight);
    static FAnimationSequenceTimelineGeometry BuildLaneGeometry(
        const ImVec2& CanvasPos,
        const ImVec2& CanvasSize);

    float TimeToX(const FAnimationSequenceEditorState& State, float Time) const;
    float XToTime(const FAnimationSequenceEditorState& State, float X) const;
    float GetClampedPlayheadX(const FAnimationSequenceEditorState& State) const;
    float ClampX(float X) const;

    int32 GetStartFrame(const FAnimationSequenceEditorState& State) const;
    int32 GetEndFrame(const FAnimationSequenceEditorState& State) const;
    int32 GetFirstMinorFrame(const FAnimationSequenceEditorState& State, int32 MinorStep) const;
};
