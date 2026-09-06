#include "Editor/Animation/AnimationSequenceTimelineWidget.h"

#include "Editor/Animation/AnimationSequenceEditorState.h"
#include "Editor/Animation/AnimationSequencePreviewController.h"
#include "Editor/Animation/AnimationSequenceSequencerLayout.h"
#include "Editor/Animation/AnimationSequenceTimelineGeometry.h"

#include "ImGui/imgui.h"

#include <algorithm>
#include <cstdio>

namespace
{
    ImU32 WithAlpha(ImU32 Color, float AlphaScale)
    {
        const ImU32 Alpha = static_cast<ImU32>(std::clamp(AlphaScale, 0.0f, 1.0f) * 255.0f);
        return (Color & 0x00ffffffu) | (Alpha << 24);
    }

    struct FTimelineTickLayout
    {
        int32 MajorStep = 1;
        int32 MinorStep = 1;
        int32 EndFrame = 0;
        int32 FirstMinorFrame = 0;
    };

    struct FTimelineVisualColors
    {
        ImU32 BorderColor = 0;
        ImU32 PanelColor = 0;
        ImU32 RulerColor = 0;
        ImU32 GridColor = 0;
        ImU32 MinorGridColor = 0;
        ImU32 PlayheadColor = 0;
    };

    FTimelineTickLayout BuildTimelineTickLayout(
        const FAnimationSequenceEditorState& State,
        const FAnimationSequenceTimelineGeometry& Geometry)
    {
        // Resolve major/minor ruler density once per frame so grid drawing and
        // label placement use a single consistent tick layout.
        const int32 MajorStep = State.ChooseMajorFrameStep(Geometry.TimelineWidth);
        const int32 MinorStep = State.ChooseMinorFrameStep(Geometry.TimelineWidth);
        return
        {
            MajorStep,
            MinorStep,
            Geometry.GetEndFrame(State),
            Geometry.GetFirstMinorFrame(State, MinorStep)
        };
    }

    FTimelineVisualColors BuildTimelineVisualColors()
    {
        return
        {
            ImGui::GetColorU32(ImGuiCol_Border),
            IM_COL32(24, 26, 31, 255),
            IM_COL32(31, 35, 41, 255),
            WithAlpha(IM_COL32(153, 160, 171, 255), 0.34f),
            WithAlpha(IM_COL32(153, 160, 171, 255), 0.16f),
            IM_COL32(255, 106, 72, 255)
        };
    }

    void RenderTimelineBackground(
        ImDrawList* DrawList,
        const FAnimationSequenceTimelineGeometry& Geometry,
        const FTimelineVisualColors& Colors)
    {
        DrawList->AddRectFilled(Geometry.CanvasPos, Geometry.CanvasEnd, Colors.PanelColor);
        DrawList->AddRectFilled(
            ImVec2(Geometry.TimelineMinX, Geometry.RulerTop),
            ImVec2(Geometry.TimelineMaxX, Geometry.TrackTop),
            Colors.RulerColor,
            4.0f);
        DrawList->AddRect(Geometry.CanvasPos, Geometry.CanvasEnd, Colors.BorderColor);
        DrawList->AddLine(
            ImVec2(Geometry.TimelineMinX, Geometry.TrackTop),
            ImVec2(Geometry.TimelineMaxX, Geometry.TrackTop),
            Colors.BorderColor);
    }

    void RenderTimelineTicks(
        ImDrawList* DrawList,
        const FAnimationSequenceEditorState& State,
        const FAnimationSequenceTimelineGeometry& Geometry,
        const FTimelineTickLayout& TickLayout,
        const FTimelineVisualColors& Colors)
    {
        DrawList->PushClipRect(
            ImVec2(Geometry.TimelineMinX, Geometry.RulerTop),
            ImVec2(Geometry.TimelineMaxX, Geometry.CanvasEnd.y - FAnimationSequenceTimelineGeometry::VerticalPadding),
            true);

        for (int32 Frame = TickLayout.FirstMinorFrame; Frame <= TickLayout.EndFrame + TickLayout.MinorStep; Frame += std::max(TickLayout.MinorStep, 1))
        {
            const float Time = State.FrameToTime(Frame);
            const float X = Geometry.TimeToX(State, Time);
            if (X < Geometry.TimelineMinX - 1.0f || X > Geometry.TimelineMaxX + 1.0f)
            {
                continue;
            }

            const bool bMajorTick = TickLayout.MajorStep > 0 && (Frame % TickLayout.MajorStep) == 0;
            DrawList->AddLine(
                ImVec2(X, bMajorTick ? Geometry.RulerTop : Geometry.TrackTop - 10.0f),
                ImVec2(X, Geometry.CanvasEnd.y - FAnimationSequenceTimelineGeometry::VerticalPadding),
                bMajorTick ? Colors.GridColor : Colors.MinorGridColor);

            if (bMajorTick)
            {
                char Label[32];
                snprintf(Label, sizeof(Label), "%d", Frame);
                DrawList->AddText(
                    ImVec2(X + 4.0f, Geometry.RulerTop + 4.0f),
                    ImGui::GetColorU32(ImGuiCol_TextDisabled),
                    Label);
            }
        }

        DrawList->PopClipRect();
    }

