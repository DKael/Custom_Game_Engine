#include "Engine/Core/CrashDump.h"

#include <TlHelp32.h>
#include <algorithm>
#include <cstdint>
#include <utility>

namespace
{
HANDLE DuplicateThreadHandle(HANDLE ThreadHandle)
{
	if (!ThreadHandle)
	{
		return nullptr;
	}

	HANDLE DuplicatedHandle = nullptr;
	if (!DuplicateHandle(
		GetCurrentProcess(),
		ThreadHandle,
		GetCurrentProcess(),
		&DuplicatedHandle,
		0,
		FALSE,
		DUPLICATE_SAME_ACCESS))
	{
		return nullptr;
	}

	return DuplicatedHandle;
}

DWORD64 GetInstructionPointer(const CONTEXT& ContextRecord)
{
#if defined(_M_X64)
	return ContextRecord.Rip;
#elif defined(_M_IX86)
	return ContextRecord.Eip;
#else
	return 0;
#endif
}

void InitializeSyntheticExceptionRecord(EXCEPTION_RECORD& ExceptionRecord, DWORD ExceptionCode, const CONTEXT& ContextRecord)
{
	ExceptionRecord = {};
	ExceptionRecord.ExceptionCode = ExceptionCode;
	ExceptionRecord.ExceptionFlags = EXCEPTION_NONCONTINUABLE;
	ExceptionRecord.ExceptionAddress = reinterpret_cast<PVOID>(static_cast<uintptr_t>(GetInstructionPointer(ContextRecord)));
}
}

FCrashContext::FCrashContext()
{
	ResetExceptionPointers();
}

FCrashContext::~FCrashContext()
{
	CloseThreadHandle();
}

FCrashContext::FCrashContext(const FCrashContext& Other)
	: ThreadId(Other.ThreadId)
	, ContextRecord(Other.ContextRecord)
	, ExceptionRecord(Other.ExceptionRecord)
	, Message(Other.Message)
{
	SetThreadHandle(Other.ThreadHandle);
	ResetExceptionPointers();
}

FCrashContext& FCrashContext::operator=(const FCrashContext& Other)
{
	if (this == &Other)
	{
		return *this;
	}

	ThreadId = Other.ThreadId;
	ContextRecord = Other.ContextRecord;
	ExceptionRecord = Other.ExceptionRecord;
	Message = Other.Message;
	SetThreadHandle(Other.ThreadHandle);
	ResetExceptionPointers();
	return *this;
}

FCrashContext::FCrashContext(FCrashContext&& Other) noexcept
	: ThreadId(Other.ThreadId)
	, ThreadHandle(Other.ThreadHandle)
	, ContextRecord(Other.ContextRecord)
	, ExceptionRecord(Other.ExceptionRecord)
	, Message(std::move(Other.Message))
{
	ResetExceptionPointers();
	Other.ThreadId = 0;
	Other.ThreadHandle = nullptr;
	Other.ContextRecord = {};
	Other.ExceptionRecord = {};
	Other.ResetExceptionPointers();
}

FCrashContext& FCrashContext::operator=(FCrashContext&& Other) noexcept
{
	if (this == &Other)
	{
		return *this;
	}

	CloseThreadHandle();

	ThreadId = Other.ThreadId;
	ThreadHandle = Other.ThreadHandle;
	ContextRecord = Other.ContextRecord;
	ExceptionRecord = Other.ExceptionRecord;
	Message = std::move(Other.Message);
	ResetExceptionPointers();

	Other.ThreadId = 0;
	Other.ThreadHandle = nullptr;
	Other.ContextRecord = {};
	Other.ExceptionRecord = {};
	Other.ResetExceptionPointers();
	return *this;
}

void FCrashContext::ResetExceptionPointers()
{
	// EXCEPTION_POINTERS must always reference the owned deep-copy fields in this struct.
	ExceptionPointers.ExceptionRecord = &ExceptionRecord;
	ExceptionPointers.ContextRecord = &ContextRecord;
}

void FCrashContext::CloseThreadHandle()
{
	if (ThreadHandle)
	{
		CloseHandle(ThreadHandle);
		ThreadHandle = nullptr;
	}
}

void FCrashContext::SetThreadHandle(HANDLE InThreadHandle)
{
	CloseThreadHandle();
	ThreadHandle = DuplicateThreadHandle(InThreadHandle);
}

void FCrashContext::AdoptThreadHandle(HANDLE InThreadHandle)
{
	CloseThreadHandle();
	ThreadHandle = InThreadHandle;
}

bool FCrashContext::HasExceptionData() const
{
	return ExceptionPointers.ExceptionRecord != nullptr && ExceptionPointers.ContextRecord != nullptr;
}

FCrashContext CaptureCrashContext(EXCEPTION_POINTERS* ExceptionInfo, const char* Message)
{
	if (!ExceptionInfo)
	{
		return CaptureCurrentThreadContext(0u, Message);
	}

	FCrashContext CrashContext;
	CrashContext.ThreadId = GetCurrentThreadId();
	CrashContext.SetThreadHandle(GetCurrentThread());
	CrashContext.Message = Message ? Message : "";

	if (ExceptionInfo->ContextRecord)
	{
		CrashContext.ContextRecord = *ExceptionInfo->ContextRecord;
	}
	if (ExceptionInfo->ExceptionRecord)
	{
		CrashContext.ExceptionRecord = *ExceptionInfo->ExceptionRecord;
	}

	CrashContext.ResetExceptionPointers();
	return CrashContext;
}

