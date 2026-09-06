#pragma once
#include "Render/Common/ViewTypes.h"
#include "Render/Scene/RenderBus.h"
#include "Render/Resource/MeshBufferManager.h"

class FRenderCollector;

struct FRenderProxyContext
{
    const FShowFlags& ShowFlags;
    EViewMode ViewMode;
    FMeshBufferManager& MeshBufferManager;
};

class IRenderProxy
{
public:
    virtual void CollectRenderCommands(const FRenderProxyContext& Context, FRenderBus& RenderBus) = 0;
    virtual void Release() {}
};
