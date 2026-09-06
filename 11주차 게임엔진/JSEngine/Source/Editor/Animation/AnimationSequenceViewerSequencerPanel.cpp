#include "Editor/Animation/AnimationSequenceEditorWidget.h"

#include "Editor/Animation/AnimationSequenceCurveFilter.h"
#include "Editor/Animation/AnimationSequenceEditorDocument.h"
#include "Editor/Animation/AnimationSequenceEditorState.h"
#include "Editor/Animation/AnimationSequencePreviewController.h"
#include "Editor/Animation/AnimationSequenceSequencerLayout.h"
#include "Editor/Animation/AnimationSequenceViewerUiHelpers.h"
#include "Editor/Animation/AnimationSequenceTimelineGeometry.h"
#include "Editor/Animation/AnimationSequenceViewerUtils.h"

#include "ImGui/imgui.h"

#include <algorithm>
#include <cctype>
#include <cmath>

namespace
{
    constexpr float SequencerHeaderPadding = 8.0f;

    struct FSequencerCurvePartitions
    {
        TArray<FAnimationSequenceCurveViewGroup> CurveGroups;
        TArray<FAnimationSequenceCurveViewGroup> AttributeCurveGroups;
    };

    struct FSequencerRegionMetrics
    {
        float DetailsPaneHeight = 0.0f;
        float SplitPaneHeight = 0.0f;
        float MaxDetailsPaneHeight = 0.0f;
        float MaxOutlinerWidth = 0.0f;
    };

    struct FSequencerSplitPaneMetrics
    {
        float TrackOutlinerWidth = 0.0f;
        float TimelinePaneWidth = 0.0f;
        float MainPaneHeight = 0.0f;
        float TrackListHeight = 0.0f;
        float TimelineCanvasHeight = 0.0f;
        float TrackViewportHeight = 0.0f;
        float MaxScrollY = 0.0f;
    };

    struct FTrackOutlinerPaneMetrics
    {
        float FilterHeight = 0.0f;
        float TrackListHeight = 0.0f;
    };

    struct FTimelineCanvasContext
    {
        ImVec2 CanvasPos;
        ImVec2 CanvasSize;
        ImVec2 CanvasEnd;
        FAnimationSequenceTimelineGeometry Geometry;
        float TimelineRowOriginY = 0.0f;
        bool bCanvasHovered = false;
        bool bCanvasActive = false;
    };

    float GetTrackOutlinerFilterHeight()
    {
        return
            FAnimationSequenceTimelineGeometry::VerticalPadding +
            FAnimationSequenceSequencerLayout::RulerHeight +
            FAnimationSequenceSequencerLayout::TrackAreaPadding;
    }

    FString NormalizeFilterText(const FString& Text)
    {
        FString Normalized = Text;
        std::transform(
            Normalized.begin(),
            Normalized.end(),
            Normalized.begin(),
            [](unsigned char Character)
            {
                return static_cast<char>(std::tolower(Character));
            });
        return Normalized;
    }

    bool MatchesNormalizedFilter(const FString& NormalizedFilter, const FString& Candidate)
    {
        return NormalizedFilter.empty() || NormalizeFilterText(Candidate).find(NormalizedFilter) != FString::npos;
    }

    bool IsAttributeCurveGroup(const FAnimationSequenceCurveViewGroup& Group)
    {
        return Group.CurveType == EAnimCurveType::Attribute;
    }

    TArray<FAnimationSequenceCurveViewGroup> PartitionCurveGroups(
        const TArray<FAnimationSequenceCurveViewGroup>& CurveGroups,
        bool bAttributesOnly)
    {
        TArray<FAnimationSequenceCurveViewGroup> Result;
        for (const FAnimationSequenceCurveViewGroup& Group : CurveGroups)
        {
            if (IsAttributeCurveGroup(Group) == bAttributesOnly)
            {
                Result.push_back(Group);
            }
        }

        return Result;
    }

    FSequencerCurvePartitions BuildSequencerCurvePartitions(UAnimSequence* Sequence, const FAnimationSequenceEditorState* EditorState)
    {
        FSequencerCurvePartitions Partitions;
        if (!EditorState)
        {
            return Partitions;
        }

        const TArray<FAnimationSequenceCurveViewGroup> AllCurveGroups =
            AnimationSequenceCurveFilter::BuildCurveViewGroups(Sequence, *EditorState);
        Partitions.CurveGroups = PartitionCurveGroups(AllCurveGroups, false);
        Partitions.AttributeCurveGroups = PartitionCurveGroups(AllCurveGroups, true);
        return Partitions;
    }

