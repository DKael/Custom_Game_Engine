#pragma once

#include "Animation/AnimData/AnimNotifyTypes.h"
#include "Animation/AnimData/FrameRate.h"
#include "Core/CoreMinimal.h"
#include "Editor/Animation/AnimationSequencePlaybackController.h"
#include "Editor/Animation/AnimationSequencePreviewMeshResolver.h"
#include "Editor/Animation/AnimationSequencePreviewScene.h"

#ifdef GetCurrentTime
#undef GetCurrentTime
#endif

#include <memory>

class UEditorEngine;
class UAnimSequence;
class USkeletalMesh;
class USkeletalMeshComponent;
class FSceneViewport;
struct ID3D11ShaderResourceView;

class FAnimationSequencePreviewController
{
public:
    FAnimationSequencePreviewController() = default;
    ~FAnimationSequencePreviewController();

    bool Initialize(UEditorEngine* InEditorEngine, const FString& InSequencePath, UAnimSequence* InSequence);
    void Shutdown();
    void Tick(float DeltaTime);

    void SetCurrentTime(float InTime);
    float GetCurrentTime() const;
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

    void SetLooping(bool bInLooping);
    bool IsLooping() const;

    void SetPlaybackRange(float InStartTime, float InEndTime);
    float GetPlaybackRangeStart() const;
    float GetPlaybackRangeEnd() const;

    void SetPlayRate(float InPlayRate);
    float GetPlayRate() const;

    bool IsPlaying() const;
    bool HasSequence() const;
    bool HasValidPreview() const;
    const TArray<FAnimNotifyEvent>& GetRecentFiredNotifyEvents() const;
    // These "socket" queries intentionally mirror runtime attach-target
    // semantics used by AnimNotify playback. They accept/return any name that
    // the preview skinned mesh component resolves through HasSocket /
    // GetSocketTransform, which includes sockets and bone names.
    bool HasPreviewSocket(const FName& SocketName) const;
    TArray<FString> GetPreviewSocketNames() const;
    bool HasPreviewPrimitiveComponent(const FString& ComponentName) const;
    TArray<FString> GetPreviewPrimitiveComponentNames() const;
    const USkeletalMesh* GetPreviewMesh() const;
    USkeletalMeshComponent* GetPreviewComponent() const;
    bool SelectPreviewBone(int32 BoneIndex);
    void ClearPreviewSelection();

    const FString& GetPreviewStatusText() const { return PreviewStatusText; }
    const FString& GetTimelineStatusText() const { return TimelineStatusText; }

    void SetViewportSize(int32 InWidth, int32 InHeight);
    ID3D11ShaderResourceView* GetPreviewSRV() const;

    FSceneViewport* GetSceneViewport();
    const FSceneViewport* GetSceneViewport() const;

private:
    bool TryInitializePreviewScene();

private:
    static constexpr float PreviewInitializationRetryInterval = 0.25f;

    UEditorEngine* EditorEngine = nullptr;
    FString SequencePath;
    UAnimSequence* Sequence = nullptr;
    std::unique_ptr<FAnimationSequencePlaybackController> PlaybackController;
    std::unique_ptr<FAnimationSequencePreviewMeshResolver> PreviewMeshResolver;
    std::unique_ptr<FAnimationSequencePreviewScene> PreviewScene;
    float PreviewInitializationRetryTimeRemaining = 0.0f;

    FString PreviewStatusText = "Preview is not initialized.";
    FString TimelineStatusText = "Timeline controls are available after preview initialization.";
};
