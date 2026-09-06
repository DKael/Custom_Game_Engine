#include "Editor/Animation/AnimationSequenceCurveTrackWidget.h"

#include "Editor/Animation/AnimationSequenceCurveFilter.h"
#include "Editor/Animation/AnimationSequenceEditorState.h"
#include "Editor/Animation/AnimationSequenceSequencerLayout.h"
#include "Editor/Animation/AnimationSequenceSequencerVisibleLayout.h"
#include "Editor/Animation/AnimationSequenceTimelineGeometry.h"
#include "Engine/Asset/CurveFloatAsset.h"

#include "ImGui/imgui.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <string>

namespace
{
    struct FCurveRange
    {
        float MinValue = 0.0f;
        float MaxValue = 1.0f;
    };

    FCurveRange ComputeCurveRange(const FFloatCurve& Curve, float StartTime, float EndTime)
    {
        FCurveRange Range;
        Range.MinValue = FLT_MAX;
        Range.MaxValue = -FLT_MAX;

        constexpr int32 SampleCount = 64;
        const float SafeStart = std::min(StartTime, EndTime);
        const float SafeEnd = std::max(StartTime, EndTime);
        for (int32 Index = 0; Index < SampleCount; ++Index)
        {
            const float Alpha = SampleCount > 1 ? static_cast<float>(Index) / static_cast<float>(SampleCount - 1) : 0.0f;
            const float Time = SafeStart + (SafeEnd - SafeStart) * Alpha;
            const float Value = Curve.Evaluate(Time);
            Range.MinValue = std::min(Range.MinValue, Value);
            Range.MaxValue = std::max(Range.MaxValue, Value);
        }

        if (Range.MinValue == FLT_MAX || Range.MaxValue == -FLT_MAX)
        {
            Range.MinValue = 0.0f;
            Range.MaxValue = 1.0f;
        }
        else if (std::abs(Range.MaxValue - Range.MinValue) < 0.0001f)
        {
            Range.MinValue -= 0.5f;
            Range.MaxValue += 0.5f;
        }

        return Range;
    }

    ImU32 GetCurveColor(EAnimCurveType CurveType, bool bSelected)
    {
        ImU32 BaseColor = IM_COL32(144, 195, 255, 255);
        switch (CurveType)
        {
        case EAnimCurveType::MorphTarget:
            BaseColor = IM_COL32(109, 233, 209, 255);
            break;
        case EAnimCurveType::Material:
            BaseColor = IM_COL32(243, 213, 92, 255);
            break;
        case EAnimCurveType::Attribute:
            BaseColor = IM_COL32(123, 176, 255, 255);
            break;
        default:
            BaseColor = IM_COL32(176, 148, 255, 255);
            break;
        }

        return bSelected ? IM_COL32(255, 255, 255, 255) : BaseColor;
    }

    float SampleCurveY(
        const FFloatCurve& Curve,
        const FAnimationSequenceEditorState& State,
        const FCurveRange& Range,
        float Time,
        float RowTop,
        float RowBottom)
    {
        const float Value = Curve.Evaluate(Time);
        const float RangeSize = std::max(Range.MaxValue - Range.MinValue, 0.0001f);
        const float Alpha = (Value - Range.MinValue) / RangeSize;
        return std::clamp(RowBottom - Alpha * (RowBottom - RowTop), RowTop, RowBottom);
    }

    int32 FindNearestCurveKeyIndex(const FFloatCurve& Curve, float Time)
    {
        int32 BestIndex = -1;
        float BestDistance = FLT_MAX;
        for (int32 KeyIndex = 0; KeyIndex < static_cast<int32>(Curve.Keys.size()); ++KeyIndex)
        {
            const float Distance = std::fabs(Curve.Keys[KeyIndex].Time - Time);
            if (Distance < BestDistance)
            {
                BestDistance = Distance;
                BestIndex = KeyIndex;
            }
        }

        return BestIndex;
    }
}

