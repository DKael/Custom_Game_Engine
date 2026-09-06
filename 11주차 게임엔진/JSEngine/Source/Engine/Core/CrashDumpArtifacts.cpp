#include "Engine/Core/CrashDump.h"
#include "Engine/Core/Paths.h"

#include <DbgHelp.h>
#include <algorithm>
#include <cstdint>
#include <cwchar>
#include <filesystem>
#include <fstream>

#pragma comment(lib, "DbgHelp.lib")

namespace
{
std::wstring BuildCrashFilePath(const wchar_t* Prefix, const wchar_t* Extension, DWORD ThreadId)
{
	FPaths::CreateDir(FPaths::DumpDir());

	SYSTEMTIME LocalTime;
	GetLocalTime(&LocalTime);

	WCHAR FileName[MAX_PATH];
	swprintf_s(FileName, L"%s_%04u%02u%02u_%02u%02u%02u_%03u_pid%lu_tid%lu.%s",
		Prefix,
		LocalTime.wYear, LocalTime.wMonth, LocalTime.wDay,
		LocalTime.wHour, LocalTime.wMinute, LocalTime.wSecond, LocalTime.wMilliseconds,
		GetCurrentProcessId(),
		ThreadId,
		Extension);

	return FPaths::Combine(FPaths::DumpDir(), FileName);
}

bool IsSnapshotContext(const FCrashContext& CrashContext)
{
	return CrashContext.ExceptionRecord.ExceptionCode == CRASH_EXCEPTION_CODE_SNAPSHOT
		|| CrashContext.ExceptionRecord.ExceptionCode == CRASH_EXCEPTION_CODE_CAPTURED_THREAD;
}

const wchar_t* GetDumpFilePrefix(const FCrashContext& CrashContext)
{
	return IsSnapshotContext(CrashContext) ? L"Snapshot" : L"Crash";
}

const wchar_t* GetLogFilePrefix(const FCrashContext& CrashContext)
{
	return IsSnapshotContext(CrashContext) ? L"SnapshotLog" : L"CrashLog";
}

const char* DescribeExceptionCode(DWORD ExceptionCode)
{
	switch (ExceptionCode)
	{
	case EXCEPTION_ACCESS_VIOLATION:
		return "EXCEPTION_ACCESS_VIOLATION";
	case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
		return "EXCEPTION_ARRAY_BOUNDS_EXCEEDED";
	case EXCEPTION_BREAKPOINT:
		return "EXCEPTION_BREAKPOINT";
	case EXCEPTION_DATATYPE_MISALIGNMENT:
		return "EXCEPTION_DATATYPE_MISALIGNMENT";
	case EXCEPTION_FLT_DENORMAL_OPERAND:
		return "EXCEPTION_FLT_DENORMAL_OPERAND";
	case EXCEPTION_FLT_DIVIDE_BY_ZERO:
		return "EXCEPTION_FLT_DIVIDE_BY_ZERO";
	case EXCEPTION_FLT_INEXACT_RESULT:
		return "EXCEPTION_FLT_INEXACT_RESULT";
	case EXCEPTION_FLT_INVALID_OPERATION:
		return "EXCEPTION_FLT_INVALID_OPERATION";
	case EXCEPTION_FLT_OVERFLOW:
		return "EXCEPTION_FLT_OVERFLOW";
	case EXCEPTION_FLT_STACK_CHECK:
		return "EXCEPTION_FLT_STACK_CHECK";
	case EXCEPTION_FLT_UNDERFLOW:
		return "EXCEPTION_FLT_UNDERFLOW";
	case EXCEPTION_ILLEGAL_INSTRUCTION:
		return "EXCEPTION_ILLEGAL_INSTRUCTION";
	case EXCEPTION_IN_PAGE_ERROR:
		return "EXCEPTION_IN_PAGE_ERROR";
	case EXCEPTION_INT_DIVIDE_BY_ZERO:
		return "EXCEPTION_INT_DIVIDE_BY_ZERO";
	case EXCEPTION_INT_OVERFLOW:
		return "EXCEPTION_INT_OVERFLOW";
	case EXCEPTION_INVALID_DISPOSITION:
		return "EXCEPTION_INVALID_DISPOSITION";
	case EXCEPTION_NONCONTINUABLE_EXCEPTION:
		return "EXCEPTION_NONCONTINUABLE_EXCEPTION";
	case EXCEPTION_PRIV_INSTRUCTION:
		return "EXCEPTION_PRIV_INSTRUCTION";
	case EXCEPTION_SINGLE_STEP:
		return "EXCEPTION_SINGLE_STEP";
	case EXCEPTION_STACK_OVERFLOW:
		return "EXCEPTION_STACK_OVERFLOW";
	case CRASH_EXCEPTION_CODE_MANUAL:
		return "MANUAL_CAUSE_CRASH";
	case CRASH_EXCEPTION_CODE_STD_EXCEPTION:
		return "CAUGHT_STD_EXCEPTION";
	case CRASH_EXCEPTION_CODE_UNKNOWN_CPP_EXCEPTION:
		return "CAUGHT_UNKNOWN_CPP_EXCEPTION";
	case CRASH_EXCEPTION_CODE_CAPTURED_THREAD:
		return "CAPTURED_THREAD_CONTEXT";
	case CRASH_EXCEPTION_CODE_SNAPSHOT:
		return "MANUAL_THREAD_SNAPSHOT";
	default:
		return "UNKNOWN_EXCEPTION";
	}
}

const char* DescribeReportType(const FCrashContext& CrashContext)
{
	if (CrashContext.ExceptionRecord.ExceptionCode == CRASH_EXCEPTION_CODE_CAPTURED_THREAD)
	{
		return "Captured Thread Snapshot";
	}

	return IsSnapshotContext(CrashContext) ? "Manual Snapshot" : "Crash";
}

bool WriteExceptionCallStack(std::ofstream& LogFile, HANDLE Process, HANDLE ThreadHandle, const CONTEXT& SourceContext)
{
	if (!ThreadHandle)
	{
		LogFile << "Call Stack: thread handle unavailable\n";
		return false;
	}

	CONTEXT ContextRecord = SourceContext;
	STACKFRAME64 StackFrame = {};
	DWORD MachineType = 0;

#if defined(_M_X64)
	MachineType = IMAGE_FILE_MACHINE_AMD64;
	StackFrame.AddrPC.Offset = ContextRecord.Rip;
	StackFrame.AddrFrame.Offset = ContextRecord.Rbp;
	StackFrame.AddrStack.Offset = ContextRecord.Rsp;
#elif defined(_M_IX86)
	MachineType = IMAGE_FILE_MACHINE_I386;
	StackFrame.AddrPC.Offset = ContextRecord.Eip;
	StackFrame.AddrFrame.Offset = ContextRecord.Ebp;
	StackFrame.AddrStack.Offset = ContextRecord.Esp;
#else
	LogFile << "Call Stack: unsupported platform\n";
	return false;
#endif

	StackFrame.AddrPC.Mode = AddrModeFlat;
	StackFrame.AddrFrame.Mode = AddrModeFlat;
	StackFrame.AddrStack.Mode = AddrModeFlat;

	char SymbolBuffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME] = {};
	SYMBOL_INFO* Symbol = reinterpret_cast<SYMBOL_INFO*>(SymbolBuffer);
	Symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
	Symbol->MaxNameLen = MAX_SYM_NAME - 1;

