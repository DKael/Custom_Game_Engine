#include "Editor/Animation/AnimationSequenceEditorWidget.h"

#include "Animation/AnimData/AnimDataModel.h"
#include "Animation/AnimData/AnimSequence.h"
#include "Editor/Animation/AnimationSequenceCurveFilter.h"
#include "Editor/Animation/AnimationSequenceEditorState.h"
#include "Editor/Animation/AnimationSequenceViewerUtils.h"

#include "ImGui/imgui.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
    int32 GetVisibleCurveCount(const TArray<FAnimationSequenceCurveViewGroup>& CurveGroups)
    {
        int32 VisibleCount = 0;
        for (const FAnimationSequenceCurveViewGroup& Group : CurveGroups)
        {
            if (Group.bVisible)
            {
                VisibleCount += static_cast<int32>(Group.VisibleEntries.size());
            }
        }

        return VisibleCount;
    }

    const FFloatCurve* GetSelectedCurve(const UAnimSequence* Sequence, const FAnimationSequenceEditorState* State)
    {
        if (!State)
        {
            return nullptr;
        }

        const UAnimDataModel* DataModel = AnimationSequenceViewer::GetValidAnimDataModel(Sequence);
        if (!DataModel ||
            State->SelectedCurveIndex < 0 ||
            State->SelectedCurveIndex >= static_cast<int32>(DataModel->CurveData.FloatCurves.size()))
        {
            return nullptr;
        }

        return &DataModel->CurveData.FloatCurves[State->SelectedCurveIndex];
    }

    struct FCurveValueRange
    {
        bool bValid = false;
        float MinValue = 0.0f;
        float MaxValue = 0.0f;
    };

    struct FCurveKeySummary
    {
        bool bValid = false;
        int32 KeyIndex = -1;
        float Time = 0.0f;
        float Value = 0.0f;
    };

    struct FCurveInspectionSnapshot
    {
        float CurrentValue = 0.0f;
        FCurveValueRange FullValueRange;
        FCurveValueRange VisibleValueRange;
        FCurveKeySummary FirstKey;
        FCurveKeySummary LastKey;
        FCurveKeySummary NearestKey;
    };

    struct FCurveInspectionContext
    {
        const UAnimDataModel* DataModel = nullptr;
        const FFloatCurve* SelectedCurve = nullptr;
        TArray<FAnimationSequenceCurveViewGroup> CurveGroups;
        int32 VisibleCurveCount = 0;
        bool bHasSnapshot = false;
        FCurveInspectionSnapshot Snapshot;
    };

    FCurveValueRange ComputeCurveValueRangeFromKeys(const FFloatCurve& Curve)
    {
        FCurveValueRange Range;
        if (Curve.Keys.empty())
        {
            return Range;
        }

        Range.bValid = true;
        Range.MinValue = Curve.Keys.front().Value;
        Range.MaxValue = Curve.Keys.front().Value;
        for (const FCurveKey& Key : Curve.Keys)
        {
            Range.MinValue = std::min(Range.MinValue, Key.Value);
            Range.MaxValue = std::max(Range.MaxValue, Key.Value);
        }
        return Range;
    }

    FCurveValueRange ComputeCurveValueRangeFromSampling(const FFloatCurve& Curve, float StartTime, float EndTime)
    {
        FCurveValueRange Range;
        constexpr int32 SampleCount = 64;
        const float SafeStart = std::min(StartTime, EndTime);
        const float SafeEnd = std::max(StartTime, EndTime);
        float MinValue = std::numeric_limits<float>::max();
        float MaxValue = -std::numeric_limits<float>::max();

        for (int32 SampleIndex = 0; SampleIndex < SampleCount; ++SampleIndex)
        {
            const float Alpha = SampleCount > 1 ? static_cast<float>(SampleIndex) / static_cast<float>(SampleCount - 1) : 0.0f;
            const float Time = SafeStart + (SafeEnd - SafeStart) * Alpha;
            const float Value = Curve.Evaluate(Time);
            MinValue = std::min(MinValue, Value);
            MaxValue = std::max(MaxValue, Value);
        }

        if (MinValue == std::numeric_limits<float>::max() || MaxValue == -std::numeric_limits<float>::max())
        {
            return Range;
        }

        Range.bValid = true;
        Range.MinValue = MinValue;
        Range.MaxValue = MaxValue;
        return Range;
    }

    FCurveKeySummary MakeCurveKeySummary(const FFloatCurve& Curve, int32 KeyIndex)
    {
        FCurveKeySummary Summary;
        if (KeyIndex < 0 || KeyIndex >= static_cast<int32>(Curve.Keys.size()))
        {
            return Summary;
        }

        Summary.bValid = true;
        Summary.KeyIndex = KeyIndex;
        Summary.Time = Curve.Keys[KeyIndex].Time;
        Summary.Value = Curve.Keys[KeyIndex].Value;
        return Summary;
    }

    FCurveKeySummary FindNearestCurveKey(const FFloatCurve& Curve, float Time)
    {
        int32 BestIndex = -1;
        float BestDistance = std::numeric_limits<float>::max();
        for (int32 KeyIndex = 0; KeyIndex < static_cast<int32>(Curve.Keys.size()); ++KeyIndex)
        {
            const float Distance = std::fabs(Curve.Keys[KeyIndex].Time - Time);
            if (Distance < BestDistance)
            {
                BestDistance = Distance;
                BestIndex = KeyIndex;
            }
        }

        return MakeCurveKeySummary(Curve, BestIndex);
    }

    FCurveInspectionSnapshot BuildCurveInspectionSnapshot(
        const FFloatCurve& Curve,
        const FAnimationSequenceEditorState& State)
    {
        FCurveInspectionSnapshot Snapshot;
        Snapshot.CurrentValue = Curve.Evaluate(State.CurrentTime);
        Snapshot.FullValueRange = ComputeCurveValueRangeFromKeys(Curve);
        Snapshot.VisibleValueRange = ComputeCurveValueRangeFromSampling(Curve, State.VisibleTimeStart, State.VisibleTimeEnd);
        Snapshot.FirstKey = MakeCurveKeySummary(Curve, Curve.Keys.empty() ? -1 : 0);
        Snapshot.LastKey = MakeCurveKeySummary(Curve, Curve.Keys.empty() ? -1 : static_cast<int32>(Curve.Keys.size()) - 1);
        Snapshot.NearestKey = FindNearestCurveKey(Curve, State.CurrentTime);
        return Snapshot;
    }

    FCurveInspectionContext BuildCurveInspectionContext(
        const UAnimSequence* Sequence,
        const FAnimationSequenceEditorState* State)
    {
        FCurveInspectionContext Context;
        Context.DataModel = AnimationSequenceViewer::GetValidAnimDataModel(Sequence);
        if (!Context.DataModel || Context.DataModel->CurveData.FloatCurves.empty())
        {
            return Context;
        }

        if (!State)
        {
            return Context;
        }

        Context.SelectedCurve = GetSelectedCurve(Sequence, State);
        if (!Context.SelectedCurve)
        {
            Context.CurveGroups = AnimationSequenceCurveFilter::BuildCurveViewGroups(Sequence, *State);
            Context.VisibleCurveCount = GetVisibleCurveCount(Context.CurveGroups);
            return Context;
        }

        Context.Snapshot = BuildCurveInspectionSnapshot(*Context.SelectedCurve, *State);
        Context.bHasSnapshot = true;
        return Context;
    }
}

