#pragma once

#include "Core/CoreTypes.h"
#include "Core/Containers/Array.h"
#include "Math/Vector.h"
#include "Math/Vector2.h"
#include "Math/Vector4.h"
#include "Render/Common/ComPtr.h"
#include "Render/Resource/Material.h"

struct ID3D11Buffer;
struct ID3D11Device;
struct ID3D11DeviceContext;
class FRenderBus;

struct FRingVertex
{
    FVector Position;
    FVector4 Color;
    FVector2 LocalPosition;
    FVector2 Radius;
};

class FRingBatcher
{
public:
    void Create(ID3D11Device* Device);
    void Release();

    void AddRing(const FVector& Center, const FVector& AxisA, const FVector& AxisB, float InnerRadius, float OuterRadius, const FVector4& Color);
    void Clear();
    void Flush(ID3D11DeviceContext* Context, const FRenderBus* RenderBus);

private:
    UMaterialInterface* Material = nullptr;
    TArray<FRingVertex> Vertices;
    TComPtr<ID3D11Buffer> VertexBuffer;
    TComPtr<ID3D11Device> Device;
    uint32 MaxVertexCount = 0;
};