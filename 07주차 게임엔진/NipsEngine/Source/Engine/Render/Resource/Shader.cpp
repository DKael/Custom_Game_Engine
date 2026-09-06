#include "Shader.h"
#include <iostream>
#include "Core/Paths.h"
#include "Render/Resource/ShaderCompilationUtils.h"

bool FShader::Create(ID3D11Device* InDevice, const wchar_t* InFilePath, const char* InVSEntryPoint, const char* InPSEntryPoint,
	const D3D11_INPUT_ELEMENT_DESC* InInputElements, UINT InInputElementCount, const D3D_SHADER_MACRO* InDefines)
{
	FilePath = InFilePath ? FPaths::ToAbsolute(InFilePath) : L"";
	VSEntryPoint = InVSEntryPoint ? InVSEntryPoint : "";
	PSEntryPoint = InPSEntryPoint ? InPSEntryPoint : "";


	InputElements.clear();
	if (InInputElements != nullptr && InInputElementCount > 0)
	{
		InputElements.assign(InInputElements, InInputElements + InInputElementCount);
	}

	MacroDefinitions.clear();
	if (InDefines != nullptr)
	{
		for (const D3D_SHADER_MACRO* Macro = InDefines; Macro->Name != nullptr; ++Macro)
		{
			MacroDefinitions.emplace_back(Macro->Name, Macro->Definition ? Macro->Definition : "");
		}
	}

	FShaderCompiledState CompiledState;
	if (!CompileShaderState(InDevice, true, true, nullptr, CompiledState))
	{
		return false;
	}

	ApplyCompiledState(std::move(CompiledState));
	return true;
}

bool FShader::Reload(ID3D11Device* InDevice, std::string* OutFailureMessage, bool bLogFailures)
{
	if (!IsReloadable())
	{
		if (OutFailureMessage != nullptr)
		{
			*OutFailureMessage = "shader is not reloadable";
		}
		return false;
	}

	FShaderCompiledState CompiledState;
	if (!CompileShaderState(InDevice, false, bLogFailures, OutFailureMessage, CompiledState))
	{
		return false;
	}

	ApplyCompiledState(std::move(CompiledState));
	return true;
}

bool FShader::PrepareReload(
	ID3D11Device* InDevice,
	FShaderCompiledState& OutCompiledState,
	std::string* OutFailureMessage,
	bool bLogFailures)
{
	if (!IsReloadable())
	{
		if (OutFailureMessage != nullptr)
		{
			*OutFailureMessage = "shader is not reloadable";
		}
		return false;
	}

	return CompileShaderState(InDevice, false, bLogFailures, OutFailureMessage, OutCompiledState);
}

void FShader::CommitReload(FShaderCompiledState&& InCompiledState)
{
	ApplyCompiledState(std::move(InCompiledState));
}

bool FShader::CompileShaderState(
	ID3D11Device* InDevice,
	bool bAllowInterfaceChanges,
	bool bLogFailures,
	std::string* OutFailureMessage,
	FShaderCompiledState& OutCompiledState)
{
	ShaderCompilationUtils::FCompileRequest Request = {};
	Request.Device = InDevice;
	Request.FilePath = FilePath;
	Request.VSEntryPoint = VSEntryPoint;
	Request.PSEntryPoint = PSEntryPoint;
	Request.InputElements = &InputElements;
	Request.MacroDefinitions = &MacroDefinitions;
	Request.CurrentInterfaceSignature = &InterfaceSignature;
	Request.bAllowInterfaceChanges = bAllowInterfaceChanges;
	Request.bLogFailures = bLogFailures;
	return ShaderCompilationUtils::CompileShader(Request, OutCompiledState, OutFailureMessage);
}

void FShader::ApplyCompiledState(FShaderCompiledState&& InCompiledState)
{
	VertexShader = std::move(InCompiledState.VertexShader);
	PixelShader = std::move(InCompiledState.PixelShader);
	InputLayout = std::move(InCompiledState.InputLayout);
	InterfaceSignature = std::move(InCompiledState.InterfaceSignature);
}

void FShader::Release()
{
	InputLayout.Reset();
	PixelShader.Reset();
	VertexShader.Reset();
	InterfaceSignature.clear();
}

void FShader::Bind(ID3D11DeviceContext* InDeviceContext) const
{
	InDeviceContext->IASetInputLayout(InputLayout.Get());
	InDeviceContext->VSSetShader(VertexShader.Get(), nullptr, 0);
	InDeviceContext->PSSetShader(PixelShader.Get(), nullptr, 0);
}

void FComputeShader::Create(ID3D11Device* InDevice, const wchar_t* InFilePath, const char* InCSEntryPoint,
                            const D3D_SHADER_MACRO* InDefines)
{
    TComPtr<ID3DBlob> computeShaderCSO;
    TComPtr<ID3DBlob> errorBlob;

    HRESULT hr = D3DCompileFromFile(InFilePath, InDefines, D3D_COMPILE_STANDARD_FILE_INCLUDE, InCSEntryPoint, "cs_5_0",
                                    0, 0, computeShaderCSO.GetAddressOf(), errorBlob.GetAddressOf());
    if (FAILED(hr))
    {
        if (errorBlob)
        {
            MessageBoxA(nullptr, (char*)errorBlob->GetBufferPointer(), "Compute Shader Compile Error",
                        MB_OK | MB_ICONERROR);
        }
        return;
    }

    hr = InDevice->CreateComputeShader(computeShaderCSO->GetBufferPointer(), computeShaderCSO->GetBufferSize(),
                                       nullptr, ComputeShader.ReleaseAndGetAddressOf());
    if (FAILED(hr))
    {
        std::cerr << "Failed to create Compute Shader (HRESULT: " << hr << ")" << std::endl;
    }
}

void FComputeShader::Release() { ComputeShader.Reset(); }

void FComputeShader::Bind(ID3D11DeviceContext* InDeviceContext) const
{
    InDeviceContext->CSSetShader(ComputeShader.Get(), nullptr, 0);
}
