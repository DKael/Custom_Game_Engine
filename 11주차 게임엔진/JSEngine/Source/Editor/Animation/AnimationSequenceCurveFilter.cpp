#include "Editor/Animation/AnimationSequenceCurveFilter.h"

#include "Animation/AnimData/AnimDataModel.h"
#include "Animation/AnimData/AnimSequence.h"
#include "Editor/Animation/AnimationSequenceEditorState.h"
#include "Editor/Animation/AnimationSequenceSequencerLayout.h"
#include "Editor/Animation/AnimationSequenceViewerUtils.h"
#include "Engine/Asset/CurveFloatAsset.h"

#include <array>
#include <algorithm>

namespace
{
    constexpr std::array<EAnimCurveType, 4> CurveDisplayOrder =
    {
        EAnimCurveType::MorphTarget,
        EAnimCurveType::Material,
        EAnimCurveType::Attribute,
        EAnimCurveType::Unknown,
    };
}

FString AnimationSequenceCurveFilter::GetCurveGroupLabel(EAnimCurveType CurveType)
{
    switch (CurveType)
    {
    case EAnimCurveType::MorphTarget:
        return "Morph Target Curves";
    case EAnimCurveType::Material:
        return "Material Curves";
    case EAnimCurveType::Attribute:
        return "Attribute Curves";
    default:
        return "Other Curves";
    }
}

FString AnimationSequenceCurveFilter::GetCurveTypeBadge(EAnimCurveType CurveType)
{
    switch (CurveType)
    {
    case EAnimCurveType::MorphTarget:
        return "Morph";
    case EAnimCurveType::Material:
        return "Material";
    case EAnimCurveType::Attribute:
        return "Attr";
    default:
        return "Other";
    }
}

bool AnimationSequenceCurveFilter::IsCurveTypeEnabled(
    const FAnimationSequenceEditorState& State,
    EAnimCurveType CurveType)
{
    switch (CurveType)
    {
    case EAnimCurveType::MorphTarget:
        return State.bShowMorphTargetCurves;
    case EAnimCurveType::Material:
        return State.bShowMaterialCurves;
    case EAnimCurveType::Attribute:
        return State.bShowAttributeCurves;
    default:
        return State.bShowUnknownCurves;
    }
}

void AnimationSequenceCurveFilter::SetCurveTypeEnabled(
    FAnimationSequenceEditorState& State,
    EAnimCurveType CurveType,
    bool bEnabled)
{
    switch (CurveType)
    {
    case EAnimCurveType::MorphTarget:
        State.bShowMorphTargetCurves = bEnabled;
        break;
    case EAnimCurveType::Material:
        State.bShowMaterialCurves = bEnabled;
        break;
    case EAnimCurveType::Attribute:
        State.bShowAttributeCurves = bEnabled;
        break;
    default:
        State.bShowUnknownCurves = bEnabled;
        break;
    }
}

void AnimationSequenceCurveFilter::SetAllCurveTypesEnabled(FAnimationSequenceEditorState& State, bool bEnabled)
{
    State.bShowMorphTargetCurves = bEnabled;
    State.bShowMaterialCurves = bEnabled;
    State.bShowAttributeCurves = bEnabled;
    State.bShowUnknownCurves = bEnabled;
}

void AnimationSequenceCurveFilter::SetExclusiveCurveTypeVisible(FAnimationSequenceEditorState& State, EAnimCurveType CurveType)
{
    SetAllCurveTypesEnabled(State, false);
    SetCurveTypeEnabled(State, CurveType, true);
}

