#include "Engine/Runtime/Launch.h"

#include "Engine/Core/CrashDump.h"
#include "Engine/Runtime/EngineLoop.h"

#include <exception>

namespace
{
	int GuardedMain(HINSTANCE hInstance, int nShowCmd)
	{
		try
		{
			FEngineLoop EngineLoop;
			if (!EngineLoop.Init(hInstance, nShowCmd))
			{
				return -1;
			}

			const int ExitCode = EngineLoop.Run();
			EngineLoop.Shutdown();
			return ExitCode;
		}
		catch (const std::exception& e)
		{
			RaiseCrashException(CRASH_EXCEPTION_CODE_STD_EXCEPTION, e.what());
		}
		catch (...)
		{
			RaiseCrashException(CRASH_EXCEPTION_CODE_UNKNOWN_CPP_EXCEPTION, "Unknown C++ exception was rethrown as an SEH crash.");
		}

		return -1;
	}
}

int Launch(HINSTANCE hInstance, int nShowCmd)
{
	InstallCrashHandler();

	__try
	{
		return GuardedMain(hInstance, nShowCmd);
	}
	__except (ReportCrash(GetExceptionInformation()))
	{
		return static_cast<int>(GetExceptionCode());
	}
}