void FAnimationSequenceCurveTrackWidget::RenderRows(
    FAnimationSequenceEditorState& State,
    const FAnimationSequenceTimelineGeometry& Geometry,
    const FAnimationSequenceSequencerVisibleLayout& VisibleLayout,
    float TimelineRowOriginY) const
{
    ImDrawList* DrawList = ImGui::GetWindowDrawList();

    for (const FAnimationSequenceSequencerVisibleRow& Row : VisibleLayout.Rows)
    {
        if (!IsAnimationSequenceCurveEntryRow(Row.Type) || !Row.Curve)
        {
            continue;
        }

        const float RowTop = TimelineRowOriginY + Row.OffsetY;
        const float RowBottom = RowTop + Row.Height;
        const bool bSelected = State.SelectedCurveIndex == Row.SourceIndex;

        const ImVec2 RowMin(Geometry.TimelineMinX, RowTop);
        const ImVec2 RowMax(Geometry.TimelineMaxX, RowBottom);

        ImGui::SetCursorScreenPos(RowMin);
        ImGui::PushID(Row.SourceIndex + 7000);
        ImGui::InvisibleButton("##CurveRow", ImVec2(RowMax.x - RowMin.x, RowMax.y - RowMin.y));
        if (ImGui::IsItemHovered())
        {
            State.HoveredCurveIndex = Row.SourceIndex;
            State.SetHoveredSequencerRow(Row.Id);

            const float HoveredTime = Geometry.XToTime(State, Geometry.ClampX(ImGui::GetIO().MousePos.x));
            State.bHasHoveredCurveSample = true;
            State.HoveredCurveSampleTime = HoveredTime;
            State.HoveredCurveSampleValue = Row.Curve->Evaluate(HoveredTime);
            State.HoveredCurveNearestKeyIndex = FindNearestCurveKeyIndex(*Row.Curve, HoveredTime);

            const FString CurveName = Row.Curve->CurveName.IsValid() ? Row.Curve->CurveName.ToString() : FString("(Unnamed Curve)");
            if (ImGui::BeginTooltip())
            {
                ImGui::TextUnformatted(CurveName.c_str());
                ImGui::Separator();
                ImGui::Text("Time %.3fs", HoveredTime);
                ImGui::Text("Value %.3f", State.HoveredCurveSampleValue);
                if (State.HoveredCurveNearestKeyIndex >= 0 &&
                    State.HoveredCurveNearestKeyIndex < static_cast<int32>(Row.Curve->Keys.size()))
                {
                    const FCurveKey& NearestKey = Row.Curve->Keys[State.HoveredCurveNearestKeyIndex];
                    ImGui::Text(
                        "Key %d  %.3fs  %.3f",
                        State.HoveredCurveNearestKeyIndex,
                        NearestKey.Time,
                        NearestKey.Value);
                }
                ImGui::EndTooltip();
            }
        }
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
        {
            State.SetFocusedSequencerRow(Row.Id);
            State.SelectedCurveIndex = Row.SourceIndex;
        }
        ImGui::PopID();

        const FCurveRange ValueRange = ComputeCurveRange(*Row.Curve, State.VisibleTimeStart, State.VisibleTimeEnd);
        constexpr int32 SampleCount = 96;
        ImVec2 PreviousPoint = {};
        bool bHasPreviousPoint = false;

        for (int32 SampleIndex = 0; SampleIndex < SampleCount; ++SampleIndex)
        {
            const float Alpha = SampleCount > 1 ? static_cast<float>(SampleIndex) / static_cast<float>(SampleCount - 1) : 0.0f;
            const float Time = State.VisibleTimeStart + State.GetVisibleRange() * Alpha;
            const float X = Geometry.TimeToX(State, Time);
            const float Y = SampleCurveY(*Row.Curve, State, ValueRange, Time, RowTop + 6.0f, RowBottom - 6.0f);
            const ImVec2 Point(X, Y);
            if (bHasPreviousPoint)
            {
                DrawList->AddLine(PreviousPoint, Point, GetCurveColor(Row.CurveType, bSelected), bSelected ? 2.4f : 1.8f);
            }

            PreviousPoint = Point;
            bHasPreviousPoint = true;
        }

        const FString KeyLabel =
            AnimationSequenceCurveFilter::GetCurveTypeBadge(Row.CurveType) +
            "  |  " +
            std::to_string(static_cast<int32>(Row.Curve->Keys.size())) +
            " keys";
        DrawList->AddText(
            ImVec2(Geometry.TimelineMaxX - 150.0f, RowTop + 4.0f),
            IM_COL32(140, 149, 163, 255),
            KeyLabel.c_str());
    }
}
