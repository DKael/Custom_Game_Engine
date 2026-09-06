#include "Editor/Animation/AnimationSequencePlaybackController.h"

#include "Editor/Animation/AnimationSequencePreviewScene.h"
#include "Editor/Animation/AnimationSequenceViewerUtils.h"

#include <algorithm>
#include <cmath>

#ifdef GetCurrentTime
#undef GetCurrentTime
#endif

void FAnimationSequencePlaybackController::Initialize(UAnimSequence* InSequence)
{
    Sequence = InSequence;
    CurrentTime = 0.0f;
    PlayRate = 1.0f;
    PlaybackRangeStart = 0.0f;
    PlaybackRangeEnd = GetLength();
    bPlaying = false;
    bLooping = true;
    bPoseDirty = false;
}

void FAnimationSequencePlaybackController::Shutdown()
{
    Sequence = nullptr;
    CurrentTime = 0.0f;
    PlayRate = 1.0f;
    PlaybackRangeStart = 0.0f;
    PlaybackRangeEnd = 0.0f;
    bPlaying = false;
    bLooping = true;
    bPoseDirty = false;
}

bool FAnimationSequencePlaybackController::HasSequence() const
{
    return AnimationSequenceViewer::IsLiveObject(Sequence);
}

void FAnimationSequencePlaybackController::Tick(float DeltaTime, FAnimationSequencePreviewScene* PreviewScene)
{
    if (PreviewScene && PreviewScene->HasValidPreview())
    {
        // PlaybackController owns time progression; the preview component stays
        // paused and only mirrors the controller-authored time so playback
        // range clamping is respected even when a live preview exists.
        PreviewScene->ApplyPlaybackSettings(bLooping, PlayRate, false);

        if (!bPlaying && !bPoseDirty)
        {
            return;
        }

        if (bPoseDirty)
        {
            PreviewScene->SetCurrentTime(CurrentTime);
            CurrentTime = PreviewScene->GetCurrentTime();
            bPoseDirty = false;
            if (!bPlaying)
            {
                return;
            }
        }

        const float PreviousTime = CurrentTime;
        AdvanceWithoutPreview(DeltaTime);
        const bool bWrapped =
            bLooping &&
            ((PlayRate >= 0.0f && CurrentTime < PreviousTime) ||
             (PlayRate < 0.0f && CurrentTime > PreviousTime));
        PreviewScene->AdvancePreviewPlayback(
            CurrentTime,
            DeltaTime,
            bWrapped,
            PlaybackRangeStart,
            PlaybackRangeEnd);
        CurrentTime = PreviewScene->GetCurrentTime();
        return;
    }

    if (!bPlaying && !bPoseDirty)
    {
        return;
    }

    if (bPoseDirty)
    {
        CurrentTime = ClampCurrentTime(CurrentTime);
        bPoseDirty = false;
        if (!bPlaying)
        {
            return;
        }
    }

    AdvanceWithoutPreview(DeltaTime);
}

void FAnimationSequencePlaybackController::SetCurrentTime(float InTime)
{
    CurrentTime = ClampCurrentTime(InTime);
    bPoseDirty = true;
}

float FAnimationSequencePlaybackController::GetLength() const
{
    return AnimationSequenceViewer::GetSequenceLengthSeconds(Sequence);
}

FFrameRate FAnimationSequencePlaybackController::GetFrameRate() const
{
    return AnimationSequenceViewer::GetSequenceFrameRate(Sequence);
}

int32 FAnimationSequencePlaybackController::GetFrameCount() const
{
    return AnimationSequenceViewer::GetSequenceFrameCount(Sequence);
}

int32 FAnimationSequencePlaybackController::GetCurrentFrameIndex() const
{
    return AnimationSequenceViewer::TimeToFrameIndex(CurrentTime, GetFrameRate(), GetFrameCount());
}

void FAnimationSequencePlaybackController::Play()
{
    if (!HasSequence())
    {
        bPlaying = false;
        return;
    }

    bPlaying = true;
}

void FAnimationSequencePlaybackController::Pause()
{
    bPlaying = false;
}

