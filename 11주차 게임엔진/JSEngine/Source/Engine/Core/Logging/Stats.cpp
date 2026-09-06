#include "Core/Logging/Stats.h"

#include <algorithm>

FStatManager::FStatManager()
{
	QueryPerformanceFrequency(&Frequency);
}

void FStatManager::RecordTime(const char* Name, double ElapsedSeconds, EStatCategory Category)
{
	auto it = Stats.find(Name);
	if (it == Stats.end())
	{
		FStatEntry Entry;
		Entry.Name = Name;
		Entry.Category = Category;
		Entry.CallCount = 1;
		Entry.TotalTime = ElapsedSeconds;
		Entry.MaxTime = ElapsedSeconds;
		Entry.MinTime = ElapsedSeconds;
		Entry.LastTime = ElapsedSeconds;
		Stats[Name] = Entry;
		return;
	}

	FStatEntry& Entry = it->second;
	Entry.Category = Category; // Update category just in case
	Entry.CallCount++;
	Entry.TotalTime += ElapsedSeconds;
	Entry.MaxTime = (std::max)(Entry.MaxTime, ElapsedSeconds);
	Entry.MinTime = (std::min)(Entry.MinTime, ElapsedSeconds);
	Entry.LastTime = ElapsedSeconds;
}

void FStatManager::RecordCounter(const char* Name, int64 Value, EStatCategory Category)
{
	auto it = Counters.find(Name);
	if (it == Counters.end())
	{
		FStatCounterEntry Entry;
		Entry.Name = Name;
		Entry.Category = Category;
		Entry.Value = Value;
		Counters[Name] = Entry;
		return;
	}

	FStatCounterEntry& Entry = it->second;
	Entry.Category = Category;
	Entry.Value += Value;
}

void FStatManager::TakeSnapshot()
{
	Snapshot.clear();
	Snapshot.reserve(Stats.size());

	for (auto& [Key, Entry] : Stats)
	{
		Snapshot.push_back(Entry);

		// Reset for next frame
		Entry.CallCount = 0;
		Entry.TotalTime = 0.0;
		Entry.MaxTime = 0.0;
		Entry.MinTime = DBL_MAX;
		Entry.LastTime = 0.0;
	}

	CounterSnapshot.clear();
	CounterSnapshot.reserve(Counters.size());
	for (auto& [Key, Entry] : Counters)
	{
		CounterSnapshot.push_back(Entry);
		
		// Counter typically resets or stays? 
		// For things like "Active Anim Instances", it should probably be reset if it's an accumulation.
		// If it's a persistent value, it shouldn't. 
		// Unreal's counters often reset each frame if they are incremented during the frame.
		Entry.Value = 0; 
	}
}
