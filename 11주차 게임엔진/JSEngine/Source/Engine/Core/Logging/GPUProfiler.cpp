#include "Core/Logging/GPUProfiler.h"

#include <algorithm>
#include <cfloat>
#include <d3d11.h>
#include "Render/Common/D3D11DebugUtils.h"

void FGPUProfiler::Initialize(ID3D11Device* InDevice, ID3D11DeviceContext* InContext)
{
	Device = InDevice;
	Context = InContext;
	if (!Device || !Context)
	{
		bInitialized = false;
		return;
	}

	D3D11_QUERY_DESC disjointDesc = {};
	disjointDesc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;

	D3D11_QUERY_DESC timestampDesc = {};
	timestampDesc.Query = D3D11_QUERY_TIMESTAMP;

	for (uint32 f = 0; f < FRAME_COUNT; ++f)
	{
		Device->CreateQuery(&disjointDesc, Frames[f].DisjointQuery.ReleaseAndGetAddressOf());
		D3D11Debug::SetDebugName(Frames[f].DisjointQuery.Get(), "FGPUProfiler.DisjointQuery");
		Frames[f].UsedCount = 0;
		Frames[f].bSubmitted = false;

		for (uint32 i = 0; i < MAX_TIMESTAMPS; ++i)
		{
			Device->CreateQuery(&timestampDesc, Frames[f].Timestamps[i].BeginQuery.ReleaseAndGetAddressOf());
			Device->CreateQuery(&timestampDesc, Frames[f].Timestamps[i].EndQuery.ReleaseAndGetAddressOf());
			D3D11Debug::SetDebugName(Frames[f].Timestamps[i].BeginQuery.Get(), "FGPUProfiler.TimestampBeginQuery");
			D3D11Debug::SetDebugName(Frames[f].Timestamps[i].EndQuery.Get(), "FGPUProfiler.TimestampEndQuery");
			Frames[f].Timestamps[i].Name = nullptr;
		}
	}

	WriteIndex = 0;
	bSkipFrame = false;
	bInitialized = true;
}

void FGPUProfiler::Shutdown()
{
	if (!bInitialized) return;

	for (uint32 f = 0; f < FRAME_COUNT; ++f)
	{
		Frames[f].DisjointQuery.Reset();
		Frames[f].UsedCount = 0;
		Frames[f].bSubmitted = false;

		for (uint32 i = 0; i < MAX_TIMESTAMPS; ++i)
		{
			Frames[f].Timestamps[i].BeginQuery.Reset();
			Frames[f].Timestamps[i].EndQuery.Reset();
		}
	}

	Device.Reset();
	Context.Reset();
	WriteIndex = 0;
	bSkipFrame = false;
	bInitialized = false;
}

void FGPUProfiler::BeginFrame()
{
	if (!bInitialized) return;

	// 이전 프레임 결과 수집
	CollectPreviousFrame();

	if (Frames[WriteIndex].bSubmitted)
	{
		bSkipFrame = true;
		return;
	}

	bSkipFrame = false;

	// 현재 프레임 시작
	FFrameData& Write = Frames[WriteIndex];
	Write.UsedCount = 0;
	Write.bSubmitted = false;
	Context->Begin(Write.DisjointQuery.Get());
}

void FGPUProfiler::EndFrame()
{
	if (!bInitialized) return;
	if (bSkipFrame) return;

	Context->End(Frames[WriteIndex].DisjointQuery.Get());
	Frames[WriteIndex].bSubmitted = true;

	// 프레임 스왑
	WriteIndex = (WriteIndex + 1) % FRAME_COUNT;
}

