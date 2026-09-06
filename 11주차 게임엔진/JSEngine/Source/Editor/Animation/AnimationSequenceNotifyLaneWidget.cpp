#include "Editor/Animation/AnimationSequenceNotifyLaneWidget.h"

#include "Animation/AnimData/AnimNotifyTypes.h"
#include "Editor/Animation/AnimationSequenceEditorDocument.h"
#include "Editor/Animation/AnimationSequenceEditorState.h"
#include "Editor/Animation/AnimationSequenceSequencerVisibleLayout.h"
#include "Editor/Animation/AnimationSequenceTimelineGeometry.h"

#include "ImGui/imgui.h"

#include <algorithm>

namespace
{
    constexpr float NotifyStateResizeHandleWidth = 8.0f;

    ImU32 ToImGuiColor(const FColor& Color, float AlphaScale)
    {
        const float Alpha = std::clamp(Color.A * AlphaScale, 0.0f, 1.0f);
        return ImGui::GetColorU32(ImVec4(Color.R, Color.G, Color.B, Alpha));
    }

    enum class EPendingNotifyContextAction : uint8
    {
        None,
        Duplicate,
        Copy,
        Delete,
        MoveToTrack
    };

    struct FNotifyMarkerGeometry
    {
        float MarkerStartX = 0.0f;
        float MarkerEndX = 0.0f;
        float MarkerWidth = 0.0f;
        ImVec2 MarkerMin = {};
        ImVec2 MarkerMax = {};
        float ResizeHandleMinX = 0.0f;
    };

    struct FNotifyMarkerHitState
    {
        bool bSelected = false;
        bool bHovered = false;
        bool bCanResizeEnd = false;
        bool bHoveredResizeHandle = false;
    };

    FNotifyMarkerGeometry BuildNotifyMarkerGeometry(
        const FAnimationSequenceEditorState& State,
        const FAnimationSequenceTimelineGeometry& Geometry,
        const FAnimNotifyEvent& NotifyEvent,
        float RowTop,
        float RowBottom)
    {
        FNotifyMarkerGeometry MarkerGeometry;
        MarkerGeometry.MarkerStartX = Geometry.TimeToX(State, NotifyEvent.Time);
        const float MarkerEndTime = NotifyEvent.Time + std::max(NotifyEvent.Duration, 0.0f);
        MarkerGeometry.MarkerEndX = Geometry.TimeToX(State, MarkerEndTime);
        MarkerGeometry.MarkerWidth = std::max(MarkerGeometry.MarkerEndX - MarkerGeometry.MarkerStartX, 12.0f);
        MarkerGeometry.MarkerMin = ImVec2(
            std::clamp(MarkerGeometry.MarkerStartX, Geometry.TimelineMinX, Geometry.TimelineMaxX - 12.0f),
            RowTop + 3.0f);
        MarkerGeometry.MarkerMax = ImVec2(
            std::clamp(MarkerGeometry.MarkerMin.x + MarkerGeometry.MarkerWidth, MarkerGeometry.MarkerMin.x + 12.0f, Geometry.TimelineMaxX),
            RowBottom - 3.0f);
        MarkerGeometry.ResizeHandleMinX = std::max(MarkerGeometry.MarkerMin.x, MarkerGeometry.MarkerMax.x - NotifyStateResizeHandleWidth);
        return MarkerGeometry;
    }

    FNotifyMarkerHitState BuildNotifyMarkerHitState(
        const FAnimationSequenceEditorState& State,
        const FAnimNotifyEvent& NotifyEvent,
        int32 TrackIndex,
        int32 EventIndex,
        const FNotifyMarkerGeometry& MarkerGeometry)
    {
        FNotifyMarkerHitState HitState;
        HitState.bSelected =
            State.SelectedNotifyTrackIndex == TrackIndex &&
            State.SelectedNotifyEventIndex == EventIndex;
        HitState.bHovered = ImGui::IsItemHovered();
        HitState.bCanResizeEnd = NotifyEvent.IsState();
        HitState.bHoveredResizeHandle =
            HitState.bCanResizeEnd &&
            HitState.bHovered &&
            ImGui::GetIO().MousePos.x >= MarkerGeometry.ResizeHandleMinX &&
            ImGui::GetIO().MousePos.x <= MarkerGeometry.MarkerMax.x;
        return HitState;
    }

