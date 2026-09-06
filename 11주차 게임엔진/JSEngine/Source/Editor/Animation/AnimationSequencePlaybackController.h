#pragma once

#include "Animation/AnimData/FrameRate.h"
#include "Core/CoreMinimal.h"

#ifdef GetCurrentTime
#undef GetCurrentTime
#endif

class UAnimSequence;
class FAnimationSequencePreviewScene;

class FAnimationSequencePlaybackController
{
public:
    void Initialize(UAnimSequence* InSequence);
    void Shutdown();

    void Tick(float DeltaTime, FAnimationSequencePreviewScene* PreviewScene);
    void SetCurrentTime(float InTime);
    float GetCurrentTime() const { return CurrentTime; }
    float GetLength() const;
    FFrameRate GetFrameRate() const;
    int32 GetFrameCount() const;
    int32 GetCurrentFrameIndex() const;

    void Play();
    void Pause();
    void Stop();
    void StepToNextFrame();
    void StepToPreviousFrame();
    void JumpToStart();
    void JumpToEnd();

    void SetLooping(bool bInLooping) { bLooping = bInLooping; }
    bool IsLooping() const { return bLooping; }

    void SetPlaybackRange(float InStartTime, float InEndTime);
    float GetPlaybackRangeStart() const { return PlaybackRangeStart; }
    float GetPlaybackRangeEnd() const { return PlaybackRangeEnd; }

    void SetPlayRate(float InPlayRate) { PlayRate = InPlayRate; }
    float GetPlayRate() const { return PlayRate; }

    bool IsPlaying() const { return bPlaying; }
    bool HasSequence() const;

    void ApplyPendingPose(FAnimationSequencePreviewScene* PreviewScene);

private:
    float ClampCurrentTime(float InTime) const;
    float GetPlaybackRangeLength() const;
    void AdvanceWithoutPreview(float DeltaTime);

private:
    UAnimSequence* Sequence = nullptr;
    float CurrentTime = 0.0f;
    float PlayRate = 1.0f;
    float PlaybackRangeStart = 0.0f;
    float PlaybackRangeEnd = 0.0f;
    bool bPlaying = false;
    bool bLooping = true;
    bool bPoseDirty = false;
};
