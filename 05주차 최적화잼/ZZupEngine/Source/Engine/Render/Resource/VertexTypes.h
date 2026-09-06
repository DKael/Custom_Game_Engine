#pragma once

#include "Core/CoreMinimal.h"

/*
	Vertex 구조체들을 정의하는 Header입니다.
	추후에 다양한 Vertex 구조체들을 추가할 수 있습니다.
*/

struct FVertex
{ 
	FVector Position;
	FColor Color;
	int SubID;
};

struct FNormalVertex
{
	FVector Position;
	FColor Color;
	FVector Normal;
	FVector2 UVs;	//	TexCoord
};

struct FOverlayVertex
{
	float X, Y;
};

// Position + TexCoord 범용 버텍스 (FontBatcher, SubUVBatcher 등 텍스처 기반 배처 공용)
struct FTextureVertex
{
	FVector  Position;
	FVector2 TexCoord;
};

struct FMeshData
{
	TArray<FVertex> Vertices;
	TArray<uint32> Indices;
};

// PerformanceStaticMesh 전용: Color 제거 + UV half-float 압축
// 36 bytes(FNormalVertex) → 16 bytes (56% 절감)
struct FPerformanceVertex
{
	FVector  Position;    // 12 bytes
	uint32_t PackedUV;    // 4 bytes  (DXGI_FORMAT_R16G16_FLOAT: lo=U, hi=V)
};
