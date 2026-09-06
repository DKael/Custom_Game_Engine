#include "Editor/Animation/AnimationSequencePreviewController.h"

#include "Animation/AnimData/AnimSequence.h"
#include "Core/Paths.h"
#include "Editor/Animation/AnimationSequencePlaybackController.h"
#include "Editor/Animation/AnimationSequencePreviewMeshResolver.h"
#include "Editor/Animation/AnimationSequencePreviewScene.h"
#include "Editor/Animation/AnimationSequenceViewerUtils.h"
#include "Editor/EditorEngine.h"

#include <algorithm>

#ifdef GetCurrentTime
#undef GetCurrentTime
#endif

FAnimationSequencePreviewController::~FAnimationSequencePreviewController()
{
    Shutdown();
}

bool FAnimationSequencePreviewController::Initialize(
    UEditorEngine* InEditorEngine,
    const FString& InSequencePath,
    UAnimSequence* InSequence)
{
    Shutdown();

    EditorEngine = InEditorEngine;
    SequencePath = FPaths::Normalize(InSequencePath);
    Sequence = InSequence;
    PlaybackController = std::make_unique<FAnimationSequencePlaybackController>();
    PreviewMeshResolver = std::make_unique<FAnimationSequencePreviewMeshResolver>();
    PreviewScene = std::make_unique<FAnimationSequencePreviewScene>();
    PlaybackController->Initialize(Sequence);
    PreviewInitializationRetryTimeRemaining = 0.0f;

    PreviewStatusText = "Preview is not initialized.";
    TimelineStatusText = "Timeline controls are available after preview initialization.";

    if (!InEditorEngine || !AnimationSequenceViewer::IsLiveObject(Sequence))
    {
        PreviewStatusText =
            "Preview initialization failed: sequence document is missing editor context or animation data.";
        TimelineStatusText = "Timeline is unavailable until the animation sequence is loaded.";
        return false;
    }

    PlaybackController->SetLooping(true);
    PlaybackController->SetPlayRate(1.0f);
    PlaybackController->Pause();
    PlaybackController->SetPlaybackRange(0.0f, PlaybackController->GetLength());
    PlaybackController->SetCurrentTime(0.0f);

    return TryInitializePreviewScene();
}

void FAnimationSequencePreviewController::Shutdown()
{
    if (PreviewScene)
    {
        PreviewScene->Shutdown();
        PreviewScene.reset();
    }

    if (PlaybackController)
    {
        PlaybackController->Shutdown();
        PlaybackController.reset();
    }

    PreviewMeshResolver.reset();
    EditorEngine = nullptr;
    Sequence = nullptr;
    SequencePath.clear();
    PreviewInitializationRetryTimeRemaining = 0.0f;
}

void FAnimationSequencePreviewController::Tick(float DeltaTime)
{
    if (!HasValidPreview() && PlaybackController && PreviewMeshResolver && PreviewScene &&
        EditorEngine && AnimationSequenceViewer::IsLiveObject(Sequence))
    {
        PreviewInitializationRetryTimeRemaining =
            std::max(0.0f, PreviewInitializationRetryTimeRemaining - std::max(DeltaTime, 0.0f));
        if (PreviewInitializationRetryTimeRemaining <= 0.0f)
        {
            TryInitializePreviewScene();
        }
    }

    if (PreviewScene)
    {
        PreviewScene->TickViewportClient(DeltaTime);
    }

    if (PlaybackController)
    {
        PlaybackController->Tick(DeltaTime, PreviewScene.get());
    }
}

void FAnimationSequencePreviewController::SetCurrentTime(float InTime)
{
    if (!PlaybackController)
    {
        return;
    }

    PlaybackController->SetCurrentTime(InTime);
    PlaybackController->ApplyPendingPose(PreviewScene.get());
}

float FAnimationSequencePreviewController::GetCurrentTime() const
{
    return PlaybackController ? PlaybackController->GetCurrentTime() : 0.0f;
}

float FAnimationSequencePreviewController::GetLength() const
{
    if (PreviewScene && PreviewScene->HasValidPreview())
    {
        return PreviewScene->GetPreviewLength();
    }

    return PlaybackController ? PlaybackController->GetLength() : 0.0f;
}

FFrameRate FAnimationSequencePreviewController::GetFrameRate() const
{
    return PlaybackController ? PlaybackController->GetFrameRate() : FFrameRate();
}

