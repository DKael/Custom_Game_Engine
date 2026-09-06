#include "Editor/Animation/AnimationSequenceTrackOutlinerWidget.h"

#include "Editor/Animation/AnimationSequenceCurveFilter.h"
#include "Editor/Animation/AnimationSequenceEditorDocument.h"
#include "Editor/Animation/AnimationSequenceEditorState.h"
#include "Editor/Animation/AnimationSequenceTimelineGeometry.h"

#include "ImGui/imgui.h"

#include <algorithm>
#include <cstring>
#include <string>

namespace
{
    struct FRowInteraction
    {
        bool bHovered = false;
        bool bClicked = false;
    };

    ImU32 ToPanelColor(float Intensity, float Alpha = 1.0f)
    {
        const int32 Value = static_cast<int32>(std::clamp(Intensity, 0.0f, 1.0f) * 255.0f);
        const int32 AlphaValue = static_cast<int32>(std::clamp(Alpha, 0.0f, 1.0f) * 255.0f);
        return IM_COL32(Value, Value, Value, AlphaValue);
    }

    FRowInteraction DrawSectionRow(
        const char* Id,
        const FString& Label,
        bool bCanExpand,
        bool& bExpanded,
        bool bFocused,
        const ImVec2& Min,
        const ImVec2& Max)
    {
        ImGui::SetCursorScreenPos(Min);
        ImGui::PushID(Id);
        ImGui::InvisibleButton("##SectionRow", ImVec2(Max.x - Min.x, Max.y - Min.y));
        FRowInteraction Interaction;
        Interaction.bHovered = ImGui::IsItemHovered();
        Interaction.bClicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
        if (Interaction.bClicked && bCanExpand)
        {
            bExpanded = !bExpanded;
        }

        ImDrawList* DrawList = ImGui::GetWindowDrawList();
        const float FillIntensity = bFocused ? 0.24f : (Interaction.bHovered ? 0.20f : 0.15f);
        DrawList->AddRectFilled(Min, Max, ToPanelColor(FillIntensity), 4.0f);
        DrawList->AddRect(Min, Max, ToPanelColor(0.30f), 4.0f);

        float LabelX = Min.x + 10.0f;
        if (bCanExpand)
        {
            const float ArrowCenterY = 0.5f * (Min.y + Max.y);
            const float ArrowX = Min.x + 10.0f;
            if (bExpanded)
            {
                DrawList->AddTriangleFilled(
                    ImVec2(ArrowX - 4.0f, ArrowCenterY - 2.0f),
                    ImVec2(ArrowX + 4.0f, ArrowCenterY - 2.0f),
                    ImVec2(ArrowX, ArrowCenterY + 3.0f),
                    IM_COL32(220, 225, 232, 255));
            }
            else
            {
                DrawList->AddTriangleFilled(
                    ImVec2(ArrowX - 2.0f, ArrowCenterY - 4.0f),
                    ImVec2(ArrowX - 2.0f, ArrowCenterY + 4.0f),
                    ImVec2(ArrowX + 3.0f, ArrowCenterY),
                    IM_COL32(220, 225, 232, 255));
            }

            LabelX = Min.x + 20.0f;
        }

        DrawList->AddText(ImVec2(LabelX, Min.y + 4.0f), IM_COL32(230, 234, 242, 255), Label.c_str());
        ImGui::PopID();
        return Interaction;
    }

    FRowInteraction DrawSelectableRow(
        int32 UniqueId,
        const FString& Label,
        const FString& RightLabel,
        bool bSelected,
        bool bFocused,
        const ImVec2& Min,
        const ImVec2& Max)
    {
        ImGui::SetCursorScreenPos(Min);
        ImGui::PushID(UniqueId);
        ImGui::InvisibleButton("##Row", ImVec2(Max.x - Min.x, Max.y - Min.y));
        FRowInteraction Interaction;
        Interaction.bHovered = ImGui::IsItemHovered();
        Interaction.bClicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);