    void RenderTimelinePlayhead(
        ImDrawList* DrawList,
        const FAnimationSequenceEditorState& State,
        const FAnimationSequenceTimelineGeometry& Geometry,
        ImU32 PlayheadColor)
    {
        const float CurrentX = Geometry.GetClampedPlayheadX(State);
        DrawList->AddLine(
            ImVec2(CurrentX, Geometry.RulerTop),
            ImVec2(CurrentX, Geometry.CanvasEnd.y - FAnimationSequenceTimelineGeometry::VerticalPadding),
            PlayheadColor,
            2.0f);
        DrawList->AddTriangleFilled(
            ImVec2(CurrentX, Geometry.RulerTop + 1.0f),
            ImVec2(CurrentX - 6.0f, Geometry.RulerTop + 11.0f),
            ImVec2(CurrentX + 6.0f, Geometry.RulerTop + 11.0f),
            PlayheadColor);
    }

    bool IsMouseInTimelineRuler(const FAnimationSequenceTimelineGeometry& Geometry, const ImVec2& MousePos)
    {
        return
            MousePos.x >= Geometry.TimelineMinX &&
            MousePos.x <= Geometry.TimelineMaxX &&
            MousePos.y >= Geometry.RulerTop &&
            MousePos.y <= Geometry.TrackTop;
    }

    void HandleTimelinePanInteraction(
        FAnimationSequenceEditorState& State,
        const FAnimationSequenceTimelineGeometry& Geometry,
        bool bCanvasHovered)
    {
        if (!(bCanvasHovered && ImGui::IsMouseDragging(ImGuiMouseButton_Middle)))
        {
            return;
        }

        const float DeltaTime = -(ImGui::GetIO().MouseDelta.x / Geometry.TimelineWidth) * State.GetVisibleRange();
        State.PanVisibleRange(DeltaTime);
    }

    void HandleTimelineScrubInteraction(
        FAnimationSequenceEditorState& State,
        FAnimationSequencePreviewController* PreviewController,
        const FAnimationSequenceTimelineGeometry& Geometry,
        bool bCanvasActive,
        bool bMouseInRuler)
    {
        // Restrict left-drag scrubbing to the ruler. Notify drags own left mouse
        // input inside the lane, and middle-drag remains reserved for panning.
        if (!(PreviewController && bCanvasActive && bMouseInRuler && ImGui::IsMouseDown(ImGuiMouseButton_Left) && !State.bDraggingNotify))
        {
            return;
        }

        const ImVec2 MousePos = ImGui::GetIO().MousePos;
        const float NewTime = State.ClampOrSnapTime(
            Geometry.XToTime(
                State,
                Geometry.ClampX(MousePos.x)));
        PreviewController->SetCurrentTime(NewTime);
        State.SetCurrentTime(PreviewController->GetCurrentTime(), false);
    }
}

float FAnimationSequenceTimelineWidget::GetRulerHeight() const
{
    return FAnimationSequenceSequencerLayout::RulerHeight;
}

void FAnimationSequenceTimelineWidget::RenderCanvas(
    FAnimationSequenceEditorState& State,
    FAnimationSequencePreviewController* PreviewController,
    const FAnimationSequenceTimelineGeometry& Geometry,
    bool bCanvasHovered,
    bool bCanvasActive) const
{
    ImDrawList* DrawList = ImGui::GetWindowDrawList();
    const FTimelineVisualColors Colors = BuildTimelineVisualColors();
    RenderTimelineBackground(DrawList, Geometry, Colors);

    if (!State.HasTimelineData())
    {
        DrawList->AddText(
            ImVec2(Geometry.TimelineMinX, Geometry.TrackTop + 10.0f),
            ImGui::GetColorU32(ImGuiCol_TextDisabled),
            "Timeline data is unavailable.");
        return;
    }

    const FTimelineTickLayout TickLayout = BuildTimelineTickLayout(State, Geometry);
    RenderTimelineTicks(DrawList, State, Geometry, TickLayout, Colors);
    RenderTimelinePlayhead(DrawList, State, Geometry, Colors.PlayheadColor);

    const ImVec2 MousePos = ImGui::GetIO().MousePos;
    const bool bMouseInRuler = IsMouseInTimelineRuler(Geometry, MousePos);

    HandleTimelinePanInteraction(State, Geometry, bCanvasHovered);
    HandleTimelineScrubInteraction(State, PreviewController, Geometry, bCanvasActive, bMouseInRuler);
}
