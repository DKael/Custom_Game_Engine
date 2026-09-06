#pragma once

#include "Editor/Animation/AnimationSequenceSequencerVisibleLayout.h"

class FAnimationSequenceEditorState;
struct FAnimationSequenceTimelineGeometry;

class FAnimationSequenceCurveTrackWidget
{
public:
    void RenderRows(
        FAnimationSequenceEditorState& State,
        const FAnimationSequenceTimelineGeometry& Geometry,
        const FAnimationSequenceSequencerVisibleLayout& VisibleLayout,
        float TimelineRowOriginY) const;
};
