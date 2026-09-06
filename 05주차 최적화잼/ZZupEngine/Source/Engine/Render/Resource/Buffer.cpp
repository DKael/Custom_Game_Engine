#include "Buffer.h"
#include <d3d11.h>
#pragma region __FMESHBUFFER__

void FMeshBuffer::Create(ID3D11Device* InDevice, const FMeshData& InMeshData)
{
	if (InMeshData.Vertices.empty())
	{
		VertexBuffer.Release();
		IndexBuffer.Release();
		return;
	}

	VertexBuffer.Create(InDevice, InMeshData.Vertices, static_cast<uint32>(sizeof(FVertex) * InMeshData.Vertices.size()), sizeof(FVertex));
	if (!InMeshData.Indices.empty())
	{
		IndexBuffer.Create(InDevice, InMeshData.Indices, static_cast<uint32>(sizeof(uint32) * InMeshData.Indices.size()));
	}
}

void FMeshBuffer::CreateForStaticMesh(ID3D11Device* InDevice, const TArray<FNormalVertex>& InVertices, const TArray<uint32>& InIndices)
{
	if (InVertices.empty() || !InDevice)
	{
		return;
	}

	const uint32 Stride     = sizeof(FNormalVertex);
	const uint32 ByteWidth  = static_cast<uint32>(Stride * InVertices.size());

	D3D11_BUFFER_DESC Desc  = {};
	Desc.ByteWidth           = ByteWidth;
	Desc.Usage               = D3D11_USAGE_IMMUTABLE;
	Desc.BindFlags           = D3D11_BIND_VERTEX_BUFFER;

	D3D11_SUBRESOURCE_DATA SRD = { InVertices.data() };

	ID3D11Buffer* RawBuffer = nullptr;
	if (SUCCEEDED(InDevice->CreateBuffer(&Desc, &SRD, &RawBuffer)))
	{
		VertexBuffer.SetRaw(RawBuffer, static_cast<uint32>(InVertices.size()), Stride);
	}

	if (!InIndices.empty())
	{
		IndexBuffer.Create(InDevice, InIndices, static_cast<uint32>(sizeof(uint32) * InIndices.size()));
	}
}

void FMeshBuffer::CreateForPerformanceMesh(ID3D11Device* InDevice, const TArray<FPerformanceVertex>& InVertices, const TArray<uint32>& InIndices)
{
	if (InVertices.empty() || !InDevice)
	{
		return;
	}

	const uint32 Stride    = sizeof(FPerformanceVertex);
	const uint32 ByteWidth = static_cast<uint32>(Stride * InVertices.size());

	D3D11_BUFFER_DESC Desc  = {};
	Desc.ByteWidth           = ByteWidth;
	Desc.Usage               = D3D11_USAGE_IMMUTABLE;
	Desc.BindFlags           = D3D11_BIND_VERTEX_BUFFER;

	D3D11_SUBRESOURCE_DATA SRD = { InVertices.data() };

	ID3D11Buffer* RawBuffer = nullptr;
	if (SUCCEEDED(InDevice->CreateBuffer(&Desc, &SRD, &RawBuffer)))
	{
		VertexBuffer.SetRaw(RawBuffer, static_cast<uint32>(InVertices.size()), Stride);
	}

	if (!InIndices.empty())
	{
		IndexBuffer.Create(InDevice, InIndices, static_cast<uint32>(sizeof(uint32) * InIndices.size()));
	}
}

void FMeshBuffer::Release()
{
	VertexBuffer.Release();
	IndexBuffer.Release();
}

#pragma endregion

#pragma region __FSTRUCTUREDBUFFER__

void FStructuredBuffer::Create(ID3D11Device* Device, uint32 ElementCount, uint32 ElementStride)
{
	Release();

	if (ElementCount == 0 || ElementStride == 0 || Device == nullptr)
	{
		return;
	}

	// 1. Structured Buffer 생성
	D3D11_BUFFER_DESC Desc = {};
	Desc.ByteWidth = ElementCount * ElementStride;
	Desc.Usage = D3D11_USAGE_DYNAMIC;                 // CPU에서 주기적으로 업데이트 가능하도록 DYNAMIC 사용
	Desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;      // t 레지스터에 바인딩하기 위해 SRV 플래그 지정
	Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	Desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	Desc.StructureByteStride = ElementStride;

	HRESULT hr = Device->CreateBuffer(&Desc, nullptr, Buffer.ReleaseAndGetAddressOf());
	if (FAILED(hr))
	{
		return;
	}

	// 2. Shader Resource View(SRV) 생성
	D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
	SRVDesc.Format = DXGI_FORMAT_UNKNOWN;             // Structured Buffer는 Format을 UNKNOWN으로 설정합니다.
	SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	SRVDesc.Buffer.FirstElement = 0;
	SRVDesc.Buffer.NumElements = ElementCount;

	hr = Device->CreateShaderResourceView(Buffer.Get(), &SRVDesc, SRV.ReleaseAndGetAddressOf());
	if (FAILED(hr))
	{
		Release();
	}
}

