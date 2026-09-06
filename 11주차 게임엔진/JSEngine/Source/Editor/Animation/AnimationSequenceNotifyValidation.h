#pragma once

#include "Animation/AnimData/AnimNotifyTypes.h"
#include "Core/CoreMinimal.h"

class UAnimSequence;
class FAnimationSequencePreviewController;

enum class EAnimNotifyValidationSeverity : uint8
{
    Info = 0,
    Warning,
    Error,
};

enum class EAnimNotifyValidationField : uint8
{
    General = 0,
    Type,
    NotifyClass,
    Payload,
    Time,
    Duration,
    Name,
};

struct FAnimNotifyValidationIssue
{
    EAnimNotifyValidationSeverity Severity = EAnimNotifyValidationSeverity::Info;
    EAnimNotifyValidationField Field = EAnimNotifyValidationField::General;
    FString Message;
    FString Hint;
    int32 TrackIndex = -1;
    int32 EventIndex = -1;
    FGuid StableId;
};

struct FAnimNotifyValidationReport
{
    TArray<FAnimNotifyValidationIssue> Issues;
    int32 InfoCount = 0;
    int32 WarningCount = 0;
    int32 ErrorCount = 0;

    void AddIssue(const FAnimNotifyValidationIssue& Issue);
    void AddIssue(
        EAnimNotifyValidationSeverity Severity,
        EAnimNotifyValidationField Field,
        const FString& Message,
        const FString& Hint = FString(),
        int32 TrackIndex = -1,
        int32 EventIndex = -1,
        const FGuid& StableId = FGuid());
    void Append(const FAnimNotifyValidationReport& Other);

    int32 GetCount(EAnimNotifyValidationSeverity Severity) const;
    bool HasAnyIssues() const;
    bool HasIssuesForField(EAnimNotifyValidationField Field) const;
    bool HasIssuesOfSeverity(EAnimNotifyValidationSeverity Severity) const;
};

struct FAnimNotifyValidationContext
{
    const FAnimationSequencePreviewController* PreviewController = nullptr;
};

FAnimNotifyValidationReport ValidateAnimNotifyEvent(
    const FAnimNotifyEvent& NotifyEvent,
    int32 TrackIndex,
    int32 EventIndex,
    const FAnimNotifyValidationContext& Context);

FAnimNotifyValidationReport ValidateAnimNotifyDocument(
    const UAnimSequence* Sequence,
    const FAnimNotifyValidationContext& Context);

FString FormatAnimNotifyValidationSummary(const FAnimNotifyValidationReport& Report);
