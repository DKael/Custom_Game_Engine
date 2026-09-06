#pragma once
#include "SWindow.h"

#include <memory>

struct FViewportMouseEvent;
class ISlateViewport;
class FSceneViewport;

/*
* Viewport가 차지하는 공간을 묘사하는 Slate Widget
*/

class SViewport : public SWindow
{

public:
    SViewport();
    ~SViewport();

    bool OnMouseMove(int32 X, int32 Y) override;
    bool OnMouseButtonDown(int32 Button, int32 X, int32 Y) override;
    bool OnMouseButtonUp(int32 Button, int32 X, int32 Y) override;
    bool OnMouseWheel(int32 Delta, int32 X, int32 Y) override;
    bool OnKeyDown(uint32 Key) override;
    bool OnKeyUp(uint32 Key) override;

    // Get Set
    void SetViewportInterface(ISlateViewport* InInterface) { ViewportInterface = InInterface; }
    ISlateViewport* GetViewportInterface() const { return ViewportInterface; }

    FSceneViewport& GetSceneViewport() { return *SceneViewport; }
    const FSceneViewport& GetSceneViewport() const { return *SceneViewport; }

private:
    FViewportMouseEvent MakeMouseEvent(int32 X, int32 Y) const;

private:
    ISlateViewport* ViewportInterface = nullptr;
    std::unique_ptr<FSceneViewport> SceneViewport;
};

