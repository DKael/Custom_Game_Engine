#pragma once

#include "Editor/UI/EditorWidget.h"
#include "Engine/GameFramework/Camera/CameraSequenceManager.h"

class FEditorCameraSequenceWidget : public FEditorWidget
{
public:
    virtual void Initialize(UEditorEngine* InEditorEngine) override;
    virtual void Render(float DeltaTime) override;

private:
    void RenderToolbar();
    void RenderSequenceList();
    void RenderEditorBody();
    void RenderChannelCanvas();
    void RenderKeyInspector();

    void RefreshSequenceList();
    void CreateNewSequence();
    bool LoadSequence(const FString& Identifier);
    void SaveCurrentSequence();
    void ReloadCurrentSequence();
    void EnsureAllBuiltInChannelsExist();
    void UpdateTimeDomainFromSequence();
    void FrameValueDomain();
    void ClampDomains();
    FString GetSelectedChannelName() const;
    bool IsSelectedChannelFOV() const;
    FCameraSequenceChannel* GetSelectedChannel();
    const FCameraSequenceChannel* GetSelectedChannel() const;
    void ClampSelection();
    void ResetInteractionState();

private:
    TArray<FString> SequencePaths;
    FCameraSequenceAsset CurrentAsset;
    FString CurrentSequencePath;
    FString StatusMessage;
    int32 SelectedSequenceIndex = -1;
    int32 SelectedChannelIndex = 0;
    int32 SelectedKeyIndex = -1;
    int32 ActiveKeyDragIndex = -1;
    int32 ActiveLeaveHandleKeyIndex = -1;
    int32 ActiveArriveHandleKeyIndex = -1;
    bool bHasLoadedSequence = false;
    float TimeDomainMin = 0.0f;
    float TimeDomainMax = 1.0f;
    float ValueDomainMin = -1.0f;
    float ValueDomainMax = 1.0f;
    char SequencePathBuffer[260] = {};
    char SequenceNameBuffer[128] = {};
};
