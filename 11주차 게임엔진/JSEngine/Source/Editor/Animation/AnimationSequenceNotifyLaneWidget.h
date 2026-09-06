#pragma once

struct FAnimationSequenceSequencerVisibleLayout;
class FAnimationSequenceEditorState;
class FAnimationSequenceEditorDocument;
struct FAnimationSequenceTimelineGeometry;

class FAnimationSequenceNotifyLaneWidget
{
public:
    void RenderRows(
        FAnimationSequenceEditorState& State,
        FAnimationSequenceEditorDocument* Document,
        const FAnimationSequenceTimelineGeometry& Geometry,
        const FAnimationSequenceSequencerVisibleLayout& VisibleLayout,
        float TimelineRowOriginY);
};
