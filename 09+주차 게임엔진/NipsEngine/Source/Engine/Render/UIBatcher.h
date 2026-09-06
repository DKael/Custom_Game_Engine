#pragma once

#include "Core/CoreTypes.h"
#include "Core/Containers/Array.h"
#include "Core/ResourceTypes.h"
#include "Render/Common/ComPtr.h"
#include "Render/Resource/Material.h"
#include "Render/Resource/VertexTypes.h"

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11Buffer;
struct ID3D11ShaderResourceView;
class FRenderBus;


// 텍스처가 같은 쿼드를 묶어 DrawIndexed 한 번으로 처리
struct FUIBatch
{
    UTexture* Texture;  
    uint32    IndexStart;
    uint32    IndexCount;
    int32     BaseVertex;
};

class FUIBatcher
{
public:
    FUIBatcher()  = default;
    ~FUIBatcher() = default;

    void Create(ID3D11Device* InDevice);
    void Release();

    // 픽셀 좌표 기준으로 쿼드 추가
    // UVMin/UVMax 생략 시 전체 텍스처 ({0,0}→{1,1})
    void AddQuad(FVector2 ScreenXY,
        FVector2 QuadSize,
        FVector2 ViewportWH,
        UTexture* Texture,
        FVector4 Color,
        FVector2 UVMin = { 0.f, 0.f },
        FVector2 UVMax = { 1.f, 1.f },
        float RotationDegrees = 0.f);

    void Clear();
    void Flush(ID3D11DeviceContext* Context, const FRenderBus* RenderBus);

    uint32 GetQuadCount() const { return static_cast<uint32>(Vertices.size() / 4); }

private:
    void CreateBuffers();
    void CreateWhiteTexture();

private:
    TArray<FUIVertex> Vertices;
    TArray<uint32>    Indices;
    TArray<FUIBatch>  Batches;

    TComPtr<ID3D11Buffer> VertexBuffer;
    TComPtr<ID3D11Buffer> IndexBuffer;

    uint32 MaxVertexCount = 0;
    uint32 MaxIndexCount  = 0;

    TComPtr<ID3D11Device>             Device;
    UMaterialInterface*               Material = nullptr;

    // 단색 쿼드(Texture == nullptr)용 1x1 흰 SRV — UTexture 소유권 없이 직접 관리
    TComPtr<ID3D11ShaderResourceView> WhiteSRV;
};