	LogFile << "Call Stack:\n";

	constexpr uint32_t MaxFrames = 64;
	for (uint32_t FrameIndex = 0; FrameIndex < MaxFrames; ++FrameIndex)
	{
		const DWORD64 Address = StackFrame.AddrPC.Offset;
		if (Address == 0)
		{
			break;
		}

		DWORD64 SymbolDisplacement = 0;
		IMAGEHLP_LINE64 Line = {};
		Line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
		DWORD LineDisplacement = 0;

		LogFile << "[" << FrameIndex << "] ";
		if (SymFromAddr(Process, Address, &SymbolDisplacement, Symbol))
		{
			LogFile << Symbol->Name;
			if (SymGetLineFromAddr64(Process, Address, &LineDisplacement, &Line))
			{
				LogFile << " (" << Line.FileName << ":" << Line.LineNumber << ")";
			}
			else
			{
				LogFile << " (0x" << std::hex << Address << std::dec << ")";
			}
		}
		else
		{
			LogFile << "(0x" << std::hex << Address << std::dec << ")";
		}
		LogFile << "\n";

		// StackWalk64 must use the thread represented by the captured context, not the caller thread.
		if (!StackWalk64(
			MachineType,
			Process,
			ThreadHandle,
			&StackFrame,
			&ContextRecord,
			nullptr,
			SymFunctionTableAccess64,
			SymGetModuleBase64,
			nullptr))
		{
			break;
		}
	}

