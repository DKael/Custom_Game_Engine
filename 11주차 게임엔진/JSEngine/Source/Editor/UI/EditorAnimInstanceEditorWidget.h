#pragma once

#include "Editor/UI/AnimInstanceStateSequenceContext.h"
#include "Editor/UI/EditorWidget.h"

class UAnimInstanceAsset;

class FEditorAnimInstanceEditorWidget : public FEditorWidget
{
public:
    void Render(float DeltaTime) override;

    void OpenAnimInstanceAsset(const FString& AnimInstancePath);
    bool IsVisible() const { return bVisible; }

private:
    void DrawToolbar();
    void DrawGraph();
    void DrawDetails();
    void InvalidateSelectedStateSequenceContext();
    void RefreshSelectedStateSequenceContext();
    int32 GetSelectedStateFinishedTransitionCount() const;
    void DrawSelectedStateSequenceContext(const FAnimInstanceStateSequenceContextReport& Context);
    void DrawSelectedStateNotifyContext(const FAnimInstanceStateSequenceContextReport& Context);
    void DrawSelectedStateAuthoringHints(const FAnimInstanceStateSequenceContextReport& Context);
    void AddState();
    void AddTransition();
    void DeleteSelectedState();
    void DeleteSelectedTransition();
    bool HasSelectedState() const;
    bool HasSelectedTransition() const;
    bool SaveAsset();
    bool ReloadAsset();
    void MarkDirty() { bDirty = true; }

private:
    FString CurrentPath;
    UAnimInstanceAsset* CurrentAsset = nullptr;
    int32 SelectedStateIndex = -1;
    int32 SelectedTransitionIndex = -1;
    FAnimInstanceStateSequenceContextReport CachedSelectedStateContext;
    int32 CachedSelectedStateContextStateIndex = -1;
    FString CachedSelectedStateContextStateName;
    FString CachedSelectedStateContextSequencePath;
    bool bCachedSelectedStateContextLoop = false;
    int32 CachedSelectedStateFinishedTransitionCount = -1;
    bool bCachedSelectedStateContextValid = false;
    bool bVisible = false;
    bool bDirty = false;
};
