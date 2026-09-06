#pragma once

class FAnimationSequenceEditorState;
class FAnimationSequencePreviewController;
struct FAnimationSequenceTimelineGeometry;

class FAnimationSequenceTimelineWidget
{
public:
    float GetRulerHeight() const;
    void RenderCanvas(
        FAnimationSequenceEditorState& State,
        FAnimationSequencePreviewController* PreviewController,
        const FAnimationSequenceTimelineGeometry& Geometry,
        bool bCanvasHovered,
        bool bCanvasActive) const;
};
