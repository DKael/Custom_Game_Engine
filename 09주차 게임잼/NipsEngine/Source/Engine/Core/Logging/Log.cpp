#include "Core/Logging/Log.h"

#include <Windows.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <utility>
#include <vector>

namespace
{
// ---------- Shared state ----------

struct FLogSinkEntry
{
    uint32 Id = 0;
    NLogging::FLogSink Sink;
};

std::mutex GLogMutex;
TArray<FLogSinkEntry> GLogSinks;
uint32 GNextSinkId = 1;

struct FThreadLocalLogState
{
    TArray<TArray<char>> ScratchBuffers;
    size_t ScratchDepth = 0;
    bool bIsDispatchingSink = false;
};

thread_local FThreadLocalLogState GTlsLogState;

// ---------- RAII guards ----------

class FThreadLocalScratchScope
{
public:
    FThreadLocalScratchScope()
        : DepthIndex(GTlsLogState.ScratchDepth++)
    {
        if (GTlsLogState.ScratchBuffers.size() <= DepthIndex)
        {
            GTlsLogState.ScratchBuffers.emplace_back();
        }
    }

    ~FThreadLocalScratchScope()
    {
        --GTlsLogState.ScratchDepth;
    }

    TArray<char>& GetBuffer()
    {
        return GTlsLogState.ScratchBuffers[DepthIndex];
    }

private:
    size_t DepthIndex = 0;
};

class FSinkDispatchGuard
{
public:
    FSinkDispatchGuard()
    {
        GTlsLogState.bIsDispatchingSink = true;
    }

    ~FSinkDispatchGuard()
    {
        GTlsLogState.bIsDispatchingSink = false;
    }
};

// ---------- Output helpers ----------

bool EndsWithNewLine(const char* Message)
{
    if (Message == nullptr)
    {
        return true;
    }

    const size_t Length = std::strlen(Message);
    if (Length == 0)
    {
        // Treat empty message as no trailing newline so UE_LOG("") prints a blank line.
        return false;
    }

    const char Tail = Message[Length - 1];
    return Tail == '\n' || Tail == '\r';
}

void WriteToDefaultOutputs(const char* Message)
{
    if (Message == nullptr)
    {
        return;
    }

    const bool bHasTrailingNewLine = EndsWithNewLine(Message);

    std::fputs(Message, stdout);
    if (!bHasTrailingNewLine)
    {
        std::fputc('\n', stdout);
    }
    std::fflush(stdout);

    OutputDebugStringA(Message);
    if (!bHasTrailingNewLine)
    {
        OutputDebugStringA("\n");
    }
}
} // namespace

namespace NLogging
{
uint32 RegisterLogSink(FLogSink Sink)
{
    if (!Sink)
    {
        return 0;
    }

    std::lock_guard<std::mutex> Lock(GLogMutex);

    const uint32 SinkId = GNextSinkId++;
    GLogSinks.push_back({SinkId, std::move(Sink)});
    return SinkId;
}

void UnregisterLogSink(uint32 SinkHandle)
{
    if (SinkHandle == 0)
    {
        return;
    }

    std::lock_guard<std::mutex> Lock(GLogMutex);

    const auto It = std::remove_if(
        GLogSinks.begin(),
        GLogSinks.end(),
        [SinkHandle](const FLogSinkEntry& Entry)
        {
            return Entry.Id == SinkHandle;
        });

    GLogSinks.erase(It, GLogSinks.end());
}

void LogMessage(const char* Message)
{
    if (Message == nullptr)
    {
        return;
    }

    TArray<FLogSinkEntry> SinksSnapshot;
    {
        std::lock_guard<std::mutex> Lock(GLogMutex);
        SinksSnapshot = GLogSinks;
    }

    WriteToDefaultOutputs(Message);

    // Prevent sink recursion loops: sink -> UE_LOG -> sink -> ...
    if (GTlsLogState.bIsDispatchingSink)
    {
        return;
    }

    FSinkDispatchGuard SinkDispatchGuard;
    for (const FLogSinkEntry& Entry : SinksSnapshot)
    {
        if (Entry.Sink)
        {
            Entry.Sink(Message);
        }
    }
}

void LogV(const char* Format, va_list Args)
{
    if (Format == nullptr)
    {
        return;
    }

    va_list ArgsCopy;
    va_copy(ArgsCopy, Args);
    const int32 Required = std::vsnprintf(nullptr, 0, Format, ArgsCopy);
    va_end(ArgsCopy);

    if (Required < 0)
    {
        return;
    }

    // Reuse thread-local scratch memory.
    // Depth-indexed slots avoid corruption under nested logging.
    FThreadLocalScratchScope ScratchScope;
    TArray<char>& Buffer = ScratchScope.GetBuffer();

    const size_t RequiredBufferSize = static_cast<size_t>(Required) + 1u;
    if (Buffer.size() < RequiredBufferSize)
    {
        Buffer.resize(RequiredBufferSize);
    }

    std::vsnprintf(Buffer.data(), Buffer.size(), Format, Args);
    LogMessage(Buffer.data());
}

void Log(const char* Format, ...)
{
    va_list Args;
    va_start(Args, Format);
    LogV(Format, Args);
    va_end(Args);
}
} // namespace NLogging
