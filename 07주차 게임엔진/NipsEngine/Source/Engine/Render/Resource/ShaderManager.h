#pragma once
#include "Core/CoreMinimal.h"
#include <d3d11.h>
#include <chrono>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <string>
#include <vector>

// 현재 발제 내용에서 하나의 Uber Shader로 여러 라이팅 모델을 렌더링해야합니다.
// 따라서 ShaderKey로 ViewMode를 사용해서 Opaque 패스가 같은 셰이더를 바인딩 할 계획입니다.
// 이후 이 클래스를 마주친 기구한 인원이 있다면, 머테리얼이 ShaderKey값을 통해
// 필요한 셰이더를 찾을 수 있도록 개선해주세요.

class FShader;
struct FRenderResources;
class FFontBatcher;
class FSubUVBatcher;


// 0000_0000_0000_0000_0000_000L_OOON_VVVV
enum EShaderKeyBits : uint32
{
    VIEWMODE_MASK = 0xF, // 4 bits
    NORMALMAP_BIT = 1 << 4,
	OPAQUE_TYPE_SHIFT=5,
	OPAQUE_TYPE_MASK = 0x3 << OPAQUE_TYPE_SHIFT,
	LIGHTCULLING_BIT = 1 << 8
    // 필요하면 계속 추가
};

enum EOpaqueType : uint32
{
	StaticMesh,
	Decal,
	Count
};

struct FShaderKey
{
    uint32 Bits = 0;

    bool operator==(const FShaderKey& other) const { return Bits == other.Bits; }

    inline void SetViewMode(uint32 view)
    {
        Bits &= ~VIEWMODE_MASK;
        Bits |= (view & VIEWMODE_MASK);
    }

	inline void SetNormalMap(bool enable)
    {
        if (enable)
            Bits |= NORMALMAP_BIT;
        else
            Bits &= ~NORMALMAP_BIT;
    }
	inline void SetOpaqueType(uint32 type)
	{
		Bits &= ~OPAQUE_TYPE_MASK;
        Bits |= (type << OPAQUE_TYPE_SHIFT) & OPAQUE_TYPE_MASK;
	}
    inline void SetLightCullMode(bool enable)
    {
        if (enable)
            Bits |= LIGHTCULLING_BIT;
        else
            Bits &= ~LIGHTCULLING_BIT;
    }
};

struct FShaderKeyHash
{
    size_t operator()(const FShaderKey& key) const { return std::hash<uint32>()(key.Bits); }
};

class FShaderManager
{
  public:
    FShader* GetShader(const FShaderKey& Key);
    void     PreloadShaders(ID3D11Device* Device);
    void     ProcessHotReloads(
        ID3D11Device* Device,
        const std::vector<std::wstring>& ChangedFiles,
        FRenderResources& Resources,
        FFontBatcher& FontBatcher,
        FSubUVBatcher& SubUVBatcher);

  private:
    FShader* CreateShader(ID3D11Device* Device, const FShaderKey& Key);
    void     ReloadShaders(
            ID3D11Device* Device,
            const std::set<std::wstring>& DirtyFiles,
            FRenderResources& Resources,
            FFontBatcher& FontBatcher,
            FSubUVBatcher& SubUVBatcher);
    void     CollectReloadableShaders(FRenderResources& Resources, FFontBatcher& FontBatcher, FSubUVBatcher& SubUVBatcher,
                std::vector<FShader*>& OutShaders);
    void     CollectShaderDependencies(
                const std::wstring& ShaderFilePath,
                std::unordered_set<std::wstring>& OutDependencies,
                std::unordered_map<std::wstring, std::unordered_set<std::wstring>>& Cache);
    std::wstring NormalizePath(const std::wstring& InPath) const;

  private:
    ID3D11Device* CachedDevice = nullptr;
    TMap<FShaderKey, std::unique_ptr<FShader>, FShaderKeyHash> ShaderMap;
    std::unordered_map<std::wstring, std::chrono::steady_clock::time_point> PendingShaderFiles;
    static constexpr uint32 ShaderReloadDebounceMs = 250;
};