void FAnimationSequencePlaybackController::Stop()
{
    Pause();
    JumpToStart();
}

void FAnimationSequencePlaybackController::StepToNextFrame()
{
    const int32 FrameCount = GetFrameCount();
    if (FrameCount <= 0)
    {
        return;
    }

    Pause();
    SetCurrentTime(
        AnimationSequenceViewer::FrameIndexToTime(
            std::min(GetCurrentFrameIndex() + 1, FrameCount - 1),
            GetFrameRate()));
}

void FAnimationSequencePlaybackController::StepToPreviousFrame()
{
    const int32 FrameCount = GetFrameCount();
    if (FrameCount <= 0)
    {
        return;
    }

    Pause();
    SetCurrentTime(
        AnimationSequenceViewer::FrameIndexToTime(
            std::max(GetCurrentFrameIndex() - 1, 0),
            GetFrameRate()));
}

void FAnimationSequencePlaybackController::JumpToStart()
{
    Pause();
    SetCurrentTime(PlaybackRangeStart);
}

void FAnimationSequencePlaybackController::JumpToEnd()
{
    Pause();
    SetCurrentTime(PlaybackRangeEnd);
}

void FAnimationSequencePlaybackController::SetPlaybackRange(float InStartTime, float InEndTime)
{
    const float Length = GetLength();
    const float MinimumRange = std::max(
        GetFrameRate().AsDecimal() > 0.0 ? static_cast<float>(1.0 / GetFrameRate().AsDecimal()) : 0.0f,
        1.0f / 240.0f);
    float StartTime = std::min(InStartTime, InEndTime);
    float EndTime = std::max(InStartTime, InEndTime);

    if (Length <= 0.0f)
    {
        PlaybackRangeStart = 0.0f;
        PlaybackRangeEnd = 0.0f;
        CurrentTime = 0.0f;
        bPoseDirty = true;
        return;
    }

    StartTime = std::clamp(StartTime, 0.0f, Length);
    EndTime = std::clamp(EndTime, StartTime + MinimumRange, Length);
    if (EndTime - StartTime < MinimumRange)
    {
        EndTime = std::min(Length, StartTime + MinimumRange);
        StartTime = std::max(0.0f, EndTime - MinimumRange);
    }

    PlaybackRangeStart = StartTime;
    PlaybackRangeEnd = EndTime;
    CurrentTime = ClampCurrentTime(CurrentTime);
    bPoseDirty = true;
}

void FAnimationSequencePlaybackController::ApplyPendingPose(FAnimationSequencePreviewScene* PreviewScene)
{
    if (!PreviewScene || !PreviewScene->HasValidPreview())
    {
        return;
    }

    PreviewScene->ApplyPlaybackSettings(bLooping, PlayRate, bPlaying);
    PreviewScene->SetCurrentTime(CurrentTime);
    CurrentTime = PreviewScene->GetCurrentTime();
    bPoseDirty = false;
}

float FAnimationSequencePlaybackController::ClampCurrentTime(float InTime) const
{
    return std::clamp(InTime, PlaybackRangeStart, PlaybackRangeEnd);
}

float FAnimationSequencePlaybackController::GetPlaybackRangeLength() const
{
    return std::max(PlaybackRangeEnd - PlaybackRangeStart, 0.0f);
}

void FAnimationSequencePlaybackController::AdvanceWithoutPreview(float DeltaTime)
{
    const float RangeLength = GetPlaybackRangeLength();
    CurrentTime += DeltaTime * PlayRate;

    if (bLooping && RangeLength > 0.0f)
    {
        CurrentTime -= PlaybackRangeStart;
        CurrentTime = std::fmod(CurrentTime, RangeLength);
        if (CurrentTime < 0.0f)
        {
            CurrentTime += RangeLength;
        }
        CurrentTime += PlaybackRangeStart;
        return;
    }

    CurrentTime = std::clamp(CurrentTime, PlaybackRangeStart, PlaybackRangeEnd);
    if ((PlayRate >= 0.0f && CurrentTime >= PlaybackRangeEnd) || (PlayRate < 0.0f && CurrentTime <= PlaybackRangeStart))
    {
        bPlaying = false;
    }
}
