#pragma once

#include "Core/CoreMinimal.h"

#include <algorithm>

struct FAnimationSequenceSequencerLayout
{
    static constexpr float PreviewMinHeight = 200.0f;
    static constexpr float SequencerMinHeight = 260.0f;
    static constexpr float SectionSplitterHeight = 6.0f;

    static constexpr float BottomControlStripHeight = 40.0f;
    static constexpr float DetailsPanelHeight = 156.0f;
    static constexpr float DetailsPanelMinHeight = 120.0f;
    static constexpr float RulerHeight = 24.0f;
    static constexpr float TrackAreaPadding = 8.0f;
    static constexpr float SectionHeaderHeight = 24.0f;
    static constexpr float SectionGap = 10.0f;
    static constexpr float EmptySectionRowHeight = 22.0f;

    static constexpr float NotifyTrackRowHeight = 24.0f;
    static constexpr float NotifyTrackRowSpacing = 4.0f;

    static constexpr float CurveTrackRowHeight = 48.0f;
    static constexpr float CurveTrackRowSpacing = 6.0f;
    static constexpr float CurveGroupHeaderHeight = 22.0f;
    static constexpr float CurveGroupHeaderSpacing = 4.0f;

    static constexpr float OutlinerMinWidth = 190.0f;
    static constexpr float OutlinerDefaultWidth = 260.0f;
    static constexpr float OutlinerMaxWidth = 420.0f;
    static constexpr float OutlinerSplitterWidth = 6.0f;

    static float GetNotifySectionHeight(int32 TrackCount, bool bExpanded)
    {
        const int32 SafeTrackCount = std::max(TrackCount, 0);
        float Height = SectionHeaderHeight;
        if (!bExpanded)
        {
            return Height;
        }

        Height += TrackAreaPadding;
        if (SafeTrackCount > 0)
        {
            Height += NotifyTrackRowHeight * SafeTrackCount;
            Height += NotifyTrackRowSpacing * std::max(0, SafeTrackCount - 1);
        }
        Height += TrackAreaPadding;
        return Height;
    }

    static float GetCurveSectionHeight(int32 CurveCount, bool bExpanded)
    {
        const int32 SafeCurveCount = std::max(CurveCount, 0);
        float Height = SectionHeaderHeight;
        if (!bExpanded)
        {
            return Height;
        }

        Height += TrackAreaPadding;
        if (SafeCurveCount > 0)
        {
            Height += CurveTrackRowHeight * SafeCurveCount;
            Height += CurveTrackRowSpacing * std::max(0, SafeCurveCount - 1);
        }
        Height += TrackAreaPadding;
        return Height;
    }

    static float GetPlaceholderSectionHeight(bool bExpanded)
    {
        float Height = SectionHeaderHeight;
        if (!bExpanded)
        {
            return Height;
        }

        Height += TrackAreaPadding;
        Height += EmptySectionRowHeight;
        Height += TrackAreaPadding;
        return Height;
    }

    static float GetTrackBodyHeight(int32 NotifyTrackCount, int32 CurveCount, bool bNotifiesExpanded, bool bCurvesExpanded)
    {
        return
            GetNotifySectionHeight(NotifyTrackCount, bNotifiesExpanded) +
            SectionGap +
            GetCurveSectionHeight(CurveCount, bCurvesExpanded);
    }
};