void FAnimationSequenceEditorWidget::RenderCurveInspectionPanel()
{
    ImGui::Spacing();
    ImGui::TextDisabled("Selected Curve");

    const FCurveInspectionContext Context = BuildCurveInspectionContext(Sequence, EditorState);
    if (!Context.DataModel || Context.DataModel->CurveData.FloatCurves.empty())
    {
        ImGui::TextDisabled("No float curves in this sequence.");
        return;
    }

    if (!Context.SelectedCurve)
    {
        ImGui::TextDisabled(
            "%d visible curves / %d total. Select a curve from the outliner or graph row.",
            Context.VisibleCurveCount,
            static_cast<int32>(Context.DataModel->CurveData.FloatCurves.size()));
        for (const FAnimationSequenceCurveViewGroup& Group : Context.CurveGroups)
        {
            ImGui::BulletText(
                "%s: %d %s",
                Group.Label.c_str(),
                Group.TotalCount,
                Group.bVisible ? "shown" : "hidden");
        }
        return;
    }

    if (!EditorState)
    {
        ImGui::TextDisabled("Curve inspection state is unavailable.");
        return;
    }

    const FFloatCurve& SelectedCurve = *Context.SelectedCurve;
    const FCurveInspectionSnapshot& Snapshot = Context.Snapshot;

    const bool bCanFrameSelection = Snapshot.FirstKey.bValid && Snapshot.LastKey.bValid;
    if (!bCanFrameSelection) { ImGui::BeginDisabled(); }
    if (ImGui::Button("Frame Selection") && bCanFrameSelection)
    {
        float FrameStart = Snapshot.FirstKey.Time;
        float FrameEnd = Snapshot.LastKey.Time;
        const float MinimumVisibleRange = EditorState->GetMinimumVisibleRange();
        if (FrameEnd - FrameStart < MinimumVisibleRange)
        {
            const float CenterTime = (FrameStart + FrameEnd) * 0.5f;
            FrameStart = CenterTime - MinimumVisibleRange * 0.5f;
            FrameEnd = CenterTime + MinimumVisibleRange * 0.5f;
        }

        EditorState->SetVisibleRange(FrameStart, FrameEnd);
    }
    if (!bCanFrameSelection) { ImGui::EndDisabled(); }

    ImGui::SameLine();
    if (ImGui::Button("Solo Type"))
    {
        AnimationSequenceCurveFilter::SetExclusiveCurveTypeVisible(*EditorState, SelectedCurve.CurveType);
    }

    ImGui::SameLine();
    if (ImGui::Button("Show All"))
    {
        AnimationSequenceCurveFilter::SetAllCurveTypesEnabled(*EditorState, true);
    }

    ImGui::BulletText("%s", SelectedCurve.CurveName.ToString().c_str());
    ImGui::BulletText("Type %s", AnimationCurveTypeToString(SelectedCurve.CurveType).c_str());
    ImGui::BulletText("Source %s", AnimationCurveSourceKindToString(SelectedCurve.SourceKind).c_str());
    ImGui::BulletText("%d keys", static_cast<int32>(SelectedCurve.Keys.size()));
    ImGui::BulletText("Current Value %.3f @ %.3fs", Snapshot.CurrentValue, EditorState->CurrentTime);

    if (Snapshot.FirstKey.bValid && Snapshot.LastKey.bValid)
    {
        ImGui::BulletText("Time Range %.3fs - %.3fs", Snapshot.FirstKey.Time, Snapshot.LastKey.Time);
        ImGui::BulletText("First Key #%d  %.3fs  %.3f", Snapshot.FirstKey.KeyIndex, Snapshot.FirstKey.Time, Snapshot.FirstKey.Value);
        ImGui::BulletText("Last Key #%d  %.3fs  %.3f", Snapshot.LastKey.KeyIndex, Snapshot.LastKey.Time, Snapshot.LastKey.Value);
    }
    else
    {
        ImGui::TextDisabled("No curve keys.");
    }

    if (Snapshot.FullValueRange.bValid)
    {
        ImGui::BulletText("Value Range %.3f - %.3f", Snapshot.FullValueRange.MinValue, Snapshot.FullValueRange.MaxValue);
    }

    if (Snapshot.VisibleValueRange.bValid)
    {
        ImGui::BulletText(
            "Visible Value Range %.3f - %.3f",
            Snapshot.VisibleValueRange.MinValue,
            Snapshot.VisibleValueRange.MaxValue);
    }

    if (Snapshot.NearestKey.bValid)
    {
        ImGui::BulletText(
            "Nearest Key #%d  %.3fs  %.3f  (delta %.3fs)",
            Snapshot.NearestKey.KeyIndex,
            Snapshot.NearestKey.Time,
            Snapshot.NearestKey.Value,
            std::fabs(Snapshot.NearestKey.Time - EditorState->CurrentTime));
    }

    if (EditorState->bHasHoveredCurveSample && EditorState->HoveredCurveIndex == EditorState->SelectedCurveIndex)
    {
        ImGui::BulletText(
            "Hovered Sample %.3f @ %.3fs",
            EditorState->HoveredCurveSampleValue,
            EditorState->HoveredCurveSampleTime);
        if (EditorState->HoveredCurveNearestKeyIndex >= 0 &&
            EditorState->HoveredCurveNearestKeyIndex < static_cast<int32>(SelectedCurve.Keys.size()))
        {
            const FCurveKey& HoveredKey = SelectedCurve.Keys[EditorState->HoveredCurveNearestKeyIndex];
            ImGui::BulletText(
                "Hovered Nearest Key #%d  %.3fs  %.3f",
                EditorState->HoveredCurveNearestKeyIndex,
                HoveredKey.Time,
                HoveredKey.Value);
        }
    }
}
