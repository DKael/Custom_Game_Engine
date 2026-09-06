#pragma once

class FTimeManager
{
public:
    void Reset();
    void PrepareFrame(float UnscaledDeltaTime);

    float GetUnscaledDeltaTime() const { return FrameUnscaledDeltaTime; }
    float GetScaledDeltaTime() const { return FrameScaledDeltaTime; }
    float GetGlobalTimeDilation() const { return ResolvedGlobalTimeDilation; }

    void SetBaseTimeDilation(float InTimeDilation);
    void StartHitStop(float Duration, float TimeScale = 0.05f);
    void StartSlomo(float TimeScale, float Duration);
    void StopSlomo();

private:
    float FrameUnscaledDeltaTime = 0.0f;
    float FrameScaledDeltaTime = 0.0f;

    float BaseTimeDilation = 1.0f;
    float ResolvedGlobalTimeDilation = 1.0f;

    float HitStopRemainingTime = 0.0f;
    float HitStopTimeScale = 0.05f;

    float SlomoRemainingTime = 0.0f;
    float SlomoTimeScale = 1.0f;
};
