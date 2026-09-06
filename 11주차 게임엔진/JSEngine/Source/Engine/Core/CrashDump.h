#pragma once

#include <Windows.h>

#include <string>
#include <vector>

inline constexpr DWORD CRASH_EXCEPTION_CODE_MANUAL = 0xE0000001u;
inline constexpr DWORD CRASH_EXCEPTION_CODE_STD_EXCEPTION = 0xE0000002u;
inline constexpr DWORD CRASH_EXCEPTION_CODE_UNKNOWN_CPP_EXCEPTION = 0xE0000003u;
inline constexpr DWORD CRASH_EXCEPTION_CODE_CAPTURED_THREAD = 0xE0000004u;
inline constexpr DWORD CRASH_EXCEPTION_CODE_SNAPSHOT = 0xE0000005u;

struct FCrashContext
{
	DWORD ThreadId = 0;
	HANDLE ThreadHandle = nullptr;
	CONTEXT ContextRecord = {};
	EXCEPTION_RECORD ExceptionRecord = {};
	EXCEPTION_POINTERS ExceptionPointers = {};
	std::string Message;

	FCrashContext();
	~FCrashContext();

	FCrashContext(const FCrashContext& Other);
	FCrashContext& operator=(const FCrashContext& Other);

	FCrashContext(FCrashContext&& Other) noexcept;
	FCrashContext& operator=(FCrashContext&& Other) noexcept;

	void ResetExceptionPointers();
	void CloseThreadHandle();
	void SetThreadHandle(HANDLE InThreadHandle);
	void AdoptThreadHandle(HANDLE InThreadHandle);
	bool HasExceptionData() const;
};

struct FThreadSnapshotInfo
{
	DWORD ThreadId = 0;
	bool bIsCurrentThread = false;
};

enum class ECrashArtifactFailureStage
{
	None,
	InvalidThreadId,
	SelfCapture,
	OpenThread,
	DuplicateThreadHandle,
	SuspendThread,
	GetThreadContext,
	ResumeThread,
	DumpWrite,
	LogWrite,
};

struct FCrashCaptureResult
{
	bool bSuccess = false;
	ECrashArtifactFailureStage FailureStage = ECrashArtifactFailureStage::None;
	DWORD ErrorCode = ERROR_SUCCESS;
};

struct FCrashArtifactResult
{
	bool bContextCaptured = false;
	bool bDumpWritten = false;
	bool bLogWritten = false;
	ECrashArtifactFailureStage FailureStage = ECrashArtifactFailureStage::None;
	DWORD FailureErrorCode = ERROR_SUCCESS;
	DWORD CaptureErrorCode = ERROR_SUCCESS;
	DWORD DumpErrorCode = ERROR_SUCCESS;
	DWORD LogErrorCode = ERROR_SUCCESS;
	std::wstring DumpPath;
	std::wstring LogPath;

	bool IsSuccess() const
	{
		return bContextCaptured && bDumpWritten && bLogWritten && FailureStage == ECrashArtifactFailureStage::None;
	}
};

const char* GetCrashArtifactFailureStageString(ECrashArtifactFailureStage Stage);
const char* DescribeCrashArtifactFailure(const FCrashArtifactResult& Result);
std::string BuildCrashArtifactConsoleSummary(const FCrashArtifactResult& Result, bool bUseRelativePaths = true);
std::wstring BuildCrashArtifactDialogMessage(const FCrashArtifactResult& Result);
void InstallCrashHandler();
FCrashContext CaptureCrashContext(EXCEPTION_POINTERS* ExceptionInfo, const char* Message = nullptr);
FCrashContext CaptureCurrentThreadContext(DWORD ExceptionCode, const char* Message = nullptr);
FCrashCaptureResult CaptureOtherThreadContext(HANDLE ThreadHandle, DWORD ThreadId, FCrashContext& OutContext, const char* Message = nullptr);
bool EnumerateProcessThreads(std::vector<FThreadSnapshotInfo>& OutThreads);
FCrashArtifactResult WriteCrashArtifacts(const FCrashContext& CrashContext);
bool WriteCrashDump(const FCrashContext& CrashContext, const std::wstring& DumpPath, DWORD* OutErrorCode = nullptr);
bool WriteCrashLog(const FCrashContext& CrashContext, const std::wstring& LogPath, DWORD* OutErrorCode = nullptr);
FCrashArtifactResult WriteCurrentThreadSnapshot(const char* Message = nullptr);
FCrashArtifactResult WriteCapturedThreadSnapshot(DWORD ThreadId, const char* Message = nullptr);
LONG WINAPI ReportCrash(EXCEPTION_POINTERS* ExceptionInfo);
[[noreturn]] void RaiseCrashException(DWORD ExceptionCode, const char* Message = nullptr);
[[noreturn]] void CauseCrash();
