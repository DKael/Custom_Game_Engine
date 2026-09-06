#include "VertexLayouts.h"

namespace VertexLayouts
{
    //	Primitive and Gizmo Input Layout
    extern const D3D11_INPUT_ELEMENT_DESC PrimitiveInputLayout[2] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, static_cast<uint32>(offsetof(FVertex, Position)),
         D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, static_cast<uint32>(offsetof(FVertex, Color)),
         D3D11_INPUT_PER_VERTEX_DATA, 0},
    };

    // StaticMesh (FNormalVertex) Input Layout
    extern const D3D11_INPUT_ELEMENT_DESC NormalVertexInputLayout[4] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, static_cast<uint32>(offsetof(FNormalVertex, Position)),
         D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, static_cast<uint32>(offsetof(FNormalVertex, Normal)),
         D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, static_cast<uint32>(offsetof(FNormalVertex, UVs)),
         D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TANGENT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, static_cast<uint32>(offsetof(FNormalVertex, Tangent)),
         D3D11_INPUT_PER_VERTEX_DATA, 0},
    };

    extern const D3D11_INPUT_ELEMENT_DESC DepthPrepassInputLayout[1] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };
} // namespace VertexLayouts