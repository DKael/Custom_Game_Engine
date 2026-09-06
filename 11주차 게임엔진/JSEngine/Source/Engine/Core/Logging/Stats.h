#pragma once

#include "Core/CoreMinimal.h"
#include "Core/Singleton.h"

#include <Windows.h>
#include <cfloat>

// --- 빌드 설정 ---
#ifndef STATS
#define STATS 0
#endif

// --- Stat Category ---
enum class EStatCategory : uint8
{
	None = 0,
	Anim,
	SkeletalMesh,
	GPU,
	Default,
	MAX
};

// --- Stat Entry ---
struct FStatEntry
{
	const char* Name = nullptr;
	EStatCategory Category = EStatCategory::Default;
	uint32 CallCount = 0;
	double TotalTime = 0.0;		// seconds
	double MaxTime   = 0.0;
	double MinTime   = DBL_MAX;
	double LastTime  = 0.0;

	double GetAvgTime() const { return CallCount > 0 ? TotalTime / CallCount : 0.0; }
};

// --- Stat Counter ---
struct FStatCounterEntry
{
	const char* Name = nullptr;
	EStatCategory Category = EStatCategory::Default;
	int64 Value = 0;
};

// --- Stat Manager (싱글턴) ---
class FStatManager : public TSingleton<FStatManager>
{
	friend class TSingleton<FStatManager>;

public:
	void RecordTime(const char* Name, double ElapsedSeconds, EStatCategory Category = EStatCategory::Default);
	void RecordCounter(const char* Name, int64 Value, EStatCategory Category = EStatCategory::Default);
	
	void TakeSnapshot();
	
	const TArray<FStatEntry>& GetSnapshot() const { return Snapshot; }
	const TArray<FStatCounterEntry>& GetCounterSnapshot() const { return CounterSnapshot; }
	
	LARGE_INTEGER GetFrequency() const { return Frequency; }

private:
	FStatManager();
	~FStatManager() = default;

	TMap<const char*, FStatEntry> Stats;
	TMap<const char*, FStatCounterEntry> Counters;

	TArray<FStatEntry> Snapshot;
	TArray<FStatCounterEntry> CounterSnapshot;

	LARGE_INTEGER Frequency;
};

// --- Scoped Timer (RAII) ---
class FScopedTimer
{
public:
	FScopedTimer(const char* InName, EStatCategory InCategory = EStatCategory::Default) 
		: Name(InName), Category(InCategory)
	{
		QueryPerformanceCounter(&StartTime);
	}

	~FScopedTimer()
	{
		LARGE_INTEGER EndTime;
		QueryPerformanceCounter(&EndTime);
		double Elapsed = static_cast<double>(EndTime.QuadPart - StartTime.QuadPart)
			/ static_cast<double>(FStatManager::Get().GetFrequency().QuadPart);
		FStatManager::Get().RecordTime(Name, Elapsed, Category);
	}

private:
	const char* Name;
	EStatCategory Category;
	LARGE_INTEGER StartTime;
};

// --- SCOPE_STAT 매크로 ---
#if STATS
#define SCOPE_STAT_CONCAT2(a, b) a##b
#define SCOPE_STAT_CONCAT(a, b)  SCOPE_STAT_CONCAT2(a, b)

#define SCOPE_STAT(Name) FScopedTimer SCOPE_STAT_CONCAT(_ScopedTimer_, __COUNTER__)(Name, EStatCategory::Default)
#define SCOPE_STAT_ANIM(Name) FScopedTimer SCOPE_STAT_CONCAT(_ScopedTimer_, __COUNTER__)(Name, EStatCategory::Anim)
#define SCOPE_STAT_SKELMESH(Name) FScopedTimer SCOPE_STAT_CONCAT(_ScopedTimer_, __COUNTER__)(Name, EStatCategory::SkeletalMesh)

#define STAT_COUNTER(Name, Value, Category) FStatManager::Get().RecordCounter(Name, Value, Category)
#define STAT_COUNTER_ANIM(Name, Value) STAT_COUNTER(Name, Value, EStatCategory::Anim)
#define STAT_COUNTER_SKELMESH(Name, Value) STAT_COUNTER(Name, Value, EStatCategory::SkeletalMesh)
#else
#define SCOPE_STAT(Name) ((void)0)
#define SCOPE_STAT_ANIM(Name) ((void)0)
#define SCOPE_STAT_SKELMESH(Name) ((void)0)
#define STAT_COUNTER(Name, Category, Value) ((void)0)
#define STAT_COUNTER_ANIM(Name, Value) ((void)0)
#define STAT_COUNTER_SKELMESH(Name, Value) ((void)0)
#endif