    void ApplyNotifyMarkerHoverState(
        FAnimationSequenceEditorState& State,
        const FAnimationSequenceSequencerVisibleRow& Row,
        int32 EventIndex,
        const FNotifyMarkerHitState& HitState,
        bool& bMouseOverMarker)
    {
        if (!HitState.bHovered)
        {
            return;
        }

        bMouseOverMarker = true;
        State.HoveredNotifyTrackIndex = Row.SourceIndex;
        State.HoveredNotifyEventIndex = EventIndex;
        State.SetHoveredSequencerRow(Row.Id);
        if (HitState.bHoveredResizeHandle)
        {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        }
    }

    void HandleNotifyMarkerLeftClick(
        FAnimationSequenceEditorState& State,
        FAnimationSequenceEditorDocument* Document,
        const FAnimationSequenceSequencerVisibleRow& Row,
        int32 EventIndex,
        const FAnimNotifyEvent& NotifyEvent,
        const FNotifyMarkerHitState& HitState,
        const FAnimationSequenceTimelineGeometry& Geometry)
    {
        if (!(ImGui::IsItemClicked(ImGuiMouseButton_Left) && Document))
        {
            return;
        }

        State.SetFocusedSequencerRow(Row.Id);
        State.SelectedCurveIndex = -1;
        State.HoveredCurveIndex = -1;
        // Preserve the cursor-to-notify offset for move drags so the block stays
        // under the mouse instead of snapping its start time directly to the
        // cursor on the next drag frame.
        State.ActiveNotifyDragMode =
            HitState.bHoveredResizeHandle ? EAnimNotifyDragMode::ResizeEnd : EAnimNotifyDragMode::Move;
        State.DraggedNotifyGrabOffsetTime = State.ActiveNotifyDragMode == EAnimNotifyDragMode::Move
            ? (Geometry.XToTime(State, Geometry.ClampX(ImGui::GetIO().MousePos.x)) - NotifyEvent.Time)
            : 0.0f;
        Document->SelectNotify(Row.SourceIndex, EventIndex);
    }

    void HandleNotifyMarkerDragStart(
        FAnimationSequenceEditorState& State,
        FAnimationSequenceEditorDocument* Document,
        const FAnimationSequenceSequencerVisibleRow& Row,
        int32 EventIndex)
    {
        if (!(ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)))
        {
            return;
        }

