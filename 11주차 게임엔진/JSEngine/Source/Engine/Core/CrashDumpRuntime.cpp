#include "Engine/Core/CrashDump.h"

#include <cstdlib>
#include <mutex>
#include <string>
#include <utility>

namespace
{
std::mutex GPendingCrashMessageMutex;
std::string GPendingCrashMessage;

void SetPendingCrashMessage(const char* Message)
{
	std::lock_guard<std::mutex> Lock(GPendingCrashMessageMutex);
	GPendingCrashMessage = Message ? Message : "";
}

std::string ConsumePendingCrashMessage()
{
	std::lock_guard<std::mutex> Lock(GPendingCrashMessageMutex);
	std::string Message = std::move(GPendingCrashMessage);
	GPendingCrashMessage.clear();
	return Message;
}

__declspec(noinline) void CauseCrashFrame3()
{
	RaiseCrashException(CRASH_EXCEPTION_CODE_MANUAL, "CauseCrash console command triggered a manual crash.");
}

__declspec(noinline) void CauseCrashFrame2()
{
	CauseCrashFrame3();
}

__declspec(noinline) void CauseCrashFrame1()
{
	CauseCrashFrame2();
}
}

void InstallCrashHandler()
{
	static bool bInstalled = false;
	if (bInstalled)
	{
		return;
	}

	SetUnhandledExceptionFilter(ReportCrash);
	bInstalled = true;
}

LONG WINAPI ReportCrash(EXCEPTION_POINTERS* ExceptionInfo)
{
	const std::string PendingMessage = ConsumePendingCrashMessage();
	const char* Message = PendingMessage.empty() ? nullptr : PendingMessage.c_str();

	FCrashContext CrashContext = CaptureCrashContext(ExceptionInfo, Message);
	const FCrashArtifactResult Result = WriteCrashArtifacts(CrashContext);
	const std::wstring DialogMessage = BuildCrashArtifactDialogMessage(Result);

	MessageBoxW(nullptr, DialogMessage.c_str(), L"Crash", MB_OK | MB_ICONERROR);
	return EXCEPTION_EXECUTE_HANDLER;
}

[[noreturn]] void RaiseCrashException(DWORD ExceptionCode, const char* Message)
{
	SetPendingCrashMessage(Message);
	RaiseException(ExceptionCode, EXCEPTION_NONCONTINUABLE, 0, nullptr);

	TerminateProcess(GetCurrentProcess(), static_cast<UINT>(ExceptionCode));
	std::abort();
}

[[noreturn]] void CauseCrash()
{
	CauseCrashFrame1();
}
