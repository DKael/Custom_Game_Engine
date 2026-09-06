#pragma once

#include "Render/Common/RenderTypes.h"

#include <string>
#include <utility>
#include <vector>

struct FShaderCompiledState
{
	TComPtr<ID3D11VertexShader> VertexShader;
	TComPtr<ID3D11PixelShader> PixelShader;
	TComPtr<ID3D11InputLayout> InputLayout;
	std::string InterfaceSignature;
};

namespace ShaderCompilationUtils
{
	struct FCompileRequest
	{
		ID3D11Device* Device = nullptr;
		std::wstring FilePath;
		std::string VSEntryPoint;
		std::string PSEntryPoint;
		const std::vector<D3D11_INPUT_ELEMENT_DESC>* InputElements = nullptr;
		const std::vector<std::pair<std::string, std::string>>* MacroDefinitions = nullptr;
		const std::string* CurrentInterfaceSignature = nullptr;
		bool bAllowInterfaceChanges = false;
		bool bLogFailures = false;
	};

	bool CompileShader(
		const FCompileRequest& Request,
		FShaderCompiledState& OutCompiledState,
		std::string* OutFailureMessage);
}