        ImDrawList* DrawList = ImGui::GetWindowDrawList();
        const ImU32 FillColor = bSelected
            ? IM_COL32(56, 87, 121, 220)
            : (bFocused
                ? IM_COL32(44, 58, 79, 220)
                : (Interaction.bHovered ? IM_COL32(38, 43, 52, 220) : IM_COL32(28, 31, 38, 180)));
        DrawList->AddRectFilled(Min, Max, FillColor, 4.0f);
        DrawList->AddRect(Min, Max, IM_COL32(255, 255, 255, 18), 4.0f);
        DrawList->AddText(ImVec2(Min.x + 16.0f, Min.y + 4.0f), IM_COL32(228, 232, 240, 255), Label.c_str());

        if (!RightLabel.empty())
        {
            const ImVec2 LabelSize = ImGui::CalcTextSize(RightLabel.c_str());
            DrawList->AddText(
                ImVec2(Max.x - LabelSize.x - 10.0f, Min.y + 4.0f),
                IM_COL32(153, 160, 171, 255),
                RightLabel.c_str());
        }

        ImGui::PopID();
        return Interaction;
    }

    struct FOutlinerRowGeometry
    {
        ImVec2 Min;
        ImVec2 Max;
    };

    struct FNotifyTrackContextResult
    {
        bool bRequestRename = false;
    };

    FOutlinerRowGeometry BuildOutlinerRowGeometry(
        const ImVec2& CanvasPos,
        const ImVec2& CanvasEnd,
        float RowTop,
        float RowBottom,
        float LeftInset,
        float RightInset)
    {
        // Keep row geometry construction separate from row interaction so
        // hit-target changes can be made without rewriting the draw helpers.
        return
        {
            ImVec2(CanvasPos.x + LeftInset, RowTop),
            ImVec2(CanvasEnd.x - RightInset, RowBottom)
        };
    }

    void ApplyRowHoverState(
        FAnimationSequenceEditorState& State,
        const FAnimationSequenceSequencerVisibleRow& Row,
        const FRowInteraction& Interaction)
    {
        if (Interaction.bHovered)
        {
            State.SetHoveredSequencerRow(Row.Id);
        }
    }

    void ApplyRowFocusState(
        FAnimationSequenceEditorState& State,
        const FAnimationSequenceSequencerVisibleRow& Row,
        const FRowInteraction& Interaction)
    {
        if (Interaction.bClicked)
        {
            State.SetFocusedSequencerRow(Row.Id);
        }
    }

    FNotifyTrackContextResult HandleNotifyTrackContextMenu(
        FAnimationSequenceEditorState& State,
        FAnimationSequenceEditorDocument* Document,
        const FAnimationSequenceSequencerVisibleRow& Row)
    {
        FNotifyTrackContextResult Result;
        const FString NotifyTrackContextPopupId = "##NotifyTrackContext" + std::to_string(Row.SourceIndex);
        if (!(Document && ImGui::BeginPopupContextItem(NotifyTrackContextPopupId.c_str())))
        {
            return Result;
        }

        if (ImGui::MenuItem("Rename Track"))
        {
            // Return intent to the widget instead of opening the popup here so
            // popup state stays owned by the outliner widget instance.
            Result.bRequestRename = true;
        }

        const bool bCanPasteNotify = Document->CanPasteNotify();
        if (ImGui::MenuItem("Paste Notify Here", nullptr, false, bCanPasteNotify))
        {
            if (Document->PasteNotifyToTrackAtTime(Row.SourceIndex, State.CurrentTime))
            {
                State.SetFocusedSequencerRow(Row.Id);
            }
        }

        const bool bCanMoveSelectedNotify =
            State.HasSelectedNotify() &&
            State.SelectedNotifyTrackIndex != Row.SourceIndex;
        if (ImGui::MenuItem("Move Selected Notify Here", nullptr, false, bCanMoveSelectedNotify))
        {
            if (Document->MoveSelectedNotifyToTrack(Row.SourceIndex))
            {
                State.SetFocusedSequencerRow(Row.Id);
            }
        }

        const bool bCanMoveUp = Row.SourceIndex > 0;
        if (ImGui::MenuItem("Move Up", nullptr, false, bCanMoveUp))
        {
            Document->MoveNotifyTrack(Row.SourceIndex, Row.SourceIndex - 1);
        }

        const bool bCanMoveDown = Row.SourceIndex + 1 < Document->GetNotifyTrackCount();
        if (ImGui::MenuItem("Move Down", nullptr, false, bCanMoveDown))
        {
            Document->MoveNotifyTrack(Row.SourceIndex, Row.SourceIndex + 1);
        }

        const FAnimNotifyTrack* Track = Document->GetNotifyTrack(Row.SourceIndex);
        const bool bCanDelete = Track && Track->Events.empty();
        if (ImGui::MenuItem("Delete Empty Track", nullptr, false, bCanDelete))
        {
            Document->DeleteNotifyTrack(Row.SourceIndex);
        }

        ImGui::EndPopup();
        return Result;
    }

    void HandleNotifyTrackSelection(
        FAnimationSequenceEditorState& State,
        FAnimationSequenceEditorDocument* Document,
        const FAnimationSequenceSequencerVisibleRow& Row,
        const FRowInteraction& Interaction)
    {
        ApplyRowHoverState(State, Row, Interaction);
        if (!Interaction.bClicked)
        {
            return;
        }

        State.SetFocusedSequencerRow(Row.Id);
        State.SelectedCurveIndex = -1;
        State.HoveredCurveIndex = -1;
        const FAnimNotifyTrack* Track = Document ? Document->GetNotifyTrack(Row.SourceIndex) : nullptr;
        if (Track && !Track->Events.empty())
        {
            const int32 EventIndex = std::clamp(State.SelectedNotifyEventIndex, 0, static_cast<int32>(Track->Events.size()) - 1);
            Document->SelectNotify(Row.SourceIndex, EventIndex);
        }
    }

    void HandleCurveGroupInteraction(
        FAnimationSequenceEditorState& State,
        const FAnimationSequenceSequencerVisibleRow& Row,
        const FRowInteraction& Interaction)
    {
        ApplyRowHoverState(State, Row, Interaction);
        if (Interaction.bClicked)
        {
            State.SetFocusedSequencerRow(Row.Id);
            AnimationSequenceCurveFilter::SetCurveTypeEnabled(State, Row.CurveType, !Row.bToggleChecked);
        }
    }

    void HandleCurveEntryInteraction(
        FAnimationSequenceEditorState& State,
        FAnimationSequenceEditorDocument* Document,
        const FAnimationSequenceSequencerVisibleRow& Row,
        const FRowInteraction& Interaction)
    {
        ApplyRowHoverState(State, Row, Interaction);
        if (!Interaction.bClicked)
        {
            return;
        }

        State.SetFocusedSequencerRow(Row.Id);
        State.SelectedCurveIndex = Row.SourceIndex;
        State.HoveredCurveIndex = Row.SourceIndex;
        if (Document)
        {
            Document->ClearNotifySelection();
        }
    }

    FRowInteraction DrawCurveGroupRow(
        int32 UniqueId,
        const FAnimationSequenceSequencerVisibleRow& Row,
        bool bFocused,
        const ImVec2& Min,
        const ImVec2& Max)
    {
        ImGui::SetCursorScreenPos(Min);
        ImGui::PushID(UniqueId);
        ImGui::InvisibleButton("##CurveGroupRow", ImVec2(Max.x - Min.x, Max.y - Min.y));
        FRowInteraction Interaction;
        Interaction.bHovered = ImGui::IsItemHovered();
        Interaction.bClicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);

        ImDrawList* DrawList = ImGui::GetWindowDrawList();
        DrawList->AddRectFilled(
            Min,
            Max,
            bFocused
                ? IM_COL32(39, 49, 63, 245)
                : (Interaction.bHovered ? IM_COL32(33, 37, 45, 245) : IM_COL32(25, 28, 34, 230)),
            4.0f);
        DrawList->AddRect(Min, Max, IM_COL32(255, 255, 255, 16), 4.0f);

        const ImVec2 CheckboxMin(Min.x + 10.0f, Min.y + 4.0f);
        const ImVec2 CheckboxMax(CheckboxMin.x + 12.0f, CheckboxMin.y + 12.0f);
        DrawList->AddRect(CheckboxMin, CheckboxMax, IM_COL32(190, 198, 210, 180), 2.0f);
        if (Row.bToggleChecked)
        {
            DrawList->AddLine(
                ImVec2(CheckboxMin.x + 2.0f, CheckboxMin.y + 6.0f),
                ImVec2(CheckboxMin.x + 5.0f, CheckboxMin.y + 9.0f),
                IM_COL32(98, 202, 255, 255),
                2.0f);
            DrawList->AddLine(
                ImVec2(CheckboxMin.x + 5.0f, CheckboxMin.y + 9.0f),
                ImVec2(CheckboxMax.x - 2.0f, CheckboxMin.y + 2.0f),
                IM_COL32(98, 202, 255, 255),
                2.0f);
        }

        DrawList->AddText(
            ImVec2(CheckboxMax.x + 8.0f, Min.y + 3.0f),
            IM_COL32(215, 220, 228, 255),
            Row.Label.c_str());

        if (!Row.SecondaryLabel.empty())
        {
            const ImVec2 CountSize = ImGui::CalcTextSize(Row.SecondaryLabel.c_str());
            DrawList->AddText(
                ImVec2(Max.x - CountSize.x - 10.0f, Min.y + 3.0f),
                IM_COL32(143, 150, 161, 255),
                Row.SecondaryLabel.c_str());
        }

        ImGui::PopID();
        return Interaction;
    }

    void DrawInfoRow(
        const FString& Label,
        bool bFocused,
        const ImVec2& Min,
        const ImVec2& Max)
    {
        ImDrawList* DrawList = ImGui::GetWindowDrawList();
        DrawList->AddRectFilled(Min, Max, bFocused ? IM_COL32(35, 44, 58, 205) : IM_COL32(24, 27, 33, 205), 4.0f);
        DrawList->AddRect(Min, Max, IM_COL32(255, 255, 255, 14), 4.0f);
        DrawList->AddText(ImVec2(Min.x + 16.0f, Min.y + 4.0f), IM_COL32(132, 139, 150, 255), Label.c_str());
    }
}