    FSequencerRegionMetrics BuildSequencerRegionMetrics(
        FAnimationSequenceEditorState& EditorState,
        float SequencerHeight)
    {
        FSequencerRegionMetrics Metrics;
        const float DefaultDetailsPaneHeight =
            std::min(FAnimationSequenceSequencerLayout::DetailsPanelHeight, SequencerHeight * 0.34f);
        Metrics.MaxDetailsPaneHeight = std::max(
            FAnimationSequenceSequencerLayout::DetailsPanelMinHeight,
            SequencerHeight -
                FAnimationSequenceSequencerLayout::BottomControlStripHeight -
                FAnimationSequenceSequencerLayout::SectionSplitterHeight -
                140.0f);

        if (EditorState.SequencerDetailsPaneHeight <= 0.0f)
        {
            EditorState.SequencerDetailsPaneHeight = DefaultDetailsPaneHeight;
        }

        Metrics.DetailsPaneHeight = std::clamp(
            EditorState.SequencerDetailsPaneHeight,
            FAnimationSequenceSequencerLayout::DetailsPanelMinHeight,
            Metrics.MaxDetailsPaneHeight);
        Metrics.SplitPaneHeight = std::max(
            SequencerHeight -
                Metrics.DetailsPaneHeight -
                FAnimationSequenceSequencerLayout::SectionSplitterHeight,
            140.0f);
        Metrics.MaxOutlinerWidth = std::max(
            FAnimationSequenceSequencerLayout::OutlinerMinWidth,
            ImGui::GetContentRegionAvail().x - 280.0f);
        return Metrics;
    }

    FSequencerSplitPaneMetrics BuildSequencerSplitPaneMetrics(
        const FAnimationSequenceEditorState& EditorState,
        const FAnimationSequenceSequencerVisibleLayout& VisibleLayout,
        float SplitPaneHeight,
        float MaxOutlinerWidth)
    {
        FSequencerSplitPaneMetrics Metrics;
        Metrics.TrackOutlinerWidth = std::clamp(
            EditorState.TrackOutlinerWidth,
            FAnimationSequenceSequencerLayout::OutlinerMinWidth,
            MaxOutlinerWidth);

        const float TrackBodyHeight = VisibleLayout.ContentHeight;
        Metrics.MainPaneHeight = std::max(
            SplitPaneHeight -
                FAnimationSequenceSequencerLayout::BottomControlStripHeight -
                SequencerHeaderPadding,
            120.0f);
        Metrics.TrackListHeight = std::max(
            Metrics.MainPaneHeight -
                GetTrackOutlinerFilterHeight() -
                SequencerHeaderPadding,
            80.0f);
        Metrics.TimelineCanvasHeight = std::max(Metrics.MainPaneHeight, 80.0f);
        Metrics.TrackViewportHeight =
            std::min(Metrics.TrackListHeight, Metrics.TimelineCanvasHeight) -
            FAnimationSequenceSequencerLayout::RulerHeight -
            FAnimationSequenceTimelineGeometry::VerticalPadding * 2.0f;
        Metrics.MaxScrollY = std::max(
            0.0f,
            TrackBodyHeight + FAnimationSequenceSequencerLayout::TrackAreaPadding * 2.0f - Metrics.TrackViewportHeight);

        const float SplitWidth = std::max(ImGui::GetContentRegionAvail().x, 1.0f);
        Metrics.TimelinePaneWidth = std::max(
            SplitWidth -
                Metrics.TrackOutlinerWidth -
                FAnimationSequenceSequencerLayout::OutlinerSplitterWidth,
            120.0f);
        return Metrics;
    }

    FTrackOutlinerPaneMetrics BuildTrackOutlinerPaneMetrics(float Height)
    {
        FTrackOutlinerPaneMetrics Metrics;
        Metrics.FilterHeight = GetTrackOutlinerFilterHeight();
        Metrics.TrackListHeight = std::max(Height - Metrics.FilterHeight - SequencerHeaderPadding, 80.0f);
        return Metrics;
    }

    bool HandlePreviewSequencerSplitter(
        FAnimationSequenceEditorState& EditorState,
        float MaxPreviewHeight)
    {
        ImDrawList* SplitterDrawList = ImGui::GetWindowDrawList();
        const float SplitterWidth = std::max(ImGui::GetContentRegionAvail().x, 1.0f);
        ImGui::InvisibleButton(
            "##AnimSequencePreviewSequencerSplitter",
            ImVec2(SplitterWidth, FAnimationSequenceSequencerLayout::SectionSplitterHeight));
        const bool bSplitterHovered = ImGui::IsItemHovered();
        const bool bSplitterActive = ImGui::IsItemActive();
        const ImVec2 SplitterMin = ImGui::GetItemRectMin();
        const ImVec2 SplitterMax = ImGui::GetItemRectMax();
        const ImU32 SplitterColor = ImGui::GetColorU32(
            bSplitterActive ? ImGuiCol_SeparatorActive : (bSplitterHovered ? ImGuiCol_SeparatorHovered : ImGuiCol_Separator));
        SplitterDrawList->AddRectFilled(SplitterMin, SplitterMax, SplitterColor, 2.0f);
        if (bSplitterHovered || bSplitterActive)
        {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
        }
        if (bSplitterActive)
        {
            EditorState.PreviewPaneHeight = std::clamp(
                EditorState.PreviewPaneHeight + ImGui::GetIO().MouseDelta.y,
                FAnimationSequenceSequencerLayout::PreviewMinHeight,
                MaxPreviewHeight);
        }
        return bSplitterActive;
    }