	return true;
}

void RecordFirstFailure(FCrashArtifactResult& Result, ECrashArtifactFailureStage Stage, DWORD ErrorCode)
{
	if (Result.FailureStage != ECrashArtifactFailureStage::None)
	{
		return;
	}

	Result.FailureStage = Stage;
	Result.FailureErrorCode = ErrorCode;
}
}

bool WriteCrashDump(const FCrashContext& CrashContext, const std::wstring& DumpPath, DWORD* OutErrorCode)
{
	if (OutErrorCode)
	{
		*OutErrorCode = ERROR_SUCCESS;
	}

	HANDLE File = CreateFileW(
		DumpPath.c_str(),
		GENERIC_WRITE,
		0,
		nullptr,
		CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL,
		nullptr);

	if (File == INVALID_HANDLE_VALUE)
	{
		if (OutErrorCode)
		{
			*OutErrorCode = GetLastError();
		}
		return false;
	}

	MINIDUMP_EXCEPTION_INFORMATION DumpInfo = {};
	DumpInfo.ThreadId = CrashContext.ThreadId;
	DumpInfo.ExceptionPointers = const_cast<EXCEPTION_POINTERS*>(&CrashContext.ExceptionPointers);
	DumpInfo.ClientPointers = FALSE;

	const MINIDUMP_TYPE DumpType = static_cast<MINIDUMP_TYPE>(
		MiniDumpWithDataSegs |
		MiniDumpWithHandleData |
		MiniDumpWithIndirectlyReferencedMemory |
		MiniDumpWithThreadInfo);

	const BOOL bSucceeded = MiniDumpWriteDump(
		GetCurrentProcess(),
		GetCurrentProcessId(),
		File,
		DumpType,
		CrashContext.HasExceptionData() ? &DumpInfo : nullptr,
		nullptr,
		nullptr);

	const DWORD ErrorCode = bSucceeded == TRUE ? ERROR_SUCCESS : GetLastError();
	CloseHandle(File);
	if (OutErrorCode)
	{
		*OutErrorCode = ErrorCode;
	}
	return bSucceeded == TRUE;
}

bool WriteCrashLog(const FCrashContext& CrashContext, const std::wstring& LogPath, DWORD* OutErrorCode)
{
	if (OutErrorCode)
	{
		*OutErrorCode = ERROR_SUCCESS;
	}

	std::ofstream LogFile(std::filesystem::path(LogPath), std::ios::out | std::ios::trunc);
	if (!LogFile.is_open())
	{
		if (OutErrorCode)
		{
			*OutErrorCode = ERROR_OPEN_FAILED;
		}
		return false;
	}

	const DWORD ExceptionCode = CrashContext.ExceptionRecord.ExceptionCode;
	const void* ExceptionAddress = CrashContext.ExceptionRecord.ExceptionAddress;

	LogFile << "========================================\n";
	LogFile << "[Engine Crash Report]\n";
	LogFile << "Report Type: " << DescribeReportType(CrashContext) << "\n";
	LogFile << "Process Id: " << GetCurrentProcessId() << "\n";
	LogFile << "Thread Id: " << CrashContext.ThreadId << "\n";
	LogFile << "Exception Code: 0x" << std::hex << ExceptionCode << std::dec
		<< " (" << DescribeExceptionCode(ExceptionCode) << ")\n";
	LogFile << "Exception Address: 0x"
		<< std::hex << reinterpret_cast<uintptr_t>(ExceptionAddress) << std::dec << "\n";
	if (!CrashContext.Message.empty())
	{
		LogFile << "Crash Context: " << CrashContext.Message << "\n";
	}

	HANDLE Process = GetCurrentProcess();
	SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME);
	if (!SymInitialize(Process, nullptr, TRUE))
	{
		LogFile << "Call Stack: symbol initialization failed\n";
		LogFile << "========================================\n";
		LogFile.flush();
		LogFile.close();
		return true;
	}

	WriteExceptionCallStack(LogFile, Process, CrashContext.ThreadHandle, CrashContext.ContextRecord);
	LogFile << "========================================\n";

	SymCleanup(Process);
	LogFile.flush();
	const bool bWriteSucceeded = LogFile.good();
	LogFile.close();
	if (!bWriteSucceeded)
	{
		if (OutErrorCode)
		{
			*OutErrorCode = ERROR_WRITE_FAULT;
		}
		return false;
	}

	return true;
}