        State.bDraggingNotify = true;
        State.DraggedNotifyTrackIndex = Row.SourceIndex;
        State.DraggedNotifyEventIndex = EventIndex;
        State.SelectedCurveIndex = -1;
        if (State.ActiveNotifyDragMode == EAnimNotifyDragMode::None)
        {
            State.ActiveNotifyDragMode = EAnimNotifyDragMode::Move;
        }
        if (Document)
        {
            Document->SelectNotify(Row.SourceIndex, EventIndex);
        }
    }

    void HandleNotifyMarkerRightClickSelection(
        FAnimationSequenceEditorState& State,
        FAnimationSequenceEditorDocument* Document,
        const FAnimationSequenceSequencerVisibleRow& Row,
        int32 EventIndex)
    {
        if (!(ImGui::IsItemClicked(ImGuiMouseButton_Right) && Document))
        {
            return;
        }

        State.SetFocusedSequencerRow(Row.Id);
        Document->SelectNotify(Row.SourceIndex, EventIndex);
    }

    void HandleNotifyMarkerContextMenu(
        FAnimationSequenceEditorDocument* Document,
        const FAnimationSequenceSequencerVisibleRow& Row,
        EPendingNotifyContextAction& PendingContextAction,
        int32& PendingMoveTargetTrackIndex)
    {
        if (!(Document && ImGui::BeginPopupContextItem("##AnimNotifyEventContext")))
        {
            return;
        }

        if (ImGui::MenuItem("Duplicate"))
        {
            PendingContextAction = EPendingNotifyContextAction::Duplicate;
        }
        if (ImGui::MenuItem("Copy"))
        {
            PendingContextAction = EPendingNotifyContextAction::Copy;
        }
        if (ImGui::MenuItem("Delete"))
        {
            PendingContextAction = EPendingNotifyContextAction::Delete;
        }

        if (ImGui::BeginMenu("Move To Track"))
        {
            const int32 TrackCount = Document->GetNotifyTrackCount();
            for (int32 TrackIndex = 0; TrackIndex < TrackCount; ++TrackIndex)
            {
                const FAnimNotifyTrack* TargetTrack = Document->GetNotifyTrack(TrackIndex);
                const FString TrackLabel =
                    (TargetTrack && TargetTrack->TrackName.IsValid())
                        ? TargetTrack->TrackName.ToString()
                        : ("Track " + std::to_string(TrackIndex + 1));
                const bool bIsCurrentTrack = TrackIndex == Row.SourceIndex;
                if (ImGui::MenuItem(TrackLabel.c_str(), nullptr, false, !bIsCurrentTrack))
                {
                    PendingContextAction = EPendingNotifyContextAction::MoveToTrack;
                    PendingMoveTargetTrackIndex = TrackIndex;
                }
            }
            ImGui::EndMenu();
        }

        ImGui::EndPopup();
    }

    void RenderNotifyMarker(
        ImDrawList* DrawList,
        const FAnimNotifyEvent& NotifyEvent,
        const FNotifyMarkerGeometry& MarkerGeometry,
        const FNotifyMarkerHitState& HitState)
    {
        const ImU32 MarkerColor = HitState.bSelected
            ? ToImGuiColor(NotifyEvent.Color, 1.0f)
            : ToImGuiColor(NotifyEvent.Color, HitState.bHovered ? 0.95f : 0.78f);
        DrawList->AddRectFilled(MarkerGeometry.MarkerMin, MarkerGeometry.MarkerMax, MarkerColor, 4.0f);
        DrawList->AddRect(
            MarkerGeometry.MarkerMin,
            MarkerGeometry.MarkerMax,
            HitState.bSelected ? IM_COL32(255, 255, 255, 255) : IM_COL32(0, 0, 0, 96),
            4.0f);
        DrawList->AddText(
            ImVec2(MarkerGeometry.MarkerMin.x + 6.0f, MarkerGeometry.MarkerMin.y + 2.0f),
            IM_COL32(14, 18, 22, 255),
            NotifyEvent.Name.IsValid() ? NotifyEvent.Name.ToString().c_str() : "Notify");
    }

    void ExecutePendingNotifyContextAction(
        FAnimationSequenceEditorState& State,
        FAnimationSequenceEditorDocument* Document,
        EPendingNotifyContextAction PendingContextAction,
        int32 PendingMoveTargetTrackIndex)
    {
        if (!Document)
        {
            return;
        }

        // Defer context actions until after row iteration so we never mutate the
        // current track/event arrays while the lane is still traversing them.
        switch (PendingContextAction)
        {
        case EPendingNotifyContextAction::Duplicate:
            Document->DuplicateSelectedNotify();
            break;
        case EPendingNotifyContextAction::Copy:
            Document->CopySelectedNotify();
            break;
        case EPendingNotifyContextAction::Delete:
            Document->DeleteSelectedNotify();
            break;
        case EPendingNotifyContextAction::MoveToTrack:
            if (Document->MoveSelectedNotifyToTrack(PendingMoveTargetTrackIndex))
            {
                State.SetFocusedSequencerRow(
                    MakeAnimationSequenceSequencerRowId(
                        EAnimationSequenceSequencerVisibleRowType::NotifyTrack,
                        PendingMoveTargetTrackIndex));
            }
            break;
        case EPendingNotifyContextAction::None:
        default:
            break;
        }
    }

    void HandleActiveNotifyDrag(
        FAnimationSequenceEditorState& State,
        FAnimationSequenceEditorDocument* Document,
        const FAnimationSequenceTimelineGeometry& Geometry)
    {
        if (!(State.bDraggingNotify && ImGui::IsMouseDown(ImGuiMouseButton_Left) && Document && Document->GetSelectedNotify()))
        {
            return;
        }

        const float MouseTime = Geometry.XToTime(State, Geometry.ClampX(ImGui::GetIO().MousePos.x));
        // Route drag updates through the selected notify so move/resize paths
        // keep using the existing document mutation semantics and snap rules.
        if (State.ActiveNotifyDragMode == EAnimNotifyDragMode::ResizeEnd)
        {
            Document->SetSelectedNotifyEndTime(MouseTime, State.bSnapToFrames);
        }
        else
        {
            const float NewTime = MouseTime - State.DraggedNotifyGrabOffsetTime;
            Document->SetSelectedNotifyTime(NewTime, State.bSnapToFrames);
        }
    }

    bool IsMouseInsideNotifyTrackRow(
        const ImVec2& MousePos,
        const FAnimationSequenceSequencerVisibleRow& Row,
        const FAnimationSequenceTimelineGeometry& Geometry,
        float TimelineRowOriginY)
    {
        const float RowTop = TimelineRowOriginY + Row.OffsetY;
        const float RowBottom = RowTop + Row.Height;
        return
            MousePos.y >= RowTop &&
            MousePos.y <= RowBottom &&
            MousePos.x >= Geometry.TimelineMinX &&
            MousePos.x <= Geometry.TimelineMaxX;
    }

    void HandleNotifyLaneBackgroundContextMenu(
        FAnimationSequenceEditorState& State,
        FAnimationSequenceEditorDocument* Document,
        const FAnimationSequenceTimelineGeometry& Geometry,
        const FAnimationSequenceSequencerVisibleLayout& VisibleLayout,
        float TimelineRowOriginY,
        bool bMouseOverMarker,
        int32& PendingLaneContextTrackIndex,
        float& PendingLaneContextTime)
    {
        if (!(Document && ImGui::IsMouseClicked(ImGuiMouseButton_Right) && !bMouseOverMarker))
        {
            return;
        }

        const ImVec2 MousePos = ImGui::GetIO().MousePos;
        for (const FAnimationSequenceSequencerVisibleRow& Row : VisibleLayout.Rows)
        {
            if (Row.Type != EAnimationSequenceSequencerVisibleRowType::NotifyTrack)
            {
                continue;
            }

            if (IsMouseInsideNotifyTrackRow(MousePos, Row, Geometry, TimelineRowOriginY))
            {
                PendingLaneContextTrackIndex = Row.SourceIndex;
                PendingLaneContextTime = Geometry.XToTime(State, Geometry.ClampX(MousePos.x));
                State.SetFocusedSequencerRow(Row.Id);
                ImGui::OpenPopup("##AnimNotifyLaneContext");
                break;
            }
        }
    }

    void HandleNotifyLaneDoubleClickAdd(
        FAnimationSequenceEditorState& State,
        FAnimationSequenceEditorDocument* Document,
        const FAnimationSequenceTimelineGeometry& Geometry,
        const FAnimationSequenceSequencerVisibleLayout& VisibleLayout,
        float TimelineRowOriginY,
        bool bMouseOverMarker)
    {
        if (!(Document && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && !bMouseOverMarker))
        {
            return;
        }

        const ImVec2 MousePos = ImGui::GetIO().MousePos;
        for (const FAnimationSequenceSequencerVisibleRow& Row : VisibleLayout.Rows)
        {
            if (Row.Type != EAnimationSequenceSequencerVisibleRowType::NotifyTrack)
            {
                continue;
            }

            const float RowTop = TimelineRowOriginY + Row.OffsetY;
            const float RowBottom = RowTop + Row.Height;
            if (MousePos.y >= RowTop && MousePos.y <= RowBottom)
            {
                const float NewTime = Geometry.XToTime(State, Geometry.ClampX(MousePos.x));
                State.SetFocusedSequencerRow(Row.Id);
                State.SelectedCurveIndex = -1;
                Document->AddNotifyAtTime(Row.SourceIndex, NewTime);
                break;
            }
        }
    }
}