void FAnimationSequenceTrackOutlinerWidget::Render(
    FAnimationSequenceEditorState& State,
    FAnimationSequenceEditorDocument* Document,
    const FAnimationSequenceSequencerVisibleLayout& VisibleLayout,
    float RowOriginY)
{
    const ImVec2 CanvasPos = ImGui::GetCursorScreenPos();
    const ImVec2 CanvasSize(
        std::max(1.0f, ImGui::GetContentRegionAvail().x),
        std::max(1.0f, ImGui::GetContentRegionAvail().y));
    ImGui::Dummy(CanvasSize);

    const ImVec2 CanvasEnd(CanvasPos.x + CanvasSize.x, CanvasPos.y + CanvasSize.y);
    ImDrawList* DrawList = ImGui::GetWindowDrawList();
    DrawList->AddRectFilled(CanvasPos, CanvasEnd, IM_COL32(22, 24, 29, 255));
    DrawList->AddRect(CanvasPos, CanvasEnd, IM_COL32(255, 255, 255, 20));

    for (const FAnimationSequenceSequencerVisibleRow& Row : VisibleLayout.Rows)
    {
        const float RowTop = RowOriginY + Row.OffsetY;
        const float RowBottom = RowTop + Row.Height;
        const bool bRowFocused = State.IsFocusedSequencerRow(Row.Id);

        switch (Row.Type)
        {
        case EAnimationSequenceSequencerVisibleRowType::NotifyHeader:
        {
            const FOutlinerRowGeometry RowGeometry =
                BuildOutlinerRowGeometry(CanvasPos, CanvasEnd, RowTop, RowBottom, 4.0f, 4.0f);
            const FRowInteraction Interaction = DrawSectionRow(
                "NotifySection",
                Row.Label,
                Row.bCanExpand,
                State.bNotifiesExpanded,
                bRowFocused,
                RowGeometry.Min,
                RowGeometry.Max);
            ApplyRowHoverState(State, Row, Interaction);
            ApplyRowFocusState(State, Row, Interaction);
            if (Document && ImGui::BeginPopupContextItem("##NotifySectionContext"))
            {
                if (ImGui::MenuItem("Add Track"))
                {
                    const int32 NewTrackIndex = Document->AddNotifyTrack();
                    if (NewTrackIndex >= 0)
                    {
                        State.bNotifiesExpanded = true;
                        State.SetFocusedSequencerRow(
                            MakeAnimationSequenceSequencerRowId(
                                EAnimationSequenceSequencerVisibleRowType::NotifyTrack,
                                NewTrackIndex));
                    }
                }
                ImGui::EndPopup();
            }
            break;
        }

        case EAnimationSequenceSequencerVisibleRowType::NotifyTrack:
        {
            const FOutlinerRowGeometry RowGeometry =
                BuildOutlinerRowGeometry(CanvasPos, CanvasEnd, RowTop, RowBottom, 8.0f, 8.0f);
            const FRowInteraction Interaction = DrawSelectableRow(
                    1000 + Row.SourceIndex,
                    Row.Label,
                    Row.SecondaryLabel,
                    Row.bSelected,
                    bRowFocused,
                    RowGeometry.Min,
                    RowGeometry.Max);
            HandleNotifyTrackSelection(State, Document, Row, Interaction);
            const FNotifyTrackContextResult ContextResult = HandleNotifyTrackContextMenu(State, Document, Row);
            if (ContextResult.bRequestRename)
            {
                OpenRenameNotifyTrackPopup(Row.SourceIndex, Row.Label);
            }
            break;
        }

        case EAnimationSequenceSequencerVisibleRowType::CurveHeader:
        {
            const FOutlinerRowGeometry RowGeometry =
                BuildOutlinerRowGeometry(CanvasPos, CanvasEnd, RowTop, RowBottom, 4.0f, 4.0f);
            const FRowInteraction Interaction = DrawSectionRow(
                "CurveSection",
                Row.Label,
                Row.bCanExpand,
                State.bCurvesExpanded,
                bRowFocused,
                RowGeometry.Min,
                RowGeometry.Max);
            ApplyRowHoverState(State, Row, Interaction);
            ApplyRowFocusState(State, Row, Interaction);
            break;
        }

        case EAnimationSequenceSequencerVisibleRowType::CurveGroup:
        case EAnimationSequenceSequencerVisibleRowType::AttributeGroup:
        {
            const FOutlinerRowGeometry RowGeometry =
                BuildOutlinerRowGeometry(CanvasPos, CanvasEnd, RowTop, RowBottom, 8.0f, 8.0f);
            const FRowInteraction Interaction = DrawCurveGroupRow(
                    3000 + Row.GroupIndex + (Row.Type == EAnimationSequenceSequencerVisibleRowType::AttributeGroup ? 2000 : 0),
                    Row,
                    bRowFocused,
                    RowGeometry.Min,
                    RowGeometry.Max);
            HandleCurveGroupInteraction(State, Row, Interaction);
            break;
        }

        case EAnimationSequenceSequencerVisibleRowType::CurveEntry:
        case EAnimationSequenceSequencerVisibleRowType::AttributeEntry:
        {
            const FOutlinerRowGeometry RowGeometry =
                BuildOutlinerRowGeometry(CanvasPos, CanvasEnd, RowTop, RowBottom, 18.0f, 8.0f);
            const FRowInteraction Interaction = DrawSelectableRow(
                    4000 + Row.SourceIndex + (Row.Type == EAnimationSequenceSequencerVisibleRowType::AttributeEntry ? 2000 : 0),
                    Row.Label,
                    Row.SecondaryLabel,
                    Row.bSelected,
                    bRowFocused,
                    RowGeometry.Min,
                    RowGeometry.Max);
            HandleCurveEntryInteraction(State, Document, Row, Interaction);
            break;
        }

        case EAnimationSequenceSequencerVisibleRowType::AdditiveHeader:
        {
            const FOutlinerRowGeometry RowGeometry =
                BuildOutlinerRowGeometry(CanvasPos, CanvasEnd, RowTop, RowBottom, 4.0f, 4.0f);
            const FRowInteraction Interaction = DrawSectionRow(
                "AdditiveLayerSection",
                Row.Label,
                Row.bCanExpand,
                State.bAdditiveLayerTracksExpanded,
                bRowFocused,
                RowGeometry.Min,
                RowGeometry.Max);
            ApplyRowHoverState(State, Row, Interaction);
            ApplyRowFocusState(State, Row, Interaction);
            break;
        }

        case EAnimationSequenceSequencerVisibleRowType::AdditiveEmpty:
        case EAnimationSequenceSequencerVisibleRowType::AttributeEmpty:
        {
            const FOutlinerRowGeometry RowGeometry =
                BuildOutlinerRowGeometry(CanvasPos, CanvasEnd, RowTop, RowBottom, 8.0f, 8.0f);
            DrawInfoRow(
                Row.Label,
                bRowFocused,
                RowGeometry.Min,
                RowGeometry.Max);
            break;
        }

        case EAnimationSequenceSequencerVisibleRowType::AttributeHeader:
        {
            const FOutlinerRowGeometry RowGeometry =
                BuildOutlinerRowGeometry(CanvasPos, CanvasEnd, RowTop, RowBottom, 4.0f, 4.0f);
            const FRowInteraction Interaction = DrawSectionRow(
                "AttributeSection",
                Row.Label,
                Row.bCanExpand,
                State.bAttributesExpanded,
                bRowFocused,
                RowGeometry.Min,
                RowGeometry.Max);
            ApplyRowHoverState(State, Row, Interaction);
            ApplyRowFocusState(State, Row, Interaction);
            break;
        }
        }
    }

    if (bOpenRenameNotifyTrackPopupPending)
    {
        ImGui::OpenPopup("##RenameNotifyTrackPopup");
        bOpenRenameNotifyTrackPopupPending = false;
    }

    RenderRenameNotifyTrackPopup(Document);
}

