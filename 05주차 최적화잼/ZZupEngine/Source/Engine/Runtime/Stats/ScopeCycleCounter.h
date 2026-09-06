#pragma once
#include "Engine/Runtime/WindowsPlatformTime.h"
struct TStatId
{
};

typedef FWindowsPlatformTime FPlatformTime;

class FScopeCycleCounter
{
public:
	FScopeCycleCounter()
		: StartCycles(FPlatformTime::Cycles64())
	{
	}

	~FScopeCycleCounter()
	{
		Finish();
	}
		
	uint64 Finish()
	{
		const uint64 EndCycles = FPlatformTime::Cycles64();
		const uint64 CycleDiff = EndCycles - StartCycles;

		// FThreadStats::AddMessage(UsedStatId, EStatOperation::Add, CycleDiff);

		return CycleDiff;
	}

private:
	uint64 StartCycles;
	TStatId UsedStatId;
};