    void HandleDetailsPaneSplitter(
        FAnimationSequenceEditorState& EditorState,
        float MaxDetailsPaneHeight)
    {
        ImDrawList* SplitterDrawList = ImGui::GetWindowDrawList();
        ImGui::InvisibleButton(
            "##AnimSequenceTracksDetailsSplitter",
            ImVec2(std::max(ImGui::GetContentRegionAvail().x, 1.0f), FAnimationSequenceSequencerLayout::SectionSplitterHeight));
        const bool bDetailsSplitterHovered = ImGui::IsItemHovered();
        const bool bDetailsSplitterActive = ImGui::IsItemActive();
        const ImVec2 DetailsSplitterMin = ImGui::GetItemRectMin();
        const ImVec2 DetailsSplitterMax = ImGui::GetItemRectMax();
        const ImU32 DetailsSplitterColor = ImGui::GetColorU32(
            bDetailsSplitterActive ? ImGuiCol_SeparatorActive : (bDetailsSplitterHovered ? ImGuiCol_SeparatorHovered : ImGuiCol_Separator));
        SplitterDrawList->AddRectFilled(DetailsSplitterMin, DetailsSplitterMax, DetailsSplitterColor, 2.0f);
        if (bDetailsSplitterHovered || bDetailsSplitterActive)
        {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
        }
        if (bDetailsSplitterActive)
        {
            EditorState.SequencerDetailsPaneHeight = std::clamp(
                EditorState.SequencerDetailsPaneHeight - ImGui::GetIO().MouseDelta.y,
                FAnimationSequenceSequencerLayout::DetailsPanelMinHeight,
                MaxDetailsPaneHeight);
        }
    }

    void HandleOutlinerSplitter(
        FAnimationSequenceEditorState& EditorState,
        float MainPaneHeight,
        float MaxOutlinerWidth)
    {
        ImGui::InvisibleButton(
            "##AnimationSequenceOutlinerSplitter",
            ImVec2(FAnimationSequenceSequencerLayout::OutlinerSplitterWidth, MainPaneHeight));
        const bool bHovered = ImGui::IsItemHovered();
        const bool bActive = ImGui::IsItemActive();
        const ImVec2 SplitterMin = ImGui::GetItemRectMin();
        const ImVec2 SplitterMax = ImGui::GetItemRectMax();
        ImGui::GetWindowDrawList()->AddRectFilled(
            SplitterMin,
            SplitterMax,
            ImGui::GetColorU32(bActive ? ImGuiCol_SeparatorActive : (bHovered ? ImGuiCol_SeparatorHovered : ImGuiCol_Separator)),
            2.0f);
        if (bHovered || bActive)
        {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        }
        if (bActive)
        {
            EditorState.TrackOutlinerWidth = std::clamp(
                EditorState.TrackOutlinerWidth + ImGui::GetIO().MouseDelta.x,
                FAnimationSequenceSequencerLayout::OutlinerMinWidth,
                MaxOutlinerWidth);
        }
    }

    void UpdateSequencerFocusState(
        FAnimationSequenceEditorState& EditorState,
        const FAnimationSequenceSequencerVisibleLayout& VisibleLayout)
    {
        bool bFocusedRowVisible = !EditorState.FocusedSequencerRowId.IsValid();
        for (const FAnimationSequenceSequencerVisibleRow& Row : VisibleLayout.Rows)
        {
            if (EditorState.FocusedSequencerRowId.IsValid() && Row.Id == EditorState.FocusedSequencerRowId)
            {
                bFocusedRowVisible = true;
                break;
            }
        }

        if (!bFocusedRowVisible)
        {
            EditorState.ClearFocusedSequencerRow();
        }

        if (!EditorState.FocusedSequencerRowId.IsValid())
        {
            for (const FAnimationSequenceSequencerVisibleRow& Row : VisibleLayout.Rows)
            {
                if (Row.bSelected)
                {
                    EditorState.SetFocusedSequencerRow(Row.Id);
                    break;
                }
            }
        }
    }

    void ClampSequencerScrollState(
        FAnimationSequenceEditorState& EditorState,
        float MaxScrollY)
    {
        EditorState.SequencerScrollY = std::clamp(EditorState.SequencerScrollY, 0.0f, MaxScrollY);
    }

    template <size_t BufferSize>
    void RenderTrackOutlinerFilterBar(
        std::array<char, BufferSize>& TrackFilterEditBuffer,
        const FTrackOutlinerPaneMetrics& Metrics)
    {
        const float FilterBandTopY = ImGui::GetCursorPosY();
        const float FilterInputHeight = ImGui::GetFrameHeight();
        ImGui::SetCursorPosY(
            FilterBandTopY +
            std::max(0.0f, (Metrics.FilterHeight - FilterInputHeight) * 0.5f));
        AnimationSequenceViewerUi::SetFullWidthItem();
        ImGui::InputTextWithHint(
            "##AnimationSequenceTrackFilter",
            "Filter tracks and curves",
            TrackFilterEditBuffer.data(),
            TrackFilterEditBuffer.size());
        ImGui::SetCursorPosY(FilterBandTopY + Metrics.FilterHeight);
    }

