#include "ShaderCompilationUtils.h"

#include "Core/Paths.h"
#include "Editor/UI/EditorConsoleWidget.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace
{
	void LogShaderMessage(const std::string& Message)
	{
		UE_LOG("%s", Message.c_str());
		OutputDebugStringA((Message + "\n").c_str());
	}

	std::string TrimCopy(const std::string& Value)
	{
		size_t Start = 0;
		while (Start < Value.size() && std::isspace(static_cast<unsigned char>(Value[Start])) != 0)
		{
			++Start;
		}

		size_t End = Value.size();
		while (End > Start && std::isspace(static_cast<unsigned char>(Value[End - 1])) != 0)
		{
			--End;
		}

		return Value.substr(Start, End - Start);
	}

	std::string NormalizeWhitespace(const std::string& Value)
	{
		std::string Result;
		Result.reserve(Value.size());

		bool bPreviousWasWhitespace = false;
		for (char Character : Value)
		{
			if (std::isspace(static_cast<unsigned char>(Character)) != 0)
			{
				if (!bPreviousWasWhitespace)
				{
					Result.push_back(' ');
					bPreviousWasWhitespace = true;
				}
				continue;
			}

			Result.push_back(Character);
			bPreviousWasWhitespace = false;
		}

		return TrimCopy(Result);
	}

	bool IsIdentifierCharacter(char Character)
	{
		return std::isalnum(static_cast<unsigned char>(Character)) != 0 || Character == '_';
	}

	bool IsStandaloneToken(const std::string& Source, size_t Position, const char* Token)
	{
		const size_t TokenLength = std::strlen(Token);
		if (Source.compare(Position, TokenLength, Token) != 0)
		{
			return false;
		}

		const bool bHasLeftIdentifier = Position > 0 && IsIdentifierCharacter(Source[Position - 1]);
		const bool bHasRightIdentifier =
			Position + TokenLength < Source.size() && IsIdentifierCharacter(Source[Position + TokenLength]);
		return !bHasLeftIdentifier && !bHasRightIdentifier;
	}

	std::string RemoveComments(const std::string& Source)
	{
		std::string Result;
		Result.reserve(Source.size());

		bool bInLineComment = false;
		bool bInBlockComment = false;
		for (size_t Index = 0; Index < Source.size(); ++Index)
		{
			const char Character = Source[Index];
			const char NextCharacter = Index + 1 < Source.size() ? Source[Index + 1] : '\0';

			if (bInLineComment)
			{
				if (Character == '\n')
				{
					bInLineComment = false;
					Result.push_back(Character);
				}
				continue;
			}

			if (bInBlockComment)
			{
				if (Character == '*' && NextCharacter == '/')
				{
					bInBlockComment = false;
					++Index;
				}
				continue;
			}

			if (Character == '/' && NextCharacter == '/')
			{
				bInLineComment = true;
				++Index;
				continue;
			}

			if (Character == '/' && NextCharacter == '*')
			{
				bInBlockComment = true;
				++Index;
				continue;
			}

			Result.push_back(Character);
		}

		return Result;
	}

	bool ReadTextFile(const std::wstring& FilePath, std::string& OutText)
	{
		std::ifstream File(std::filesystem::path(FilePath), std::ios::binary);
		if (!File.is_open())
		{
			return false;
		}

		std::ostringstream Stream;
		Stream << File.rdbuf();
		OutText = Stream.str();
		return true;
	}

	void ExtractCBufferBlocks(const std::string& Source, std::vector<std::string>& OutDeclarations)
	{
		size_t SearchPosition = 0;
		while (true)
		{
			const size_t CBufferPosition = Source.find("cbuffer", SearchPosition);
			if (CBufferPosition == std::string::npos)
			{
				return;
			}

			if (!IsStandaloneToken(Source, CBufferPosition, "cbuffer"))
			{
				SearchPosition = CBufferPosition + 7;
				continue;
			}

			const size_t OpenBracePosition = Source.find('{', CBufferPosition);
			if (OpenBracePosition == std::string::npos)
			{
				return;
			}

			int BraceDepth = 1;
			size_t CloseBracePosition = OpenBracePosition + 1;
			while (CloseBracePosition < Source.size() && BraceDepth > 0)
			{
				if (Source[CloseBracePosition] == '{')
				{
					++BraceDepth;
				}
				else if (Source[CloseBracePosition] == '}')
				{
					--BraceDepth;
				}

				++CloseBracePosition;
			}

			if (BraceDepth != 0)
			{
				return;
			}

			size_t DeclarationEnd = Source.find(';', CloseBracePosition);
			if (DeclarationEnd == std::string::npos)
			{
				DeclarationEnd = CloseBracePosition - 1;
			}

			OutDeclarations.push_back(NormalizeWhitespace(Source.substr(CBufferPosition, DeclarationEnd - CBufferPosition + 1)));
			SearchPosition = DeclarationEnd + 1;
		}
	}

	void ExtractResourceDeclarations(const std::string& Source, std::vector<std::string>& OutDeclarations)
	{
		static const char* ResourceKeywords[] = {
			"Texture", "Sampler", "StructuredBuffer", "RWStructuredBuffer", "ByteAddressBuffer", "RWByteAddressBuffer",
			"AppendStructuredBuffer", "ConsumeStructuredBuffer", "ConstantBuffer", "RWTexture", "Buffer"};

		size_t StatementStart = 0;
		while (StatementStart < Source.size())
		{
			size_t StatementEnd = Source.find(';', StatementStart);
			if (StatementEnd == std::string::npos)
			{
				StatementEnd = Source.size();
			}

			const std::string Statement = NormalizeWhitespace(Source.substr(StatementStart, StatementEnd - StatementStart));
			StatementStart = StatementEnd + 1;

			if (Statement.empty() || Statement.find('{') != std::string::npos || Statement.find('}') != std::string::npos)
			{
				continue;
			}

			bool bIsResourceDeclaration = false;
			for (const char* Keyword : ResourceKeywords)
			{
				if (Statement.find(Keyword) != std::string::npos)
				{
					bIsResourceDeclaration = true;
					break;
				}
			}

			if (bIsResourceDeclaration)
			{
				OutDeclarations.push_back(Statement + ';');
			}
		}
	}

	bool BuildShaderInterfaceSignature(const std::wstring& FilePath, std::string& OutSignature, std::string& OutError)
	{
		std::string Source;
		if (!ReadTextFile(FilePath, Source))
		{
			OutError = "failed to read shader source";
			return false;
		}

		const std::string CleanSource = RemoveComments(Source);
		std::vector<std::string> Declarations;
		ExtractCBufferBlocks(CleanSource, Declarations);
		ExtractResourceDeclarations(CleanSource, Declarations);

		std::sort(Declarations.begin(), Declarations.end());

		std::ostringstream Stream;
		for (const std::string& Declaration : Declarations)
		{
			Stream << Declaration << '\n';
		}

		OutSignature = Stream.str();
		return true;
	}

	std::string FormatShaderLog(const std::wstring& FilePath, const std::string& Message)
	{
		std::ostringstream Stream;
		Stream << "[Shader] " << FPaths::ToUtf8(FilePath) << " - " << Message;
		return Stream.str();
	}
}

