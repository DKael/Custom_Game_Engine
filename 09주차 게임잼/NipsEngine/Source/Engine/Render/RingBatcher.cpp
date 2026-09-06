#include "RingBatcher.h"

#include <d3d11.h>
#include <cstring>

#include "Core/ResourceManager.h"

namespace
{
    bool CreateDynamicVertexBuffer(ID3D11Device* Device, uint32 ByteWidth, TComPtr<ID3D11Buffer>& OutBuffer)
    {
        if (!Device || ByteWidth == 0)
        {
            return false;
        }

        D3D11_BUFFER_DESC Desc = {};
        Desc.ByteWidth = ByteWidth;
        Desc.Usage = D3D11_USAGE_DYNAMIC;
        Desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        OutBuffer.Reset();
        return SUCCEEDED(Device->CreateBuffer(&Desc, nullptr, OutBuffer.ReleaseAndGetAddressOf()));
    }

    FRingVertex MakeRingVertex(
        const FVector& Center,
        const FVector& AxisA,
        const FVector& AxisB,
        const FVector2& LocalPosition,
        const FVector2& Radius,
        const FVector4& Color)
    {
        FRingVertex Vertex = {};
        Vertex.Position = Center + AxisA * LocalPosition.X + AxisB * LocalPosition.Y;
        Vertex.Color = Color;
        Vertex.LocalPosition = LocalPosition;
        Vertex.Radius = Radius;
        return Vertex;
    }
}

void FRingBatcher::Create(ID3D11Device* InDevice)
{
    Release();

    Device = InDevice;
    if (!Device)
    {
        return;
    }

    MaxVertexCount = 6;
    if (!CreateDynamicVertexBuffer(Device.Get(), sizeof(FRingVertex) * MaxVertexCount, VertexBuffer))
    {
        Release();
        return;
    }

    UMaterial* RingMaterial = FResourceManager::Get().GetOrCreateMaterial("RingMat", "Shaders/ShaderRing.hlsl");
    RingMaterial->DepthStencilType = EDepthStencilType::Default;
    RingMaterial->BlendType = EBlendType::AlphaBlend;
    RingMaterial->RasterizerType = ERasterizerType::SolidNoCull;
    RingMaterial->SamplerType = ESamplerType::EST_Linear;

    Material = RingMaterial;
}

void FRingBatcher::Release()
{
    VertexBuffer.Reset();
    Device.Reset();
    Vertices.clear();
    MaxVertexCount = 0;
    Material = nullptr;
}

void FRingBatcher::AddRing(const FVector& Center, const FVector& AxisA, const FVector& AxisB, float InnerRadius, float OuterRadius, const FVector4& Color)
{
    if (OuterRadius <= 0.0f || InnerRadius < 0.0f || InnerRadius >= OuterRadius)
    {
        return;
    }

    const FVector SafeAxisA = AxisA.GetSafeNormal();
    const FVector SafeAxisB = AxisB.GetSafeNormal();
    const FVector2 Radius(InnerRadius, OuterRadius);

    const FVector2 BottomLeft(-OuterRadius, -OuterRadius);
    const FVector2 TopLeft(-OuterRadius, OuterRadius);
    const FVector2 TopRight(OuterRadius, OuterRadius);
    const FVector2 BottomRight(OuterRadius, -OuterRadius);

    Vertices.push_back(MakeRingVertex(Center, SafeAxisA, SafeAxisB, BottomLeft, Radius, Color));
    Vertices.push_back(MakeRingVertex(Center, SafeAxisA, SafeAxisB, TopLeft, Radius, Color));
    Vertices.push_back(MakeRingVertex(Center, SafeAxisA, SafeAxisB, TopRight, Radius, Color));

    Vertices.push_back(MakeRingVertex(Center, SafeAxisA, SafeAxisB, BottomLeft, Radius, Color));
    Vertices.push_back(MakeRingVertex(Center, SafeAxisA, SafeAxisB, TopRight, Radius, Color));
    Vertices.push_back(MakeRingVertex(Center, SafeAxisA, SafeAxisB, BottomRight, Radius, Color));
}

void FRingBatcher::Clear()
{
    Vertices.clear();
}

void FRingBatcher::Flush(ID3D11DeviceContext* Context, const FRenderBus* RenderBus)
{
    if (!Context || !Device || !Material || Vertices.empty())
    {
        return;
    }

    const uint32 RequiredVertexCount = static_cast<uint32>(Vertices.size());
    if (!VertexBuffer || RequiredVertexCount > MaxVertexCount)
    {
        MaxVertexCount = RequiredVertexCount * 2;
        if (!CreateDynamicVertexBuffer(Device.Get(), sizeof(FRingVertex) * MaxVertexCount, VertexBuffer))
        {
            MaxVertexCount = 0;
            return;
        }
    }

    D3D11_MAPPED_SUBRESOURCE MappedResource = {};
    if (FAILED(Context->Map(VertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource)))
    {
        return;
    }

    memcpy(MappedResource.pData, Vertices.data(), sizeof(FRingVertex) * RequiredVertexCount);
    Context->Unmap(VertexBuffer.Get(), 0);

    Material->Bind(Context, RenderBus);

    UINT Stride = sizeof(FRingVertex);
    UINT Offset = 0;
    ID3D11Buffer* VertexBufferPtr = VertexBuffer.Get();
    Context->IASetVertexBuffers(0, 1, &VertexBufferPtr, &Stride, &Offset);
    Context->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
    Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    Context->Draw(RequiredVertexCount, 0);
}