    void HandleOutlinerScrollInput(FAnimationSequenceEditorState& EditorState)
    {
        if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) &&
            std::fabs(ImGui::GetIO().MouseWheel) > 0.0f)
        {
            EditorState.SequencerScrollY = std::max(EditorState.SequencerScrollY - ImGui::GetIO().MouseWheel * 24.0f, 0.0f);
        }
    }

    void RenderTrackOutlinerList(
        FAnimationSequenceTrackOutlinerWidget& TrackOutlinerWidget,
        FAnimationSequenceEditorState& EditorState,
        FAnimationSequenceEditorDocument* Document,
        const FAnimationSequenceSequencerVisibleLayout& VisibleLayout,
        const FTrackOutlinerPaneMetrics& Metrics)
    {
        ImGui::BeginChild(
            "##AnimationSequenceTrackOutlinerList",
            ImVec2(0.0f, Metrics.TrackListHeight),
            false,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        const float RowOriginY = ImGui::GetCursorScreenPos().y - EditorState.SequencerScrollY;
        TrackOutlinerWidget.Render(EditorState, Document, VisibleLayout, RowOriginY);
        HandleOutlinerScrollInput(EditorState);
        ImGui::EndChild();
    }

    FTimelineCanvasContext BuildTimelineCanvasContext(
        FAnimationSequenceTimelineWidget& TimelineWidget,
        const FAnimationSequenceEditorState& EditorState,
        const FAnimationSequenceSequencerVisibleLayout& VisibleLayout,
        float Height)
    {
        FTimelineCanvasContext Context;
        const float RequiredCanvasHeight =
            FAnimationSequenceTimelineGeometry::VerticalPadding * 2.0f +
            TimelineWidget.GetRulerHeight() +
            FAnimationSequenceSequencerLayout::TrackAreaPadding * 2.0f +
            VisibleLayout.ContentHeight;

        Context.CanvasPos = ImGui::GetCursorScreenPos();
        Context.CanvasSize = ImVec2(
            std::max(1.0f, ImGui::GetContentRegionAvail().x),
            std::max(std::max(Height - 2.0f, 1.0f), RequiredCanvasHeight));
        ImGui::Dummy(Context.CanvasSize);

        Context.CanvasEnd = ImVec2(Context.CanvasPos.x + Context.CanvasSize.x, Context.CanvasPos.y + Context.CanvasSize.y);
        Context.bCanvasHovered =
            ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) &&
            ImGui::IsMouseHoveringRect(Context.CanvasPos, Context.CanvasEnd, false);
        Context.bCanvasActive =
            Context.bCanvasHovered &&
            (ImGui::IsMouseDown(ImGuiMouseButton_Left) || ImGui::IsMouseDown(ImGuiMouseButton_Middle));
        Context.Geometry =
            FAnimationSequenceTimelineGeometry::BuildSequencerCanvasGeometry(Context.CanvasPos, Context.CanvasSize, TimelineWidget.GetRulerHeight());
        Context.TimelineRowOriginY =
            Context.Geometry.TrackTop +
            FAnimationSequenceSequencerLayout::TrackAreaPadding -
            EditorState.SequencerScrollY;

        return Context;
    }

    void UpdateTimelineHoveredRow(
        FAnimationSequenceEditorState& EditorState,
        const FAnimationSequenceTimelineGeometry& Geometry,
        const FAnimationSequenceSequencerVisibleLayout& VisibleLayout,
        float TimelineRowOriginY,
        bool bCanvasHovered)
    {
        if (!bCanvasHovered)
        {
            return;
        }

        const ImVec2 MousePos = ImGui::GetIO().MousePos;
        for (const FAnimationSequenceSequencerVisibleRow& Row : VisibleLayout.Rows)
        {
            const float RowTop = TimelineRowOriginY + Row.OffsetY;
            const float RowBottom = RowTop + Row.Height;
            if (MousePos.x >= Geometry.TimelineMinX &&
                MousePos.x <= Geometry.TimelineMaxX &&
                MousePos.y >= RowTop &&
                MousePos.y <= RowBottom)
            {
                EditorState.SetHoveredSequencerRow(Row.Id);
                break;
            }
        }
    }

    void PrepareTimelineCanvasInteractionState(
        FAnimationSequenceEditorState& EditorState,
        const FTimelineCanvasContext& CanvasContext,
        const FAnimationSequenceSequencerVisibleLayout& VisibleLayout)
    {
        EditorState.ClearHoveredSequencerRow();
        UpdateTimelineHoveredRow(
            EditorState,
            CanvasContext.Geometry,
            VisibleLayout,
            CanvasContext.TimelineRowOriginY,
            CanvasContext.bCanvasHovered);
    }

    void ResetTimelineCurveHoverState(FAnimationSequenceEditorState& EditorState)
    {
        EditorState.HoveredCurveIndex = -1;
        EditorState.bHasHoveredCurveSample = false;
        EditorState.HoveredCurveSampleTime = 0.0f;
        EditorState.HoveredCurveSampleValue = 0.0f;
        EditorState.HoveredCurveNearestKeyIndex = -1;
    }

    void RenderTimelineCanvasLayers(
        FAnimationSequenceTimelineWidget& TimelineWidget,
        FAnimationSequenceNotifyLaneWidget& NotifyLaneWidget,
        FAnimationSequenceCurveTrackWidget& CurveTrackWidget,
        FAnimationSequenceEditorState& EditorState,
        FAnimationSequenceEditorDocument* Document,
        FAnimationSequencePreviewController* PreviewController,
        const FTimelineCanvasContext& CanvasContext,
        const FAnimationSequenceSequencerVisibleLayout& VisibleLayout)
    {
        TimelineWidget.RenderCanvas(
            EditorState,
            PreviewController,
            CanvasContext.Geometry,
            CanvasContext.bCanvasHovered,
            CanvasContext.bCanvasActive);
        NotifyLaneWidget.RenderRows(
            EditorState,
            Document,
            CanvasContext.Geometry,
            VisibleLayout,
            CanvasContext.TimelineRowOriginY);
        ResetTimelineCurveHoverState(EditorState);
        CurveTrackWidget.RenderRows(
            EditorState,
            CanvasContext.Geometry,
            VisibleLayout,
            CanvasContext.TimelineRowOriginY);
        if (Document &&
            ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
            EditorState.HoveredCurveIndex >= 0)
        {
            Document->ClearNotifySelection();
        }
    }

    struct FSequencerVisibleLayoutBuilder
    {
        FAnimationSequenceSequencerVisibleLayout Layout;
        const FAnimationSequenceEditorState& EditorState;
        FAnimationSequenceEditorDocument* Document = nullptr;
        FString NormalizedFilter;
        float CursorY = 0.0f;

        FAnimationSequenceSequencerVisibleRow& AddRow(
            EAnimationSequenceSequencerVisibleRowType Type,
            float Height)
        {
            FAnimationSequenceSequencerVisibleRow& Row = Layout.Rows.emplace_back();
            Row.Type = Type;
            Row.Id = MakeAnimationSequenceSequencerRowId(Type);
            Row.OffsetY = CursorY;
            Row.Height = Height;
            CursorY += Height;
            return Row;
        }

        void AppendNotifyVisibleRows()
        {
            FAnimationSequenceSequencerVisibleRow& NotifyHeader =
                AddRow(EAnimationSequenceSequencerVisibleRowType::NotifyHeader, FAnimationSequenceSequencerLayout::SectionHeaderHeight);
            NotifyHeader.Id = MakeAnimationSequenceSequencerRowId(EAnimationSequenceSequencerVisibleRowType::NotifyHeader);
            NotifyHeader.Label = "Notifies";
            const int32 NotifyTrackCount = Document ? Document->GetNotifyTrackCount() : 0;
            NotifyHeader.bCanExpand = NotifyTrackCount > 0;
            NotifyHeader.bExpanded = EditorState.bNotifiesExpanded;
            if (!(NotifyHeader.bCanExpand && EditorState.bNotifiesExpanded))
            {
                return;
            }

            for (int32 TrackIndex = 0; TrackIndex < NotifyTrackCount; ++TrackIndex)
            {
                const FAnimNotifyTrack* Track = Document ? Document->GetNotifyTrack(TrackIndex) : nullptr;
                const FString TrackLabel = Track && Track->TrackName.IsValid()
                    ? Track->TrackName.ToString()
                    : ("Track " + std::to_string(TrackIndex + 1));
                if (!MatchesNormalizedFilter(NormalizedFilter, TrackLabel))
                {
                    continue;
                }

                FAnimationSequenceSequencerVisibleRow& Row =
                    AddRow(EAnimationSequenceSequencerVisibleRowType::NotifyTrack, FAnimationSequenceSequencerLayout::NotifyTrackRowHeight);
                Row.SourceIndex = TrackIndex;
                Row.Id = MakeAnimationSequenceSequencerRowId(EAnimationSequenceSequencerVisibleRowType::NotifyTrack, TrackIndex);
                Row.Label = TrackLabel;
                Row.SecondaryLabel = Track ? std::to_string(static_cast<int32>(Track->Events.size())) : FString("0");
                Row.bSelected = EditorState.SelectedNotifyTrackIndex == TrackIndex;
            }
        }

        void AppendCurveGroupRows(
            const TArray<FAnimationSequenceCurveViewGroup>& CurveGroups,
            EAnimationSequenceSequencerVisibleRowType GroupRowType,
            EAnimationSequenceSequencerVisibleRowType EntryRowType)
        {
            for (int32 GroupIndex = 0; GroupIndex < static_cast<int32>(CurveGroups.size()); ++GroupIndex)
            {
                const FAnimationSequenceCurveViewGroup& Group = CurveGroups[GroupIndex];
                const bool bGroupMatchesFilter = MatchesNormalizedFilter(NormalizedFilter, Group.Label);
                TArray<FAnimationSequenceCurveViewEntry> MatchingEntries;
                MatchingEntries.reserve(Group.VisibleEntries.size());
                for (const FAnimationSequenceCurveViewEntry& Entry : Group.VisibleEntries)
                {
                    const FString CurveLabel = Entry.Curve ? Entry.Curve->CurveName.ToString() : FString("Curve");
                    if (MatchesNormalizedFilter(NormalizedFilter, CurveLabel))
                    {
                        MatchingEntries.push_back(Entry);
                    }
                }

                if (!NormalizedFilter.empty() && !bGroupMatchesFilter && MatchingEntries.empty())
                {
                    continue;
                }

                FAnimationSequenceSequencerVisibleRow& GroupRow =
                    AddRow(GroupRowType, FAnimationSequenceSequencerLayout::CurveGroupHeaderHeight);
                GroupRow.GroupIndex = GroupIndex;
                GroupRow.Id = MakeAnimationSequenceSequencerRowId(GroupRowType, -1, GroupIndex);
                GroupRow.Label = Group.Label;
                GroupRow.SecondaryLabel = std::to_string(Group.TotalCount);
                GroupRow.bToggleChecked = Group.bVisible;
                GroupRow.CurveType = Group.CurveType;

                if (!(Group.bVisible && !MatchingEntries.empty()))
                {
                    continue;
                }

                for (int32 EntryIndex = 0; EntryIndex < static_cast<int32>(MatchingEntries.size()); ++EntryIndex)
                {
                    const FAnimationSequenceCurveViewEntry& Entry = MatchingEntries[EntryIndex];
                    FAnimationSequenceSequencerVisibleRow& Row =
                        AddRow(EntryRowType, FAnimationSequenceSequencerLayout::CurveTrackRowHeight);
                    Row.SourceIndex = Entry.SourceIndex;
                    Row.Id = MakeAnimationSequenceSequencerRowId(EntryRowType, Entry.SourceIndex, GroupIndex);
                    Row.Curve = Entry.Curve;
                    Row.CurveType = Group.CurveType;
                    Row.Label = Entry.Curve ? Entry.Curve->CurveName.ToString() : FString("Curve");
                    Row.SecondaryLabel = Entry.Curve
                        ? (std::to_string(static_cast<int32>(Entry.Curve->Keys.size())) + " keys")
                        : FString();
                    Row.bSelected = EditorState.SelectedCurveIndex == Entry.SourceIndex;
                }
            }
        }

        void AppendCurveVisibleRows(const TArray<FAnimationSequenceCurveViewGroup>& CurveGroups)
        {
            FAnimationSequenceSequencerVisibleRow& CurveHeader =
                AddRow(EAnimationSequenceSequencerVisibleRowType::CurveHeader, FAnimationSequenceSequencerLayout::SectionHeaderHeight);
            CurveHeader.Id = MakeAnimationSequenceSequencerRowId(EAnimationSequenceSequencerVisibleRowType::CurveHeader);
            CurveHeader.Label = "Curves";
            CurveHeader.bCanExpand = !CurveGroups.empty();
            CurveHeader.bExpanded = EditorState.bCurvesExpanded;
            if (CurveHeader.bCanExpand && EditorState.bCurvesExpanded)
            {
                AppendCurveGroupRows(
                    CurveGroups,
                    EAnimationSequenceSequencerVisibleRowType::CurveGroup,
                    EAnimationSequenceSequencerVisibleRowType::CurveEntry);
            }
        }

        void AppendAdditiveVisibleRows()
        {
            FAnimationSequenceSequencerVisibleRow& AdditiveHeader =
                AddRow(EAnimationSequenceSequencerVisibleRowType::AdditiveHeader, FAnimationSequenceSequencerLayout::SectionHeaderHeight);
            AdditiveHeader.Id = MakeAnimationSequenceSequencerRowId(EAnimationSequenceSequencerVisibleRowType::AdditiveHeader);
            AdditiveHeader.Label = "Additive Layer Tracks";
            AdditiveHeader.bCanExpand = false;
            AdditiveHeader.bExpanded = EditorState.bAdditiveLayerTracksExpanded;
            if (AdditiveHeader.bCanExpand && EditorState.bAdditiveLayerTracksExpanded)
            {
                FAnimationSequenceSequencerVisibleRow& Row =
                    AddRow(EAnimationSequenceSequencerVisibleRowType::AdditiveEmpty, FAnimationSequenceSequencerLayout::EmptySectionRowHeight);
                Row.Id = MakeAnimationSequenceSequencerRowId(EAnimationSequenceSequencerVisibleRowType::AdditiveEmpty);
                Row.Label = "No additive layer tracks.";
            }
        }

        void AppendAttributeVisibleRows(const TArray<FAnimationSequenceCurveViewGroup>& AttributeCurveGroups)
        {
            FAnimationSequenceSequencerVisibleRow& AttributeHeader =
                AddRow(EAnimationSequenceSequencerVisibleRowType::AttributeHeader, FAnimationSequenceSequencerLayout::SectionHeaderHeight);
            AttributeHeader.Id = MakeAnimationSequenceSequencerRowId(EAnimationSequenceSequencerVisibleRowType::AttributeHeader);
            AttributeHeader.Label = "Attributes";
            AttributeHeader.bCanExpand = !AttributeCurveGroups.empty();
            AttributeHeader.bExpanded = EditorState.bAttributesExpanded;
            if (AttributeHeader.bCanExpand && EditorState.bAttributesExpanded)
            {
                AppendCurveGroupRows(
                    AttributeCurveGroups,
                    EAnimationSequenceSequencerVisibleRowType::AttributeGroup,
                    EAnimationSequenceSequencerVisibleRowType::AttributeEntry);
            }
        }

        FAnimationSequenceSequencerVisibleLayout Build(
            const TArray<FAnimationSequenceCurveViewGroup>& CurveGroups,
            const TArray<FAnimationSequenceCurveViewGroup>& AttributeCurveGroups)
        {
            AppendNotifyVisibleRows();
            AppendCurveVisibleRows(CurveGroups);
            AppendAdditiveVisibleRows();
            AppendAttributeVisibleRows(AttributeCurveGroups);
            Layout.ContentHeight = CursorY;
            return Layout;
        }
    };
}

