#include "Engine/Core/CrashDump.h"
#include "Engine/Core/Paths.h"

namespace
{
const char* GetFailureStageStringInternal(ECrashArtifactFailureStage Stage)
{
	switch (Stage)
	{
	case ECrashArtifactFailureStage::None:
		return "None";
	case ECrashArtifactFailureStage::InvalidThreadId:
		return "InvalidThreadId";
	case ECrashArtifactFailureStage::SelfCapture:
		return "SelfCapture";
	case ECrashArtifactFailureStage::OpenThread:
		return "OpenThread";
	case ECrashArtifactFailureStage::DuplicateThreadHandle:
		return "DuplicateThreadHandle";
	case ECrashArtifactFailureStage::SuspendThread:
		return "SuspendThread";
	case ECrashArtifactFailureStage::GetThreadContext:
		return "GetThreadContext";
	case ECrashArtifactFailureStage::ResumeThread:
		return "ResumeThread";
	case ECrashArtifactFailureStage::DumpWrite:
		return "DumpWrite";
	case ECrashArtifactFailureStage::LogWrite:
		return "LogWrite";
	default:
		return "Unknown";
	}
}

const wchar_t* GetFailureStageStringWide(ECrashArtifactFailureStage Stage)
{
	switch (Stage)
	{
	case ECrashArtifactFailureStage::None:
		return L"None";
	case ECrashArtifactFailureStage::InvalidThreadId:
		return L"InvalidThreadId";
	case ECrashArtifactFailureStage::SelfCapture:
		return L"SelfCapture";
	case ECrashArtifactFailureStage::OpenThread:
		return L"OpenThread";
	case ECrashArtifactFailureStage::DuplicateThreadHandle:
		return L"DuplicateThreadHandle";
	case ECrashArtifactFailureStage::SuspendThread:
		return L"SuspendThread";
	case ECrashArtifactFailureStage::GetThreadContext:
		return L"GetThreadContext";
	case ECrashArtifactFailureStage::ResumeThread:
		return L"ResumeThread";
	case ECrashArtifactFailureStage::DumpWrite:
		return L"DumpWrite";
	case ECrashArtifactFailureStage::LogWrite:
		return L"LogWrite";
	default:
		return L"Unknown";
	}
}

const char* GetFailureDescriptionInternal(ECrashArtifactFailureStage Stage)
{
	switch (Stage)
	{
	case ECrashArtifactFailureStage::None:
		return "No failure.";
	case ECrashArtifactFailureStage::InvalidThreadId:
		return "Invalid target thread id.";
	case ECrashArtifactFailureStage::SelfCapture:
		return "The target thread is the current thread.";
	case ECrashArtifactFailureStage::OpenThread:
		return "Failed to open the target thread.";
	case ECrashArtifactFailureStage::DuplicateThreadHandle:
		return "Failed to duplicate the target thread handle.";
	case ECrashArtifactFailureStage::SuspendThread:
		return "Failed to suspend the target thread.";
	case ECrashArtifactFailureStage::GetThreadContext:
		return "Failed to capture the target thread context.";
	case ECrashArtifactFailureStage::ResumeThread:
		return "Failed to resume the target thread.";
	case ECrashArtifactFailureStage::DumpWrite:
		return "Failed to write the mini dump file.";
	case ECrashArtifactFailureStage::LogWrite:
		return "Failed to write the crash log file.";
	default:
		return "Unknown crash artifact failure.";
	}
}

std::string ToConsolePath(const std::wstring& Path, bool bUseRelativePath)
{
	if (Path.empty())
	{
		return "<not generated>";
	}

	return bUseRelativePath
		? FPaths::ToProjectRelativePath(FPaths::ToString(Path))
		: FPaths::ToString(Path);
}

void AppendArtifactPathLine(std::wstring& Message, const wchar_t* Label, const std::wstring& Path, bool bSucceeded)
{
	Message += Label;
	Message += Path.empty() ? L"<not generated>" : Path;
	if (!bSucceeded)
	{
		Message += L" [failed]";
	}
	Message += L"\n";
}

void AppendConsoleArtifactPathLine(std::string& Message, const char* Label, const std::wstring& Path, bool bSucceeded, bool bUseRelativePaths)
{
	Message += Label;
	Message += ToConsolePath(Path, bUseRelativePaths);
	if (!bSucceeded)
	{
		Message += " [failed]";
	}
	Message += "\n";
}
}

const char* GetCrashArtifactFailureStageString(ECrashArtifactFailureStage Stage)
{
	return GetFailureStageStringInternal(Stage);
}

const char* DescribeCrashArtifactFailure(const FCrashArtifactResult& Result)
{
	return GetFailureDescriptionInternal(Result.FailureStage);
}

std::string BuildCrashArtifactConsoleSummary(const FCrashArtifactResult& Result, bool bUseRelativePaths)
{
	std::string Message;
	AppendConsoleArtifactPathLine(Message, "  Dump: ", Result.DumpPath, Result.bDumpWritten, bUseRelativePaths);
	AppendConsoleArtifactPathLine(Message, "  Log : ", Result.LogPath, Result.bLogWritten, bUseRelativePaths);

	if (Result.FailureStage != ECrashArtifactFailureStage::None)
	{
		Message += "  Failure: ";
		Message += DescribeCrashArtifactFailure(Result);
		Message += "\n";
		Message += "  Failure Stage: ";
		Message += GetCrashArtifactFailureStageString(Result.FailureStage);
		Message += "\n";
	}
	if (Result.FailureErrorCode != ERROR_SUCCESS)
	{
		Message += "  Error Code: ";
		Message += std::to_string(Result.FailureErrorCode);
		Message += "\n";
	}
	if (Result.CaptureErrorCode != ERROR_SUCCESS)
	{
		Message += "  Capture Error: ";
		Message += std::to_string(Result.CaptureErrorCode);
		Message += "\n";
	}
	if (Result.DumpErrorCode != ERROR_SUCCESS)
	{
		Message += "  Dump Error: ";
		Message += std::to_string(Result.DumpErrorCode);
		Message += "\n";
	}
	if (Result.LogErrorCode != ERROR_SUCCESS)
	{
		Message += "  Log Error : ";
		Message += std::to_string(Result.LogErrorCode);
		Message += "\n";
	}

	return Message;
}

std::wstring BuildCrashArtifactDialogMessage(const FCrashArtifactResult& Result)
{
	std::wstring Message = Result.IsSuccess()
		? L"Engine crash detected.\n"
		: L"Engine crash detected, but artifact generation was incomplete.\n";

	AppendArtifactPathLine(Message, L"Dump: ", Result.DumpPath, Result.bDumpWritten);
	AppendArtifactPathLine(Message, L"Log : ", Result.LogPath, Result.bLogWritten);
	if (Result.FailureStage != ECrashArtifactFailureStage::None)
	{
		Message += L"Failure: ";
		const std::string FailureDescription = DescribeCrashArtifactFailure(Result);
		Message += std::wstring(FailureDescription.begin(), FailureDescription.end());
		Message += L"\n";
		Message += L"Failure Stage: ";
		Message += GetFailureStageStringWide(Result.FailureStage);
		Message += L"\n";
	}
	if (Result.FailureErrorCode != ERROR_SUCCESS)
	{
		Message += L"Error Code: ";
		Message += std::to_wstring(Result.FailureErrorCode);
		Message += L"\n";
	}

	return Message;
}