void FStructuredBuffer::Update(ID3D11DeviceContext* Context, const void* Data, uint32 ByteSize)
{
	if (Buffer && Data && ByteSize > 0)
	{
		D3D11_MAPPED_SUBRESOURCE MappedResource;
		HRESULT hr = Context->Map(Buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
		if (SUCCEEDED(hr))
		{
			std::memcpy(MappedResource.pData, Data, ByteSize);
			Context->Unmap(Buffer.Get(), 0);
		}
	}
}

void FStructuredBuffer::Release()
{
	SRV.Reset();
	Buffer.Reset();
}

ID3D11ShaderResourceView* FStructuredBuffer::GetSRV() const
{
	return SRV.Get();
}

bool FStructuredBuffer::IsValid() const
{
	return Buffer.Get() != nullptr && SRV.Get() != nullptr;
}

#pragma endregion

#pragma region __FVERTEXBUFFER__

void FVertexBuffer::Create(ID3D11Device* InDevice, const TArray<FVertex> & InData, uint32 InByteWidth, uint32 InStride)
{
	if (InData.empty() || InByteWidth == 0)
	{
		Release();
		VertexCount = 0;
		Stride = InStride;
		return;
	}

	D3D11_BUFFER_DESC vertexBufferDesc = {};
	vertexBufferDesc.ByteWidth = InByteWidth;
	vertexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
	vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	D3D11_SUBRESOURCE_DATA vertexBufferSRD = { InData.data() };
	
	HRESULT hr = InDevice->CreateBuffer(&vertexBufferDesc, &vertexBufferSRD, Buffer.ReleaseAndGetAddressOf());
	if (FAILED(hr))
	{
		Release();
		VertexCount = 0;
		Stride = InStride;
		return;
	}

	VertexCount = static_cast<uint32>(InData.size());
	Stride = InStride;
}

void FVertexBuffer::SetRaw(ID3D11Buffer* InBuffer, uint32 InVertexCount, uint32 InStride)
{
	Release();
	Buffer.Attach(InBuffer);
	VertexCount = InVertexCount;
	Stride      = InStride;
}

void FVertexBuffer::Release()
{
	Buffer.Reset();
}

void FVertexBuffer::Update(ID3D11DeviceContext* InDeviceContext, const TArray<uint32>& InData, uint32 InByteWidth)
{
	//	 Do nothing
}

ID3D11Buffer* FVertexBuffer::GetBuffer() const
{
	return Buffer.Get();
}

#pragma endregion

#pragma region __FCONSTANTBUFFER__

void FConstantBuffer::Create(ID3D11Device* InDevice, uint32 InByteWidth)
{
	D3D11_BUFFER_DESC constantBufferDesc = {};

	constantBufferDesc.ByteWidth = (InByteWidth + 0xf) & 0xfffffff0;
	constantBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	constantBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;	
	constantBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	InDevice->CreateBuffer(&constantBufferDesc, nullptr, Buffer.ReleaseAndGetAddressOf());
}

void FConstantBuffer::Release()
{
	Buffer.Reset();
}

void FConstantBuffer::Update(ID3D11DeviceContext* InDeviceContext, const void * InData, uint32 InByteWidth)
{
	if (Buffer)
	{
		D3D11_MAPPED_SUBRESOURCE constantbufferMSR;
		InDeviceContext->Map(Buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &constantbufferMSR);

		std::memcpy(constantbufferMSR.pData, InData, InByteWidth);

		InDeviceContext->Unmap(Buffer.Get(), 0);
	}
}

ID3D11Buffer* FConstantBuffer::GetBuffer() 
{
	return Buffer.Get();
}

#pragma endregion

#pragma region __FINDEXBUFFER__

void FIndexBuffer::Create(ID3D11Device* InDevice, const TArray<uint32>& InData, uint32 InByteWidth)
{
	if (InData.empty() || InByteWidth == 0)
	{
		Release();
		IndexCount = 0;
		return;
	}

	D3D11_BUFFER_DESC indexBufferDesc = {};

	indexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
	indexBufferDesc.ByteWidth = InByteWidth;	//	NOTE : Total byte width of the buffer, not the count of indices
	indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

	D3D11_SUBRESOURCE_DATA indexBufferSRD = { InData.data() };

	HRESULT hr = InDevice->CreateBuffer(&indexBufferDesc, &indexBufferSRD, Buffer.ReleaseAndGetAddressOf());
	if (FAILED(hr))
	{
		Release();
		IndexCount = 0;
		return;
	}

	IndexCount = static_cast<uint32>(InData.size());
}

void FIndexBuffer::Release()
{
	Buffer.Reset();
}

void FIndexBuffer::Update(ID3D11DeviceContext* InDeviceContext, const TArray<uint32>& InData, uint32 InByteWidth)
{
	//	 Do nothing
}

ID3D11Buffer * FIndexBuffer::GetBuffer() const
{
	return Buffer.Get();
}

#pragma endregion

