#pragma once

#include "Core/CoreMinimal.h"

class UAnimInstanceAsset;

enum class EAnimInstanceStateContextSeverity : uint8
{
    Info,
    Warning,
};

struct FAnimInstanceStateSequenceOverview
{
    FString DisplayName;
    FString NormalizedPath;
    FString ProjectRelativePath;
    FString SkeletonAssetPath;
    int32 NumberOfFrames = 0;
    int32 FrameRateNumerator = 0;
    int32 FrameRateDenominator = 0;
    int32 CurveCount = 0;
    float DurationSeconds = 0.0f;
    bool bMetadataAvailable = false;
    bool bSequenceLoaded = false;
};

struct FAnimInstanceStateNotifySummary
{
    int32 TrackCount = 0;
    int32 TotalNotifyCount = 0;
    int32 OneShotNotifyCount = 0;
    int32 NotifyStateCount = 0;
};

struct FAnimInstanceStateNotifyHighlight
{
    float Time = 0.0f;
    FString TrackName;
    FString NotifyName;
    FString NotifyType;
    FString NotifyClassName;
};

struct FAnimInstanceStateBuiltInNotifyUsage
{
    FString DisplayName;
    FString NotifyClassName;
    int32 Count = 0;
    TArray<FString> Samples;
};

struct FAnimInstanceStateAuthoringHint
{
    EAnimInstanceStateContextSeverity Severity = EAnimInstanceStateContextSeverity::Info;
    FString Message;
};

struct FAnimInstanceStateSequenceContextReport
{
    bool bHasSequencePath = false;
    bool bLooksLikeSequencePath = false;
    bool bSequenceExistsOnDisk = false;
    bool bSequenceContentLoaded = false;

    FAnimInstanceStateSequenceOverview Overview;
    FAnimInstanceStateNotifySummary NotifySummary;
    TArray<FAnimInstanceStateNotifyHighlight> NotifyHighlights;
    TArray<FAnimInstanceStateBuiltInNotifyUsage> BuiltInUsage;
    TArray<FAnimInstanceStateAuthoringHint> Hints;
};

FAnimInstanceStateSequenceContextReport BuildAnimInstanceStateSequenceContext(
    const UAnimInstanceAsset* Asset,
    int32 StateIndex,
    int32 MaxNotifyHighlights = 6,
    int32 MaxBuiltInUsageSamples = 3);