TArray<FAnimationSequenceCurveViewGroup> AnimationSequenceCurveFilter::BuildCurveViewGroups(
    const UAnimSequence* Sequence,
    const FAnimationSequenceEditorState& State)
{
    TArray<FAnimationSequenceCurveViewGroup> Groups;

    const UAnimDataModel* DataModel = AnimationSequenceViewer::GetValidAnimDataModel(Sequence);
    if (!DataModel)
    {
        return Groups;
    }

    for (const EAnimCurveType CurveType : CurveDisplayOrder)
    {
        FAnimationSequenceCurveViewGroup Group;
        Group.CurveType = CurveType;
        Group.Label = GetCurveGroupLabel(CurveType);
        Group.bVisible = IsCurveTypeEnabled(State, CurveType);

        for (int32 CurveIndex = 0; CurveIndex < static_cast<int32>(DataModel->CurveData.FloatCurves.size()); ++CurveIndex)
        {
            const FFloatCurve& Curve = DataModel->CurveData.FloatCurves[CurveIndex];
            if (Curve.CurveType != CurveType)
            {
                continue;
            }

            ++Group.TotalCount;
            if (Group.bVisible)
            {
                Group.VisibleEntries.push_back({ CurveIndex, &Curve });
            }
        }

        if (Group.TotalCount > 0)
        {
            Groups.push_back(Group);
        }
    }

    return Groups;
}

TArray<FAnimationSequenceCurveViewEntry> AnimationSequenceCurveFilter::BuildVisibleCurveEntries(
    const UAnimSequence* Sequence,
    const FAnimationSequenceEditorState& State)
{
    TArray<FAnimationSequenceCurveViewEntry> VisibleEntries;
    const TArray<FAnimationSequenceCurveViewGroup> Groups = BuildCurveViewGroups(Sequence, State);
    for (const FAnimationSequenceCurveViewGroup& Group : Groups)
    {
        if (!Group.bVisible)
        {
            continue;
        }

        for (const FAnimationSequenceCurveViewEntry& Entry : Group.VisibleEntries)
        {
            VisibleEntries.push_back(Entry);
        }
    }

    return VisibleEntries;
}

int32 AnimationSequenceCurveFilter::GetVisibleCurveCount(
    const UAnimSequence* Sequence,
    const FAnimationSequenceEditorState& State)
{
    int32 VisibleCount = 0;
    const TArray<FAnimationSequenceCurveViewGroup> Groups = BuildCurveViewGroups(Sequence, State);
    for (const FAnimationSequenceCurveViewGroup& Group : Groups)
    {
        if (Group.bVisible)
        {
            VisibleCount += static_cast<int32>(Group.VisibleEntries.size());
        }
    }

    return VisibleCount;
}

float AnimationSequenceCurveFilter::GetCurveSectionHeight(
    const TArray<FAnimationSequenceCurveViewGroup>& Groups,
    bool bExpanded)
{
    float Height = FAnimationSequenceSequencerLayout::SectionHeaderHeight;
    if (!bExpanded)
    {
        return Height;
    }

    Height += FAnimationSequenceSequencerLayout::TrackAreaPadding;

    for (int32 GroupIndex = 0; GroupIndex < static_cast<int32>(Groups.size()); ++GroupIndex)
    {
        const FAnimationSequenceCurveViewGroup& Group = Groups[GroupIndex];
        Height += FAnimationSequenceSequencerLayout::CurveGroupHeaderHeight;

        if (Group.bVisible && !Group.VisibleEntries.empty())
        {
            Height += FAnimationSequenceSequencerLayout::CurveGroupHeaderSpacing;
            Height +=
                FAnimationSequenceSequencerLayout::CurveTrackRowHeight * static_cast<float>(Group.VisibleEntries.size()) +
                FAnimationSequenceSequencerLayout::CurveTrackRowSpacing * static_cast<float>(std::max(0, static_cast<int32>(Group.VisibleEntries.size()) - 1));
        }

        if (GroupIndex + 1 < static_cast<int32>(Groups.size()))
        {
            Height += FAnimationSequenceSequencerLayout::CurveGroupHeaderSpacing;
        }
    }

    Height += FAnimationSequenceSequencerLayout::TrackAreaPadding;
    return Height;
}

float AnimationSequenceCurveFilter::GetCurveSectionHeight(
    const UAnimSequence* Sequence,
    const FAnimationSequenceEditorState& State)
{
    const TArray<FAnimationSequenceCurveViewGroup> Groups = BuildCurveViewGroups(Sequence, State);
    return GetCurveSectionHeight(Groups, State.bCurvesExpanded);
}