uint32 FGPUProfiler::BeginTimestamp(const char* Name)
{
	if (!bInitialized) return UINT32_MAX;
	if (bSkipFrame) return UINT32_MAX;

	FFrameData& Write = Frames[WriteIndex];
	if (Write.UsedCount >= MAX_TIMESTAMPS)
	{
		static bool bWarned = false;
		if (!bWarned)
		{
			UE_LOG("[GPU] Max timestamps exceeded (%u). Some stats will be missing.", MAX_TIMESTAMPS);
			bWarned = true;
		}
		return UINT32_MAX;
	}

	uint32 Idx = Write.UsedCount++;
	Write.Timestamps[Idx].Name = Name;
	Context->End(Write.Timestamps[Idx].BeginQuery.Get());  // Timestamp은 End()로 기록
	return Idx;
}

void FGPUProfiler::EndTimestamp(uint32 Index)
{
	if (!bInitialized || Index == UINT32_MAX) return;
	if (bSkipFrame) return;

	FFrameData& Write = Frames[WriteIndex];
	if (Index >= Write.UsedCount) return;

	Context->End(Write.Timestamps[Index].EndQuery.Get());
}

void FGPUProfiler::CollectPreviousFrame()
{
	// 현재 WriteIndex를 제외한 다른 모든 제출된 프레임 수집 시도
	for (uint32 i = 0; i < FRAME_COUNT; ++i)
	{
		if (i == WriteIndex) continue;

		FFrameData& Read = Frames[i];
		if (!Read.bSubmitted) continue;

		// Disjoint 결과 확인
		D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjointData;
		HRESULT hr = Context->GetData(Read.DisjointQuery.Get(), &disjointData, sizeof(disjointData), D3D11_ASYNC_GETDATA_DONOTFLUSH);
		
		if (hr == S_OK)
		{
			if (!disjointData.Disjoint && Read.UsedCount > 0)
			{
				double InvFrequency = 1000.0 / static_cast<double>(disjointData.Frequency); // ms 단위
				UINT64 tsBegin, tsEnd;

				for (uint32 j = 0; j < Read.UsedCount; ++j)
				{
					if (Context->GetData(Read.Timestamps[j].BeginQuery.Get(), &tsBegin, sizeof(UINT64), D3D11_ASYNC_GETDATA_DONOTFLUSH) == S_OK &&
						Context->GetData(Read.Timestamps[j].EndQuery.Get(), &tsEnd, sizeof(UINT64), D3D11_ASYNC_GETDATA_DONOTFLUSH) == S_OK)
					{
						double ElapsedMs = static_cast<double>(tsEnd - tsBegin) * InvFrequency;
						double ElapsedSec = ElapsedMs * 0.001;

						const char* Name = Read.Timestamps[j].Name;
						auto it = GPUStats.find(Name);
						if (it == GPUStats.end())
						{
							FStatEntry Entry;
							Entry.Name = Name;
							Entry.CallCount = 1;
							Entry.TotalTime = ElapsedSec;
							Entry.MaxTime = ElapsedSec;
							Entry.MinTime = ElapsedSec;
							Entry.LastTime = ElapsedSec;
							GPUStats[Name] = Entry;
						}
						else
						{
							FStatEntry& Entry = it->second;
							Entry.CallCount++;
							Entry.TotalTime += ElapsedSec;
							Entry.MaxTime = (std::max)(Entry.MaxTime, ElapsedSec);
							Entry.MinTime = (std::min)(Entry.MinTime, ElapsedSec);
							Entry.LastTime = ElapsedSec;
						}
					}
				}
			}

			Read.bSubmitted = false;
			Read.UsedCount = 0;
		}
	}
}

void FGPUProfiler::TakeSnapshot()
{
	Snapshot.clear();
	Snapshot.reserve(GPUStats.size());

	for (auto& [Key, Entry] : GPUStats)
	{
		Snapshot.push_back(Entry);

		// Reset for next frame
		Entry.CallCount = 0;
		Entry.TotalTime = 0.0;
		Entry.MaxTime = 0.0;
		Entry.MinTime = DBL_MAX;
		Entry.LastTime = 0.0;
	}
}
