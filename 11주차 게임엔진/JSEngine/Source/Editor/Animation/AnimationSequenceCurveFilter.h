#pragma once

#include "Animation/AnimData/AnimCurveTypes.h"
#include "Core/CoreMinimal.h"

class UAnimSequence;
class FAnimationSequenceEditorState;
class FFloatCurve;

struct FAnimationSequenceCurveViewEntry
{
    int32 SourceIndex = -1;
    const FFloatCurve* Curve = nullptr;
};

struct FAnimationSequenceCurveViewGroup
{
    EAnimCurveType CurveType = EAnimCurveType::Unknown;
    FString Label;
    int32 TotalCount = 0;
    bool bVisible = false;
    TArray<FAnimationSequenceCurveViewEntry> VisibleEntries;
};

namespace AnimationSequenceCurveFilter
{
    FString GetCurveGroupLabel(EAnimCurveType CurveType);
    FString GetCurveTypeBadge(EAnimCurveType CurveType);

    bool IsCurveTypeEnabled(const FAnimationSequenceEditorState& State, EAnimCurveType CurveType);
    void SetCurveTypeEnabled(FAnimationSequenceEditorState& State, EAnimCurveType CurveType, bool bEnabled);
    void SetAllCurveTypesEnabled(FAnimationSequenceEditorState& State, bool bEnabled);
    void SetExclusiveCurveTypeVisible(FAnimationSequenceEditorState& State, EAnimCurveType CurveType);

    TArray<FAnimationSequenceCurveViewGroup> BuildCurveViewGroups(
        const UAnimSequence* Sequence,
        const FAnimationSequenceEditorState& State);
    TArray<FAnimationSequenceCurveViewEntry> BuildVisibleCurveEntries(
        const UAnimSequence* Sequence,
        const FAnimationSequenceEditorState& State);

    int32 GetVisibleCurveCount(const UAnimSequence* Sequence, const FAnimationSequenceEditorState& State);
    float GetCurveSectionHeight(const TArray<FAnimationSequenceCurveViewGroup>& Groups, bool bExpanded);
    float GetCurveSectionHeight(const UAnimSequence* Sequence, const FAnimationSequenceEditorState& State);
}