void FAnimationSequenceEditorWidget::RenderSequencerRegion(float SequencerHeight, float MaxPreviewHeight)
{
    const FSequencerCurvePartitions CurvePartitions = BuildSequencerCurvePartitions(Sequence, EditorState);

    bSuppressEmbeddedPreviewViewportInput = HandlePreviewSequencerSplitter(*EditorState, MaxPreviewHeight);

    ImGui::BeginChild("##AnimationSequenceSequencerRegion", ImVec2(0.0f, SequencerHeight), true);
    const FSequencerRegionMetrics RegionMetrics = BuildSequencerRegionMetrics(*EditorState, SequencerHeight);
    EditorState->SequencerDetailsPaneHeight = RegionMetrics.DetailsPaneHeight;

    RenderSequencerSplitPane(
        RegionMetrics.SplitPaneHeight,
        RegionMetrics.MaxOutlinerWidth,
        CurvePartitions.CurveGroups,
        CurvePartitions.AttributeCurveGroups);

    HandleDetailsPaneSplitter(*EditorState, RegionMetrics.MaxDetailsPaneHeight);
    RenderSelectionDetailsPane(RegionMetrics.DetailsPaneHeight);
    ImGui::EndChild();
}

FAnimationSequenceSequencerVisibleLayout FAnimationSequenceEditorWidget::BuildSequencerVisibleLayout(
    const TArray<FAnimationSequenceCurveViewGroup>& CurveGroups,
    const TArray<FAnimationSequenceCurveViewGroup>& AttributeCurveGroups) const
{
    FAnimationSequenceSequencerVisibleLayout Layout;
    if (!EditorState)
    {
        return Layout;
    }

    FSequencerVisibleLayoutBuilder Builder
    {
        {},
        *EditorState,
        Document,
        NormalizeFilterText(TrackFilterEditBuffer.data()),
        0.0f
    };
    return Builder.Build(CurveGroups, AttributeCurveGroups);
}

