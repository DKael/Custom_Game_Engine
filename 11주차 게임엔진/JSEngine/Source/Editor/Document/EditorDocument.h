#pragma once

#include "Editor/UI/EditorCommandContext.h"
#include "Editor/UI/EditorTabManager.h"

class FSceneViewport;
class USkeletalMeshComponent;

struct FEditorSkeletalPreviewDebugSettings
{
    USkeletalMeshComponent* SkeletalMeshComponent = nullptr;
    bool bShowBones = false;
    bool bShowOnlySelectedBone = false;
    bool bShowSelectedBoneWeight = false;
    int32 SelectedBoneIndex = -1;
};

class FEditorDocument
{
public:
    virtual ~FEditorDocument() = default;

    virtual const FEditorTabId& GetTabId() const = 0;
    virtual const FString& GetTabLabel() const = 0;

    virtual void Tick(float DeltaTime) { (void)DeltaTime; }
    virtual void RenderEmbedded(float DeltaTime) = 0;
    virtual void RenderToolbar() = 0;
    virtual void BuildCommandList(FEditorCommandList& OutCommands) = 0;
    virtual FSceneViewport* GetSceneViewport() { return nullptr; }
    virtual const FSceneViewport* GetSceneViewport() const { return nullptr; }
    virtual bool GetSkeletalPreviewDebugSettings(FEditorSkeletalPreviewDebugSettings& OutSettings) const
    {
        (void)OutSettings;
        return false;
    }

    virtual bool CanSave() const { return false; }
    virtual bool Save() { return false; }
    virtual bool CanClose() const { return true; }
    virtual bool IsDetachedSupported() const { return false; }
};
