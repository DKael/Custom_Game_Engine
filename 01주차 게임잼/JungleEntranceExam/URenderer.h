#pragma once

#include <d3d11.h>
#include <d3dcompiler.h>

#include "FVertexSimple.h"
#include "FVector.h"
#include "FColor.h"

struct FConstants
{
    FVector Offset;    
    float WipeProgress;

    FVector Scale;     
    float Pad;    

    float FlashTimer;  
    FVector Pad2;  

    FColor BlockColor; 
};

class URenderer
{
public:
    ID3D11Device* Device = nullptr;
    ID3D11DeviceContext* DeviceContext = nullptr;
    IDXGISwapChain* SwapChain = nullptr;

    ID3D11Texture2D* FrameBuffer = nullptr;
    ID3D11RenderTargetView* FrameBufferRTV = nullptr;
    ID3D11RasterizerState* RasterizerState = nullptr;
    ID3D11Buffer* ConstantBuffer = nullptr;

    FLOAT ClearColor[4] = { 0.025f, 0.025f, 0.025f, 1.0f };
    D3D11_VIEWPORT ViewportInfo;

    ID3D11VertexShader* SimpleVertexShader=nullptr;
    ID3D11PixelShader* SimplePixelShader = nullptr;
    ID3D11InputLayout* SimpleInputLayout = nullptr;
    unsigned int Stride;

    UINT NumVerticesBullet;
    UINT NumVerticesSphere=0;
    UINT NumVerticesNewSphere=0;
    UINT NumVerticesBar=12;
    UINT NumVerticesRect = 12;

    ID3D11Buffer* vertexBufferSphere = nullptr;
    ID3D11Buffer* vertexBufferNewSphere = nullptr;
    ID3D11Buffer* vertexBufferBar = nullptr;
    ID3D11Buffer* vertexBufferRect = nullptr;
    ID3D11Buffer* vertexBufferTriangle = nullptr;
    ID3D11Buffer* vertexBufferBullet = nullptr;

public:
    void Create(HWND hWindow);
    void CreateDeviceAndSwapChain(HWND hWindow);
    void ReleaseDeviceAndSwapChain();
    void CreateFrameBuffer();
    void ReleaseFrameBuffer();
    void CreateRasterizerState();
    void ReleaseRasterizerState();
    void Release();
    void SwapBuffer();
    void CreateShader();
    void ReleaseShader();
    void Prepare();
    void PrepareShader();
    void RenderPrimitive(ID3D11Buffer* pBuffer, UINT numVertices);
    void RenderSphere();
    void RenderNewSphere();
    void RenderRectangle();
    void RenderTriangle();
    void RenderBullet();

    ID3D11Buffer* CreateVertexBuffer(FVertexSimple* vertices, UINT byteWidth);
    void ReleaseVertexBuffer();
    void ReleaseVertexBuffer(ID3D11Buffer* pbuffer);
    void CreateConstantBuffer();
    void ReleaseConstantBuffer();
    void UpdateConstant(FVector Offset, FVector Scale);
    void UpdateConstant(FVector Offset, FVector Scale, FColor Color, float WipeProgress, float FlashTimer);

    void UpdateConstant(FVector Offset, FVector Scale, FColor Color, float WipeProgress=-3.0f);

    void CreateRectBuffer();
    void ReleaseRectBuffer();
};