FCrashContext CaptureCurrentThreadContext(DWORD ExceptionCode, const char* Message)
{
	FCrashContext CrashContext;
	CrashContext.ThreadId = GetCurrentThreadId();
	CrashContext.SetThreadHandle(GetCurrentThread());
	CrashContext.Message = Message ? Message : "";

	RtlCaptureContext(&CrashContext.ContextRecord);
	InitializeSyntheticExceptionRecord(CrashContext.ExceptionRecord, ExceptionCode, CrashContext.ContextRecord);
	CrashContext.ResetExceptionPointers();
	return CrashContext;
}

FCrashCaptureResult CaptureOtherThreadContext(HANDLE ThreadHandle, DWORD ThreadId, FCrashContext& OutContext, const char* Message)
{
	FCrashCaptureResult Result;
	if (!ThreadHandle || ThreadId == 0 || ThreadId == GetCurrentThreadId())
	{
		Result.FailureStage = ThreadId == GetCurrentThreadId()
			? ECrashArtifactFailureStage::SelfCapture
			: ECrashArtifactFailureStage::InvalidThreadId;
		Result.ErrorCode = ERROR_INVALID_PARAMETER;
		return Result;
	}

	HANDLE OwnedThreadHandle = DuplicateThreadHandle(ThreadHandle);
	if (!OwnedThreadHandle)
	{
		Result.FailureStage = ECrashArtifactFailureStage::DuplicateThreadHandle;
		Result.ErrorCode = GetLastError();
		return Result;
	}

	const DWORD SuspendCount = SuspendThread(OwnedThreadHandle);
	if (SuspendCount == static_cast<DWORD>(-1))
	{
		Result.FailureStage = ECrashArtifactFailureStage::SuspendThread;
		Result.ErrorCode = GetLastError();
		CloseHandle(OwnedThreadHandle);
		return Result;
	}

	CONTEXT CapturedContext = {};
	CapturedContext.ContextFlags = CONTEXT_FULL;
	const BOOL bGotContext = GetThreadContext(OwnedThreadHandle, &CapturedContext);
	const DWORD GetContextErrorCode = bGotContext ? ERROR_SUCCESS : GetLastError();
	const DWORD ResumeResult = ResumeThread(OwnedThreadHandle);
	if (ResumeResult == static_cast<DWORD>(-1))
	{
		Result.FailureStage = ECrashArtifactFailureStage::ResumeThread;
		Result.ErrorCode = GetLastError();
		CloseHandle(OwnedThreadHandle);
		return Result;
	}

	if (!bGotContext)
	{
		Result.FailureStage = ECrashArtifactFailureStage::GetThreadContext;
		Result.ErrorCode = GetContextErrorCode;
		CloseHandle(OwnedThreadHandle);
		return Result;
	}

	FCrashContext CrashContext;
	CrashContext.ThreadId = ThreadId;
	CrashContext.AdoptThreadHandle(OwnedThreadHandle);
	CrashContext.ContextRecord = CapturedContext;
	CrashContext.Message = Message ? Message : "";
	InitializeSyntheticExceptionRecord(CrashContext.ExceptionRecord, CRASH_EXCEPTION_CODE_CAPTURED_THREAD, CrashContext.ContextRecord);
	CrashContext.ResetExceptionPointers();

	OutContext = std::move(CrashContext);
	Result.bSuccess = true;
	return Result;
}

bool EnumerateProcessThreads(std::vector<FThreadSnapshotInfo>& OutThreads)
{
	OutThreads.clear();

	const HANDLE SnapshotHandle = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
	if (SnapshotHandle == INVALID_HANDLE_VALUE)
	{
		return false;
	}

	const DWORD CurrentProcessId = GetCurrentProcessId();
	const DWORD CurrentThreadId = GetCurrentThreadId();

	THREADENTRY32 ThreadEntry = {};
	ThreadEntry.dwSize = sizeof(ThreadEntry);

	if (!Thread32First(SnapshotHandle, &ThreadEntry))
	{
		CloseHandle(SnapshotHandle);
		return false;
	}

	do
	{
		if (ThreadEntry.th32OwnerProcessID != CurrentProcessId)
		{
			continue;
		}

		FThreadSnapshotInfo ThreadInfo;
		ThreadInfo.ThreadId = ThreadEntry.th32ThreadID;
		ThreadInfo.bIsCurrentThread = ThreadEntry.th32ThreadID == CurrentThreadId;
		OutThreads.push_back(ThreadInfo);
	} while (Thread32Next(SnapshotHandle, &ThreadEntry));

	CloseHandle(SnapshotHandle);

	std::sort(OutThreads.begin(), OutThreads.end(),
		[](const FThreadSnapshotInfo& A, const FThreadSnapshotInfo& B)
		{
			return A.ThreadId < B.ThreadId;
		});

	return true;
}