void FAnimationSequenceNotifyLaneWidget::RenderRows(
    FAnimationSequenceEditorState& State,
    FAnimationSequenceEditorDocument* Document,
    const FAnimationSequenceTimelineGeometry& Geometry,
    const FAnimationSequenceSequencerVisibleLayout& VisibleLayout,
    float TimelineRowOriginY)
{
    static int32 PendingLaneContextTrackIndex = -1;
    static float PendingLaneContextTime = 0.0f;
    EPendingNotifyContextAction PendingContextAction = EPendingNotifyContextAction::None;
    int32 PendingMoveTargetTrackIndex = -1;

    State.HoveredNotifyTrackIndex = -1;
    State.HoveredNotifyEventIndex = -1;
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        State.bDraggingNotify = false;
        State.ActiveNotifyDragMode = EAnimNotifyDragMode::None;
        State.DraggedNotifyTrackIndex = -1;
        State.DraggedNotifyEventIndex = -1;
        State.DraggedNotifyGrabOffsetTime = 0.0f;
    }

    ImDrawList* DrawList = ImGui::GetWindowDrawList();
    bool bMouseOverMarker = false;

    for (const FAnimationSequenceSequencerVisibleRow& Row : VisibleLayout.Rows)
    {
        if (Row.Type != EAnimationSequenceSequencerVisibleRowType::NotifyTrack)
        {
            continue;
        }

        const float RowTop = TimelineRowOriginY + Row.OffsetY;
        const float RowBottom = RowTop + Row.Height;
        const FAnimNotifyTrack* Track = Document ? Document->GetNotifyTrack(Row.SourceIndex) : nullptr;
        if (!Track)
        {
            continue;
        }

        for (int32 EventIndex = 0; EventIndex < static_cast<int32>(Track->Events.size()); ++EventIndex)
        {
            const FAnimNotifyEvent& NotifyEvent = Track->Events[EventIndex];
            const FNotifyMarkerGeometry MarkerGeometry =
                BuildNotifyMarkerGeometry(State, Geometry, NotifyEvent, RowTop, RowBottom);

            ImGui::SetCursorScreenPos(MarkerGeometry.MarkerMin);
            ImGui::PushID(Row.SourceIndex * 1024 + EventIndex);
            ImGui::InvisibleButton(
                "##AnimNotifyEvent",
                ImVec2(
                    std::max(MarkerGeometry.MarkerMax.x - MarkerGeometry.MarkerMin.x, 1.0f),
                    std::max(MarkerGeometry.MarkerMax.y - MarkerGeometry.MarkerMin.y, 1.0f)));

            const FNotifyMarkerHitState HitState =
                BuildNotifyMarkerHitState(State, NotifyEvent, Row.SourceIndex, EventIndex, MarkerGeometry);
            ApplyNotifyMarkerHoverState(State, Row, EventIndex, HitState, bMouseOverMarker);
            HandleNotifyMarkerLeftClick(State, Document, Row, EventIndex, NotifyEvent, HitState, Geometry);
            HandleNotifyMarkerDragStart(State, Document, Row, EventIndex);
            HandleNotifyMarkerRightClickSelection(State, Document, Row, EventIndex);
            HandleNotifyMarkerContextMenu(Document, Row, PendingContextAction, PendingMoveTargetTrackIndex);
            RenderNotifyMarker(DrawList, NotifyEvent, MarkerGeometry, HitState);
            ImGui::PopID();
        }
    }

    ExecutePendingNotifyContextAction(State, Document, PendingContextAction, PendingMoveTargetTrackIndex);
    HandleActiveNotifyDrag(State, Document, Geometry);
    HandleNotifyLaneBackgroundContextMenu(
        State,
        Document,
        Geometry,
        VisibleLayout,
        TimelineRowOriginY,
        bMouseOverMarker,
        PendingLaneContextTrackIndex,
        PendingLaneContextTime);

    if (Document && ImGui::BeginPopup("##AnimNotifyLaneContext"))
    {
        if (ImGui::MenuItem("Add Notify Here", nullptr, false, PendingLaneContextTrackIndex >= 0))
        {
            Document->AddNotifyAtTime(PendingLaneContextTrackIndex, PendingLaneContextTime);
        }

        if (ImGui::MenuItem("Paste Notify Here", nullptr, false, PendingLaneContextTrackIndex >= 0 && Document->CanPasteNotify()))
        {
            Document->PasteNotifyToTrackAtTime(PendingLaneContextTrackIndex, PendingLaneContextTime);
        }

        ImGui::EndPopup();
    }

    HandleNotifyLaneDoubleClickAdd(State, Document, Geometry, VisibleLayout, TimelineRowOriginY, bMouseOverMarker);
}
