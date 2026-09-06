#pragma once

#include "Core/CoreMinimal.h"

// COM 인터페이스 전방 선언 (d3d11.h 없이 포인터 사용 가능)
struct ID3D11ShaderResourceView;

/**
 * @brief MTL 파일의 머테리얼 데이터를 표현하는 구조체.
 * Obj .mtl 포맷 기준으로 정의했습니다.
 */

class FMaterial
{
public:
    FString Name;

    FVector AmbientColor   = { 0.2f, 0.2f, 0.2f }; // Ka
    FVector DiffuseColor   = { 0.8f, 0.8f, 0.8f }; // Kd
    FVector SpecularColor  = { 0.0f, 0.0f, 0.0f }; // Ks
    FVector EmissiveColor  = { 0.0f, 0.0f, 0.0f }; // Ke

    float Shininess  = 0.0f; 
    float Opacity    = 1.0f; 
    int   IllumModel = 2;    

	// Texture 정보
    FString DiffuseTexPath;   // map_Kd
	bool	bHasDiffuseTexture = { false };

	// 렌더 루프에서 매 프레임 FindTexture() 문자열 조회를 피하기 위한 캐시.
	// ResourceManager::LoadMaterial() 시점에 한 번만 세팅됨.
	ID3D11ShaderResourceView* CachedDiffuseSRV = nullptr;

    FString AmbientTexPath;   // map_Ka
	bool	bHasAmbientTexture = { false };

    FString SpecularTexPath;  // map_Ks
	bool	bHasSpecularTexture = { false };

	FString BumpTexPath;      // map_bump
	bool	bHasBumpTexture = { false };
};

/**
 * @brief Obj전용 .mtl 파일 파서
 */
class FObjMtlLoader
{
public:
    /**
     * @brief MTL 파일을 파싱하여 머테리얼 맵을 채웁니다.
     * @param FilePath
     * @param OutMaterials 
     * @return 파일 열기 성공 여부 
     */
    static bool Load(const FString& FilePath, TMap<FString, FMaterial>& OutMaterials);
};