int32 FAnimationSequencePreviewController::GetFrameCount() const
{
    return PlaybackController ? PlaybackController->GetFrameCount() : 0;
}

int32 FAnimationSequencePreviewController::GetCurrentFrameIndex() const
{
    return PlaybackController ? PlaybackController->GetCurrentFrameIndex() : 0;
}

void FAnimationSequencePreviewController::Play()
{
    if (!PlaybackController)
    {
        return;
    }

    if (PreviewScene && PreviewScene->HasValidPreview())
    {
        PlaybackController->Play();
    }
    else
    {
        PlaybackController->Pause();
    }
}

void FAnimationSequencePreviewController::Pause()
{
    if (PlaybackController)
    {
        PlaybackController->Pause();
    }
}

void FAnimationSequencePreviewController::Stop()
{
    if (PlaybackController)
    {
        PlaybackController->Stop();
        PlaybackController->ApplyPendingPose(PreviewScene.get());
    }
}

void FAnimationSequencePreviewController::StepToNextFrame()
{
    if (!PlaybackController)
    {
        return;
    }

    PlaybackController->StepToNextFrame();
    PlaybackController->ApplyPendingPose(PreviewScene.get());
}

void FAnimationSequencePreviewController::StepToPreviousFrame()
{
    if (!PlaybackController)
    {
        return;
    }

    PlaybackController->StepToPreviousFrame();
    PlaybackController->ApplyPendingPose(PreviewScene.get());
}

void FAnimationSequencePreviewController::JumpToStart()
{
    if (!PlaybackController)
    {
        return;
    }

    PlaybackController->JumpToStart();
    PlaybackController->ApplyPendingPose(PreviewScene.get());
}

void FAnimationSequencePreviewController::JumpToEnd()
{
    if (!PlaybackController)
    {
        return;
    }

    PlaybackController->JumpToEnd();
    PlaybackController->ApplyPendingPose(PreviewScene.get());
}

void FAnimationSequencePreviewController::SetLooping(bool bInLooping)
{
    if (!PlaybackController)
    {
        return;
    }

    PlaybackController->SetLooping(bInLooping);
    if (PreviewScene)
    {
        PreviewScene->ApplyPlaybackSettings(
            PlaybackController->IsLooping(),
            PlaybackController->GetPlayRate(),
            PlaybackController->IsPlaying());
    }
}

bool FAnimationSequencePreviewController::IsLooping() const
{
    return PlaybackController ? PlaybackController->IsLooping() : true;
}

void FAnimationSequencePreviewController::SetPlaybackRange(float InStartTime, float InEndTime)
{
    if (!PlaybackController)
    {
        return;
    }

    PlaybackController->SetPlaybackRange(InStartTime, InEndTime);
    PlaybackController->ApplyPendingPose(PreviewScene.get());
}

float FAnimationSequencePreviewController::GetPlaybackRangeStart() const
{
    return PlaybackController ? PlaybackController->GetPlaybackRangeStart() : 0.0f;
}

float FAnimationSequencePreviewController::GetPlaybackRangeEnd() const
{
    return PlaybackController ? PlaybackController->GetPlaybackRangeEnd() : 0.0f;
}

void FAnimationSequencePreviewController::SetPlayRate(float InPlayRate)
{
    if (!PlaybackController)
    {
        return;
    }

    PlaybackController->SetPlayRate(InPlayRate);
    if (PreviewScene)
    {
        PreviewScene->ApplyPlaybackSettings(
            PlaybackController->IsLooping(),
            PlaybackController->GetPlayRate(),
            PlaybackController->IsPlaying());
    }
}

float FAnimationSequencePreviewController::GetPlayRate() const
{
    return PlaybackController ? PlaybackController->GetPlayRate() : 1.0f;
}

bool FAnimationSequencePreviewController::IsPlaying() const
{
    return PlaybackController ? PlaybackController->IsPlaying() : false;
}

bool FAnimationSequencePreviewController::HasSequence() const
{
    return PlaybackController ? PlaybackController->HasSequence() : false;
}

bool FAnimationSequencePreviewController::HasValidPreview() const
{
    return PreviewScene && PreviewScene->HasValidPreview();
}

const USkeletalMesh* FAnimationSequencePreviewController::GetPreviewMesh() const
{
    return PreviewScene ? PreviewScene->GetPreviewMesh() : nullptr;
}

