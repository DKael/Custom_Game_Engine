#pragma once

/*
	Shader들을 관리하는 Class입니다.
	추후에 Geometry Shader, Compute Shader 등 다양한 Shader들을 관리하는 Class로 확장할 수 있습니다.
*/

#include "Render/Common/RenderTypes.h"
#include "Render/Resource/ShaderCompilationUtils.h"

#include "Core/CoreTypes.h"

#include <string>
#include <utility>
#include <vector>

//	Shader Set
class FShader
{
public:
	TComPtr<ID3D11VertexShader> VertexShader;
	TComPtr<ID3D11PixelShader> PixelShader;
	TComPtr<ID3D11InputLayout> InputLayout;
	std::wstring FilePath;
	std::string VSEntryPoint;
	std::string PSEntryPoint;
	std::vector<D3D11_INPUT_ELEMENT_DESC> InputElements;
	std::vector<std::pair<std::string, std::string>> MacroDefinitions;
	std::string InterfaceSignature;

public:

private:
	bool CompileShaderState(
		ID3D11Device* InDevice,
		bool bAllowInterfaceChanges,
		bool bLogFailures,
		std::string* OutFailureMessage,
		FShaderCompiledState& OutCompiledState);
	void ApplyCompiledState(FShaderCompiledState&& InCompiledState);

public:
	bool Create(ID3D11Device* InDevice, const wchar_t* InFilePath, const char* InVSEntryPoint, const char* InPSEntryPoint,
		const D3D11_INPUT_ELEMENT_DESC* InInputElements, UINT InInputElementCount, const D3D_SHADER_MACRO* InDefines = nullptr);
	bool Reload(ID3D11Device* InDevice, std::string* OutFailureMessage = nullptr, bool bLogFailures = true);
	bool PrepareReload(
		ID3D11Device* InDevice,
		FShaderCompiledState& OutCompiledState,
		std::string* OutFailureMessage = nullptr,
		bool bLogFailures = true);
	void CommitReload(FShaderCompiledState&& InCompiledState);
	bool IsReloadable() const { return !FilePath.empty(); }
	const std::wstring& GetFilePath() const { return FilePath; }
	void Release();

	void Bind(ID3D11DeviceContext* InDeviceContext) const;
};

class FComputeShader
{
  private:
    TComPtr<ID3D11ComputeShader> ComputeShader;

  public:
    void Create(ID3D11Device* InDevice, const wchar_t* InFilePath, const char* InCSEntryPoint,
                const D3D_SHADER_MACRO* InDefines = nullptr);
    void Release();
    void Bind(ID3D11DeviceContext* InDeviceContext) const;
    bool IsValid() const { return ComputeShader != nullptr; }
};



