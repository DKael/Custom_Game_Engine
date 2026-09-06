#pragma once
#include <d3d11.h>
#include "Core/CoreTypes.h"

struct FRenderTargetSet;
struct FRenderResources;
class FRenderBus;
class FFontBatcher;
class FSubUVBatcher;
class FLineBatcher;
class FRingBatcher;
class FUIManager;

struct FRenderPassContext
{
    const FRenderBus* RenderBus = nullptr;
    FRenderTargetSet* RenderTargets = nullptr;
    ID3D11Device* Device = nullptr;
    ID3D11DeviceContext* DeviceContext = nullptr;
    FRenderResources* RenderResources = nullptr;
    FFontBatcher* FontBatcher = nullptr;
    FSubUVBatcher* SubUVBatcher = nullptr;
    FLineBatcher* GridLineBatcher = nullptr;

    FUIManager*   UIManager = nullptr;
    FLineBatcher* DebugLineBatcher = nullptr;
    FRingBatcher* DebugRingBatcher = nullptr;
    ID3D11ShaderResourceView* SceneGlobalLightBufferSRV = nullptr;
    uint32 SceneGlobalLightCount = 0;
};