bool ShaderCompilationUtils::CompileShader(
	const FCompileRequest& Request,
	FShaderCompiledState& OutCompiledState,
	std::string* OutFailureMessage)
{
	if (Request.Device == nullptr || Request.FilePath.empty())
	{
		if (OutFailureMessage != nullptr)
		{
			*OutFailureMessage = "invalid device or shader path";
		}
		return false;
	}

	std::vector<D3D_SHADER_MACRO> Defines;
	if (Request.MacroDefinitions != nullptr && !Request.MacroDefinitions->empty())
	{
		Defines.reserve(Request.MacroDefinitions->size() + 1);
		for (const auto& MacroDefinition : *Request.MacroDefinitions)
		{
			D3D_SHADER_MACRO Macro = {};
			Macro.Name = MacroDefinition.first.c_str();
			Macro.Definition = MacroDefinition.second.empty() ? nullptr : MacroDefinition.second.c_str();
			Defines.push_back(Macro);
		}
		D3D_SHADER_MACRO NullMacro = {};
		Defines.push_back(NullMacro);
	}
	const D3D_SHADER_MACRO* DefineData = Defines.empty() ? nullptr : Defines.data();

		TComPtr<ID3DBlob> VertexShaderCSO;
        TComPtr<ID3DBlob> PixelShaderCSO;
        TComPtr<ID3DBlob> ErrorBlob;

        HRESULT Hr = D3DCompileFromFile(Request.FilePath.c_str(), DefineData, D3D_COMPILE_STANDARD_FILE_INCLUDE,
                                        Request.VSEntryPoint.c_str(), "vs_5_0", 0, 0, VertexShaderCSO.GetAddressOf(),
                                        ErrorBlob.GetAddressOf());
        if (FAILED(Hr))
        {
            const std::string ErrorMessage =
                ErrorBlob ? static_cast<const char*>(ErrorBlob->GetBufferPointer()) : "vertex shader compile failed";
            if (OutFailureMessage != nullptr)
            {
                *OutFailureMessage = ErrorMessage;
            }
            if (Request.bLogFailures)
            {
                LogShaderMessage(FormatShaderLog(Request.FilePath, ErrorMessage));
            }
            return false;
        }

        const bool bHasPixelShader = !Request.PSEntryPoint.empty();
        if (bHasPixelShader)
        {
            ErrorBlob.Reset();
            Hr = D3DCompileFromFile(Request.FilePath.c_str(), DefineData, D3D_COMPILE_STANDARD_FILE_INCLUDE,
                                    Request.PSEntryPoint.c_str(), "ps_5_0", 0, 0, PixelShaderCSO.GetAddressOf(),
                                    ErrorBlob.GetAddressOf());
            if (FAILED(Hr))
            {
                const std::string ErrorMessage =
                    ErrorBlob ? static_cast<const char*>(ErrorBlob->GetBufferPointer()) : "pixel shader compile failed";
                if (OutFailureMessage != nullptr)
                {
                    *OutFailureMessage = ErrorMessage;
                }
                if (Request.bLogFailures)
                {
                    LogShaderMessage(FormatShaderLog(Request.FilePath, ErrorMessage));
                }
                return false;
            }
        }

	std::string NewInterfaceSignature;
	std::string SignatureError;
	if (!BuildShaderInterfaceSignature(Request.FilePath, NewInterfaceSignature, SignatureError))
	{
		const std::string FailureMessage = "interface scan failed: " + SignatureError;
		if (OutFailureMessage != nullptr)
		{
			*OutFailureMessage = FailureMessage;
		}
		if (Request.bLogFailures)
		{
			LogShaderMessage(FormatShaderLog(Request.FilePath, FailureMessage));
		}
		return false;
	}

	if (!Request.bAllowInterfaceChanges &&
		Request.CurrentInterfaceSignature != nullptr &&
		!Request.CurrentInterfaceSignature->empty() &&
		*Request.CurrentInterfaceSignature != NewInterfaceSignature)
	{
		const std::string FailureMessage = "hot reload blocked due to interface declaration change";
		if (OutFailureMessage != nullptr)
		{
			*OutFailureMessage = FailureMessage;
		}
		if (Request.bLogFailures)
		{
			LogShaderMessage(FormatShaderLog(Request.FilePath, FailureMessage));
		}
		return false;
	}

		Hr = Request.Device->CreateVertexShader(VertexShaderCSO->GetBufferPointer(), VertexShaderCSO->GetBufferSize(),
                                                nullptr, OutCompiledState.VertexShader.ReleaseAndGetAddressOf());
        if (FAILED(Hr))
        {
            if (OutFailureMessage != nullptr)
            {
                *OutFailureMessage = "failed to create vertex shader";
            }
            if (Request.bLogFailures)
            {
                LogShaderMessage(FormatShaderLog(Request.FilePath, "failed to create vertex shader"));
            }
            return false;
        }

        if (bHasPixelShader)
        {
            Hr = Request.Device->CreatePixelShader(PixelShaderCSO->GetBufferPointer(), PixelShaderCSO->GetBufferSize(),
                                                   nullptr, OutCompiledState.PixelShader.ReleaseAndGetAddressOf());
            if (FAILED(Hr))
            {
                if (OutFailureMessage != nullptr)
                {
                    *OutFailureMessage = "failed to create pixel shader";
                }
                if (Request.bLogFailures)
                {
                    LogShaderMessage(FormatShaderLog(Request.FilePath, "failed to create pixel shader"));
                }
                return false;
            }
        }

	if (Request.InputElements != nullptr && !Request.InputElements->empty())
	{
		Hr = Request.Device->CreateInputLayout(
			Request.InputElements->data(),
			static_cast<UINT>(Request.InputElements->size()),
			VertexShaderCSO->GetBufferPointer(),
			VertexShaderCSO->GetBufferSize(),
			OutCompiledState.InputLayout.ReleaseAndGetAddressOf());
		if (FAILED(Hr))
		{
			if (OutFailureMessage != nullptr)
			{
				*OutFailureMessage = "failed to create input layout";
			}
			if (Request.bLogFailures)
			{
				LogShaderMessage(FormatShaderLog(Request.FilePath, "failed to create input layout"));
			}
			return false;
		}
	}

	OutCompiledState.InterfaceSignature = std::move(NewInterfaceSignature);
	return true;
}
