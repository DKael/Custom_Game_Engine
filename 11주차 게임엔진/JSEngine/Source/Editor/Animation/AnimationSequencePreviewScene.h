#pragma once

#include "Animation/AnimData/AnimNotifyTypes.h"
#include "Core/CoreMinimal.h"
#include "Editor/Viewport/FSceneViewport.h"
#include "Editor/Viewport/SkeletalMeshViewportClient.h"

#ifdef GetCurrentTime
#undef GetCurrentTime
#endif

class UEditorEngine;
class UAnimSequence;
class USkeletalMesh;
class USkeletalMeshComponent;
class ASkeletalMeshActor;
class UWorld;
struct ID3D11ShaderResourceView;

class FAnimationSequencePreviewScene
{
public:
    FAnimationSequencePreviewScene() = default;
    ~FAnimationSequencePreviewScene();

    bool Initialize(
        UEditorEngine* InEditorEngine,
        const FString& InSequencePath,
        UAnimSequence* InSequence,
        const FString& InPreviewMeshPath);
    void Shutdown();

    bool HasValidPreview() const;
    const FString& GetPreviewMeshPath() const { return PreviewMeshPath; }
    const USkeletalMesh* GetPreviewMesh() const { return HasValidPreview() ? PreviewMesh : nullptr; }
    USkeletalMeshComponent* GetPreviewComponent() const { return HasValidPreview() ? PreviewComponent : nullptr; }

    void TickViewportClient(float DeltaTime);
    void ApplyPlaybackSettings(bool bLooping, float PlayRate, bool bPlaying);
    void SetCurrentTime(float InTime);
    float GetCurrentTime() const;
    float GetPreviewLength() const;
    const TArray<FAnimNotifyEvent>& GetRecentFiredNotifyEvents() const;
    bool HasPreviewSocket(const FName& SocketName) const;
    TArray<FString> GetPreviewSocketNames() const;
    bool HasPreviewPrimitiveComponent(const FString& ComponentName) const;
    TArray<FString> GetPreviewPrimitiveComponentNames() const;
    bool SelectPreviewBone(int32 BoneIndex);
    void ClearPreviewSelection();
    void RefreshPreviewPose(float DeltaTime);
    void AdvancePreviewPlayback(float InTime, float DeltaTime, bool bWrapped, float RangeStart, float RangeEnd);

    void SetViewportSize(int32 InWidth, int32 InHeight);
    ID3D11ShaderResourceView* GetPreviewSRV() const;

    FSceneViewport* GetSceneViewport() { return HasValidPreview() ? &PreviewViewport : nullptr; }
    const FSceneViewport* GetSceneViewport() const { return HasValidPreview() ? &PreviewViewport : nullptr; }

private:
    bool InitializePreviewWorld();
    bool InitializePreviewActor(UAnimSequence* Sequence);
    void InitializeViewportClient();
    void ConfigurePreviewCamera();

private:
    void ClearRecentNotifyHistory();
    void CaptureRecentNotifyEvents();

private:
    static constexpr uint32 InvalidPreviewResourceIndex = ~0u;
    static constexpr uint32 FirstEmbeddedPreviewResourceIndex = 1024u;
    inline static uint32 NextPreviewResourceIndex = 0;
    static constexpr int32 MaxRecentNotifyHistoryCount = 16;

    UEditorEngine* EditorEngine = nullptr;
    FString SequencePath;
    FString PreviewMeshPath;
    USkeletalMesh* PreviewMesh = nullptr;

    FName PreviewWorldHandle;
    UWorld* PreviewWorld = nullptr;
    ASkeletalMeshActor* PreviewActor = nullptr;
    USkeletalMeshComponent* PreviewComponent = nullptr;

    FSceneViewport PreviewViewport;
    FSkeletalMeshViewportClient PreviewViewportClient;
    uint32 PreviewResourceIndex = InvalidPreviewResourceIndex;
    int32 ViewportWidth = 512;
    int32 ViewportHeight = 512;
    TArray<FAnimNotifyEvent> RecentFiredNotifyHistory;
};
