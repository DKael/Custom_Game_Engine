#pragma once

#include "Editor/Animation/AnimationSequenceSequencerVisibleLayout.h"

#include <array>

class FAnimationSequenceEditorState;
class FAnimationSequenceEditorDocument;

class FAnimationSequenceTrackOutlinerWidget
{
public:
    void Render(
        FAnimationSequenceEditorState& State,
        FAnimationSequenceEditorDocument* Document,
        const FAnimationSequenceSequencerVisibleLayout& VisibleLayout,
        float RowOriginY);

private:
    void OpenRenameNotifyTrackPopup(int32 TrackIndex, const FString& CurrentLabel);
    void RenderRenameNotifyTrackPopup(FAnimationSequenceEditorDocument* Document);

private:
    bool bOpenRenameNotifyTrackPopupPending = false;
    int32 RenameNotifyTrackIndex = -1;
    std::array<char, 128> RenameNotifyTrackBuffer = {};
};