void FAnimationSequenceEditorWidget::RenderSequencerSplitPane(
    float SplitPaneHeight,
    float MaxOutlinerWidth,
    const TArray<FAnimationSequenceCurveViewGroup>& CurveGroups,
    const TArray<FAnimationSequenceCurveViewGroup>& AttributeCurveGroups)
{
    const FAnimationSequenceSequencerVisibleLayout VisibleLayout =
        BuildSequencerVisibleLayout(CurveGroups, AttributeCurveGroups);
    UpdateSequencerFocusState(*EditorState, VisibleLayout);

    const FSequencerSplitPaneMetrics PaneMetrics =
        BuildSequencerSplitPaneMetrics(*EditorState, VisibleLayout, SplitPaneHeight, MaxOutlinerWidth);
    EditorState->TrackOutlinerWidth = PaneMetrics.TrackOutlinerWidth;
    ClampSequencerScrollState(*EditorState, PaneMetrics.MaxScrollY);

    ImGui::BeginChild("##AnimationSequenceSequencerSplit", ImVec2(0.0f, SplitPaneHeight), false, ImGuiWindowFlags_NoScrollbar);
    ImGui::BeginChild("##AnimationSequenceSequencerMainRow", ImVec2(0.0f, PaneMetrics.MainPaneHeight), false, ImGuiWindowFlags_NoScrollbar);
    RenderTrackOutlinerPane(PaneMetrics.TrackOutlinerWidth, PaneMetrics.MainPaneHeight, VisibleLayout);

    ImGui::SameLine(0.0f, 0.0f);
    HandleOutlinerSplitter(*EditorState, PaneMetrics.MainPaneHeight, MaxOutlinerWidth);

    ImGui::SameLine(0.0f, 0.0f);
    RenderTimelineCanvasPane(
        PaneMetrics.TimelinePaneWidth,
        PaneMetrics.MainPaneHeight,
        VisibleLayout);

    ImGui::EndChild();
    RenderBottomControlStrip(
        FAnimationSequenceSequencerLayout::BottomControlStripHeight,
        PaneMetrics.TrackOutlinerWidth,
        PaneMetrics.TimelinePaneWidth);
    ImGui::EndChild();
}

