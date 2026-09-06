#include "AnimSingleNodeInstance.h"

#include "Object/ObjectFactory.h"

#include <algorithm>

void UAnimSingleNodeInstance::AdvancePreviewPlayback(
    float InNewTime,
    float DeltaTime,
    bool bWrapped,
    float RangeStart,
    float RangeEnd)
{
    RecentNotifyEvents.clear();

    const auto ClampTimeForSequence = [this](float InTime)
    {
        const float Length = GetSequenceLength(CurrentSequence);
        return Length > 0.0f ? std::clamp(InTime, 0.0f, Length) : 0.0f;
    };

    if (!HasValidSequence())
    {
        CurrentTime = ClampTimeForSequence(InNewTime);
        NextTime = CurrentTime;
        ClearActiveAnimNotifyStates(false);
        return;
    }

    const float PreviousTime = CurrentTime;
    const float ClampedRangeStart = ClampTimeForSequence(RangeStart);
    const float ClampedRangeEnd = ClampTimeForSequence(RangeEnd);
    CurrentTime = ClampTimeForSequence(InNewTime);
    NextTime = CurrentTime;

    if (!bWrapped)
    {
        UpdateRecentNotifyEvents(CurrentSequence, PreviousTime, CurrentTime, DeltaTime);
        return;
    }

    if (PlayRate >= 0.0f)
    {
        UpdateRecentNotifyEvents(CurrentSequence, PreviousTime, ClampedRangeEnd, DeltaTime);
    }
    else
    {
        UpdateRecentNotifyEvents(CurrentSequence, PreviousTime, ClampedRangeStart, DeltaTime);
    }

    // Playback range wrapping is an editor-only seek across a subrange, not a
    // full sequence loop. Drop active state carries at the wrap boundary so
    // states outside the playback range do not leak across the jump.
    ClearActiveAnimNotifyStates(false);

    if (PlayRate >= 0.0f)
    {
        UpdateRecentNotifyEvents(CurrentSequence, ClampedRangeStart, CurrentTime, DeltaTime);
    }
    else
    {
        UpdateRecentNotifyEvents(CurrentSequence, ClampedRangeEnd, CurrentTime, DeltaTime);
    }
}

void UAnimSingleNodeInstance::UpdateAnimation(float DeltaTime)
{
    UAnimInstance::UpdateAnimation(DeltaTime);
}

void UAnimSingleNodeInstance::EvaluatePose(FSkeletonPose& OutPose)
{
    if (!CurrentSequence)
    {
        InitializeReferencePose(OutPose);
        return;
    }

    EvaluatePoseAtTime(CurrentSequence, CurrentTime, OutPose.LocalTransforms);
}