void FAnimationSequenceTrackOutlinerWidget::OpenRenameNotifyTrackPopup(int32 TrackIndex, const FString& CurrentLabel)
{
    RenameNotifyTrackIndex = TrackIndex;
    RenameNotifyTrackBuffer.fill('\0');
    strncpy_s(RenameNotifyTrackBuffer.data(), RenameNotifyTrackBuffer.size(), CurrentLabel.c_str(), _TRUNCATE);
    bOpenRenameNotifyTrackPopupPending = true;
}

void FAnimationSequenceTrackOutlinerWidget::RenderRenameNotifyTrackPopup(FAnimationSequenceEditorDocument* Document)
{
    if (!Document)
    {
        bOpenRenameNotifyTrackPopupPending = false;
        RenameNotifyTrackIndex = -1;
        return;
    }

    if (ImGui::BeginPopup("##RenameNotifyTrackPopup"))
    {
        ImGui::TextDisabled("Rename Notify Track");
        ImGui::SetNextItemWidth(220.0f);
        const bool bConfirmedByEnter =
            ImGui::InputText(
                "##RenameNotifyTrackInput",
                RenameNotifyTrackBuffer.data(),
                RenameNotifyTrackBuffer.size(),
                ImGuiInputTextFlags_EnterReturnsTrue);
        if (ImGui::IsWindowAppearing())
        {
            ImGui::SetKeyboardFocusHere(-1);
        }

        bool bClosePopup = false;
        if (ImGui::Button("Apply"))
        {
            bClosePopup = Document->RenameNotifyTrack(RenameNotifyTrackIndex, FName(RenameNotifyTrackBuffer.data()));
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            bClosePopup = true;
        }

        if (bConfirmedByEnter)
        {
            bClosePopup = Document->RenameNotifyTrack(RenameNotifyTrackIndex, FName(RenameNotifyTrackBuffer.data()));
        }

        if (bClosePopup)
        {
            bOpenRenameNotifyTrackPopupPending = false;
            RenameNotifyTrackIndex = -1;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
    else if (RenameNotifyTrackIndex >= 0 && !ImGui::IsPopupOpen("##RenameNotifyTrackPopup"))
    {
        bOpenRenameNotifyTrackPopupPending = false;
        RenameNotifyTrackIndex = -1;
    }
}