void FAnimationSequenceEditorWidget::RenderTrackOutlinerPane(
    float Width,
    float Height,
    const FAnimationSequenceSequencerVisibleLayout& VisibleLayout)
{
    const FTrackOutlinerPaneMetrics PaneMetrics = BuildTrackOutlinerPaneMetrics(Height);

    ImGui::BeginChild(
        "##AnimationSequenceTrackOutlinerPane",
        ImVec2(Width, Height),
        true);

    RenderTrackOutlinerFilterBar(TrackFilterEditBuffer, PaneMetrics);
    RenderTrackOutlinerList(
        TrackOutlinerWidget,
        *EditorState,
        Document,
        VisibleLayout,
        PaneMetrics);

    ImGui::EndChild();
}

void FAnimationSequenceEditorWidget::RenderTimelineCanvasPane(
    float Width,
    float Height,
    const FAnimationSequenceSequencerVisibleLayout& VisibleLayout)
{
    ImGui::BeginChild(
        "##AnimationSequenceTimelineCanvasPane",
        ImVec2(Width, Height),
        true);

    ImGui::BeginChild(
        "##AnimationSequenceTimelineCanvasViewport",
        ImVec2(0.0f, 0.0f),
        false,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    const FTimelineCanvasContext CanvasContext =
        BuildTimelineCanvasContext(TimelineWidget, *EditorState, VisibleLayout, Height);
    PrepareTimelineCanvasInteractionState(*EditorState, CanvasContext, VisibleLayout);
    DrawTimelineRowBackgrounds(
        CanvasContext.Geometry,
        VisibleLayout,
        CanvasContext.TimelineRowOriginY);
    RenderTimelineCanvasLayers(
        TimelineWidget,
        NotifyLaneWidget,
        CurveTrackWidget,
        *EditorState,
        Document,
        PreviewController,
        CanvasContext,
        VisibleLayout);

    ImGui::EndChild();
    ImGui::EndChild();
}

void FAnimationSequenceEditorWidget::DrawTimelineRowBackgrounds(
    const FAnimationSequenceTimelineGeometry& Geometry,
    const FAnimationSequenceSequencerVisibleLayout& VisibleLayout,
    float TimelineRowOriginY) const
{
    const float ClipBottom = Geometry.CanvasEnd.y - FAnimationSequenceTimelineGeometry::VerticalPadding;
    ImDrawList* DrawList = ImGui::GetWindowDrawList();
    const ImU32 SeparatorColor = IM_COL32(255, 255, 255, 18);
    const ImU32 HeaderSeparatorColor = IM_COL32(255, 255, 255, 26);
    const ImU32 HeaderFillColor = IM_COL32(255, 255, 255, 10);
    const ImU32 HoverFillColor = IM_COL32(255, 255, 255, 14);
    const ImU32 FocusFillColor = IM_COL32(78, 116, 168, 64);

    DrawList->PushClipRect(
        ImVec2(Geometry.TimelineMinX, Geometry.TrackTop),
        ImVec2(Geometry.TimelineMaxX, ClipBottom),
        true);

    bool bDrewFirstBoundary = false;
    for (const FAnimationSequenceSequencerVisibleRow& Row : VisibleLayout.Rows)
    {
        const float RowTop = TimelineRowOriginY + Row.OffsetY;
        const float RowBottom = RowTop + Row.Height;
        if (RowBottom < Geometry.TrackTop || RowTop > ClipBottom)
        {
            continue;
        }

        const bool bHovered = EditorState && EditorState->IsHoveredSequencerRow(Row.Id);
        const bool bFocused = EditorState && EditorState->IsFocusedSequencerRow(Row.Id);
        const bool bHeader = IsAnimationSequenceSectionHeaderRow(Row.Type);
        ImU32 FillColor = 0;
        if (bFocused)
        {
            FillColor = FocusFillColor;
        }
        else if (bHovered)
        {
            FillColor = HoverFillColor;
        }
        else if (bHeader)
        {
            FillColor = HeaderFillColor;
        }

        if (FillColor != 0)
        {
            DrawList->AddRectFilled(
                ImVec2(Geometry.TimelineMinX, RowTop),
                ImVec2(Geometry.TimelineMaxX, RowBottom),
                FillColor);
        }

        const ImU32 TopColor = IsAnimationSequenceSectionHeaderRow(Row.Type) ? HeaderSeparatorColor : SeparatorColor;
        if (!bDrewFirstBoundary)
        {
            DrawList->AddLine(
                ImVec2(Geometry.TimelineMinX, RowTop),
                ImVec2(Geometry.TimelineMaxX, RowTop),
                TopColor);
            bDrewFirstBoundary = true;
        }

        DrawList->AddLine(
            ImVec2(Geometry.TimelineMinX, RowBottom),
            ImVec2(Geometry.TimelineMaxX, RowBottom),
            TopColor);
    }

    DrawList->PopClipRect();
}
