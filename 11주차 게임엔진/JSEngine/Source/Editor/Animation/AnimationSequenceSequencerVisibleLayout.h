#pragma once

#include "Animation/AnimData/AnimCurveTypes.h"
#include "Core/CoreMinimal.h"

class FFloatCurve;

enum class EAnimationSequenceSequencerVisibleRowType : uint8
{
    NotifyHeader,
    NotifyTrack,
    CurveHeader,
    CurveGroup,
    CurveEntry,
    AdditiveHeader,
    AdditiveEmpty,
    AttributeHeader,
    AttributeGroup,
    AttributeEntry,
    AttributeEmpty,
};

struct FAnimationSequenceSequencerRowId
{
    bool bValid = false;
    EAnimationSequenceSequencerVisibleRowType Type = EAnimationSequenceSequencerVisibleRowType::NotifyHeader;
    int32 SourceIndex = -1;
    int32 GroupIndex = -1;

    bool IsValid() const
    {
        return bValid;
    }

    bool operator==(const FAnimationSequenceSequencerRowId& Other) const
    {
        return
            bValid == Other.bValid &&
            Type == Other.Type &&
            SourceIndex == Other.SourceIndex &&
            GroupIndex == Other.GroupIndex;
    }

    bool operator!=(const FAnimationSequenceSequencerRowId& Other) const
    {
        return !(*this == Other);
    }
};

struct FAnimationSequenceSequencerVisibleRow
{
    FAnimationSequenceSequencerRowId Id;
    EAnimationSequenceSequencerVisibleRowType Type = EAnimationSequenceSequencerVisibleRowType::NotifyHeader;
    int32 SourceIndex = -1;
    int32 GroupIndex = -1;
    float OffsetY = 0.0f;
    float Height = 0.0f;
    FString Label;
    FString SecondaryLabel;
    bool bCanExpand = false;
    bool bExpanded = false;
    bool bSelected = false;
    bool bToggleChecked = false;
    EAnimCurveType CurveType = EAnimCurveType::Unknown;
    const FFloatCurve* Curve = nullptr;
};

struct FAnimationSequenceSequencerVisibleLayout
{
    float ContentHeight = 0.0f;
    TArray<FAnimationSequenceSequencerVisibleRow> Rows;
};

inline bool IsAnimationSequenceSectionHeaderRow(EAnimationSequenceSequencerVisibleRowType Type)
{
    switch (Type)
    {
    case EAnimationSequenceSequencerVisibleRowType::NotifyHeader:
    case EAnimationSequenceSequencerVisibleRowType::CurveHeader:
    case EAnimationSequenceSequencerVisibleRowType::AdditiveHeader:
    case EAnimationSequenceSequencerVisibleRowType::AttributeHeader:
        return true;
    default:
        return false;
    }
}

inline bool IsAnimationSequenceCurveEntryRow(EAnimationSequenceSequencerVisibleRowType Type)
{
    return
        Type == EAnimationSequenceSequencerVisibleRowType::CurveEntry ||
        Type == EAnimationSequenceSequencerVisibleRowType::AttributeEntry;
}

inline FAnimationSequenceSequencerRowId MakeAnimationSequenceSequencerRowId(
    EAnimationSequenceSequencerVisibleRowType Type,
    int32 SourceIndex = -1,
    int32 GroupIndex = -1)
{
    FAnimationSequenceSequencerRowId RowId;
    RowId.bValid = true;
    RowId.Type = Type;
    RowId.SourceIndex = SourceIndex;
    RowId.GroupIndex = GroupIndex;
    return RowId;
}
