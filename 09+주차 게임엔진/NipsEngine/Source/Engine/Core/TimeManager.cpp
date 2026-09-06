#include "Core/EnginePCH.h"
#include "Engine/Core/TimeManager.h"

#include "Engine/Math/Utils.h"

void FTimeManager::Reset()
{
    FrameUnscaledDeltaTime = 0.0f;
    FrameScaledDeltaTime = 0.0f;

    BaseTimeDilation = 1.0f;
    ResolvedGlobalTimeDilation = 1.0f;

    HitStopRemainingTime = 0.0f;
    HitStopTimeScale = 0.05f;

    SlomoRemainingTime = 0.0f;
    SlomoTimeScale = 1.0f;     
}

void FTimeManager::PrepareFrame(float UnscaledDeltaTime)
{
    FrameUnscaledDeltaTime = std::max(0.0f, UnscaledDeltaTime);

    HitStopRemainingTime = std::max(
        0.0f,
        HitStopRemainingTime - FrameUnscaledDeltaTime
    );

    SlomoRemainingTime = std::max(
        0.0f,
        SlomoRemainingTime - FrameUnscaledDeltaTime
    );

    const float ClampedBaseDilation = MathUtil::Clamp(
        BaseTimeDilation,
        0.0f,
        8.0f
    );

    const bool bHitStopActive = HitStopRemainingTime > 0.0f;
    const bool bSlomoActive = SlomoRemainingTime > 0.0f;

    if (bHitStopActive)
    {
        ResolvedGlobalTimeDilation =
            ClampedBaseDilation *
            MathUtil::Clamp(HitStopTimeScale, 0.0f, 1.0f);
    }
    else if (bSlomoActive)
    {
        ResolvedGlobalTimeDilation =
            ClampedBaseDilation *
            MathUtil::Clamp(SlomoTimeScale, 0.0f, 1.0f);
    }
    else
    {
        ResolvedGlobalTimeDilation = ClampedBaseDilation;
    }

    FrameScaledDeltaTime = FrameUnscaledDeltaTime * ResolvedGlobalTimeDilation;
}

void FTimeManager::SetBaseTimeDilation(float InTimeDilation)
{
    BaseTimeDilation = MathUtil::Clamp(InTimeDilation, 0.0f, 8.0f);
}

void FTimeManager::StartHitStop(float Duration, float TimeScale)
{
    HitStopRemainingTime = std::max(
        HitStopRemainingTime,
        MathUtil::Clamp(Duration, 0.0f, 5.0f)
    );

    HitStopTimeScale = MathUtil::Clamp(TimeScale, 0.0f, 1.0f);
}

void FTimeManager::StartSlomo(float TimeScale, float Duration)
{
    const float ClampedDuration = MathUtil::Clamp(Duration, 0.0f, 10.0f);
    const float ClampedTimeScale = MathUtil::Clamp(TimeScale, 0.0f, 1.0f);

    if (ClampedDuration <= 0.0f)
    {
        return;
    }

    const bool bAlreadySlomoActive = SlomoRemainingTime > 0.0f;

    SlomoRemainingTime = std::max(SlomoRemainingTime, ClampedDuration);

    if (bAlreadySlomoActive)
    {
        SlomoTimeScale = std::min(SlomoTimeScale, ClampedTimeScale);
    }
    else
    {
        SlomoTimeScale = ClampedTimeScale;
    }
}

void FTimeManager::StopSlomo()
{
    SlomoRemainingTime = 0.0f;
    SlomoTimeScale = 1.0f;
}