FCrashArtifactResult WriteCrashArtifacts(const FCrashContext& CrashContext)
{
	FCrashArtifactResult Result;
	Result.bContextCaptured = true;
	Result.DumpPath = BuildCrashFilePath(GetDumpFilePrefix(CrashContext), L"dmp", CrashContext.ThreadId);
	Result.LogPath = BuildCrashFilePath(GetLogFilePrefix(CrashContext), L"txt", CrashContext.ThreadId);
	Result.bDumpWritten = WriteCrashDump(CrashContext, Result.DumpPath, &Result.DumpErrorCode);
	if (!Result.bDumpWritten)
	{
		RecordFirstFailure(Result, ECrashArtifactFailureStage::DumpWrite, Result.DumpErrorCode);
	}

	Result.bLogWritten = WriteCrashLog(CrashContext, Result.LogPath, &Result.LogErrorCode);
	if (!Result.bLogWritten)
	{
		RecordFirstFailure(Result, ECrashArtifactFailureStage::LogWrite, Result.LogErrorCode);
	}

	return Result;
}

FCrashArtifactResult WriteCurrentThreadSnapshot(const char* Message)
{
	FCrashContext CrashContext = CaptureCurrentThreadContext(
		CRASH_EXCEPTION_CODE_SNAPSHOT,
		Message ? Message : "Manual snapshot requested.");
	return WriteCrashArtifacts(CrashContext);
}

FCrashArtifactResult WriteCapturedThreadSnapshot(DWORD ThreadId, const char* Message)
{
	FCrashArtifactResult Result;
	if (ThreadId == 0 || ThreadId == GetCurrentThreadId())
	{
		Result.FailureStage = ThreadId == GetCurrentThreadId()
			? ECrashArtifactFailureStage::SelfCapture
			: ECrashArtifactFailureStage::InvalidThreadId;
		Result.FailureErrorCode = ERROR_INVALID_PARAMETER;
		Result.CaptureErrorCode = ERROR_INVALID_PARAMETER;
		return Result;
	}

	const DWORD DesiredAccess = THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION;
	HANDLE ThreadHandle = OpenThread(DesiredAccess, FALSE, ThreadId);
	if (!ThreadHandle)
	{
		Result.FailureStage = ECrashArtifactFailureStage::OpenThread;
		Result.FailureErrorCode = GetLastError();
		Result.CaptureErrorCode = Result.FailureErrorCode;
		return Result;
	}

	FCrashContext CrashContext;
	const FCrashCaptureResult CaptureResult = CaptureOtherThreadContext(ThreadHandle, ThreadId, CrashContext, Message);
	CloseHandle(ThreadHandle);
	if (!CaptureResult.bSuccess)
	{
		Result.FailureStage = CaptureResult.FailureStage;
		Result.FailureErrorCode = CaptureResult.ErrorCode;
		Result.CaptureErrorCode = CaptureResult.ErrorCode;
		return Result;
	}

	return WriteCrashArtifacts(CrashContext);
}