USkeletalMeshComponent* FAnimationSequencePreviewController::GetPreviewComponent() const
{
    return PreviewScene ? PreviewScene->GetPreviewComponent() : nullptr;
}

bool FAnimationSequencePreviewController::SelectPreviewBone(int32 BoneIndex)
{
    return PreviewScene && PreviewScene->SelectPreviewBone(BoneIndex);
}

void FAnimationSequencePreviewController::ClearPreviewSelection()
{
    if (PreviewScene)
    {
        PreviewScene->ClearPreviewSelection();
    }
}

const TArray<FAnimNotifyEvent>& FAnimationSequencePreviewController::GetRecentFiredNotifyEvents() const
{
    static const TArray<FAnimNotifyEvent> EmptyEvents = {};
    return PreviewScene ? PreviewScene->GetRecentFiredNotifyEvents() : EmptyEvents;
}

bool FAnimationSequencePreviewController::HasPreviewSocket(const FName& SocketName) const
{
    return PreviewScene && PreviewScene->HasPreviewSocket(SocketName);
}

TArray<FString> FAnimationSequencePreviewController::GetPreviewSocketNames() const
{
    return PreviewScene ? PreviewScene->GetPreviewSocketNames() : TArray<FString>();
}

bool FAnimationSequencePreviewController::HasPreviewPrimitiveComponent(const FString& ComponentName) const
{
    return PreviewScene && PreviewScene->HasPreviewPrimitiveComponent(ComponentName);
}

TArray<FString> FAnimationSequencePreviewController::GetPreviewPrimitiveComponentNames() const
{
    return PreviewScene ? PreviewScene->GetPreviewPrimitiveComponentNames() : TArray<FString>();
}

void FAnimationSequencePreviewController::SetViewportSize(int32 InWidth, int32 InHeight)
{
    if (PreviewScene)
    {
        PreviewScene->SetViewportSize(InWidth, InHeight);
    }
}

ID3D11ShaderResourceView* FAnimationSequencePreviewController::GetPreviewSRV() const
{
    return PreviewScene ? PreviewScene->GetPreviewSRV() : nullptr;
}

FSceneViewport* FAnimationSequencePreviewController::GetSceneViewport()
{
    return PreviewScene ? PreviewScene->GetSceneViewport() : nullptr;
}

const FSceneViewport* FAnimationSequencePreviewController::GetSceneViewport() const
{
    return PreviewScene ? PreviewScene->GetSceneViewport() : nullptr;
}

bool FAnimationSequencePreviewController::TryInitializePreviewScene()
{
    if (!(PlaybackController && PreviewMeshResolver && PreviewScene &&
        EditorEngine && AnimationSequenceViewer::IsLiveObject(Sequence)))
    {
        return false;
    }

    FString ResolvedPreviewMeshPath;
    if (!PreviewMeshResolver->Resolve(SequencePath, Sequence, ResolvedPreviewMeshPath))
    {
        if (!Sequence->GetSkeletonAssetPath().empty())
        {
            PreviewStatusText =
                "Preview mesh could not be resolved from SkeletonAssetPath: " + Sequence->GetSkeletonAssetPath();
        }
        else
        {
            PreviewStatusText =
                "Preview mesh could not be resolved because the sequence has no SkeletonAssetPath metadata yet.";
        }
        TimelineStatusText = "Playback controls are available after a preview mesh is resolved.";
        PreviewInitializationRetryTimeRemaining = PreviewInitializationRetryInterval;
        return false;
    }

    if (!PreviewScene->Initialize(EditorEngine, SequencePath, Sequence, ResolvedPreviewMeshPath))
    {
        PreviewStatusText = "Failed to load preview skeletal mesh: " + ResolvedPreviewMeshPath;
        TimelineStatusText = "Playback controls are available after a valid preview mesh loads.";
        PreviewInitializationRetryTimeRemaining = PreviewInitializationRetryInterval;
        return false;
    }

    PlaybackController->ApplyPendingPose(PreviewScene.get());
    PreviewStatusText = "Preview mesh: " + PreviewScene->GetPreviewMeshPath();
    TimelineStatusText = "Use the timeline below to scrub the current pose or play the clip in place.";
    PreviewInitializationRetryTimeRemaining = 0.0f;
    return true;
}
