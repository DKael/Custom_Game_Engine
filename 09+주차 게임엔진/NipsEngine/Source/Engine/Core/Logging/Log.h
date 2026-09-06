#pragma once

#include "Core/CoreTypes.h"

#include <cstdarg>
#include <functional>

namespace NLogging
{
using FLogSink = std::function<void(const char* Message)>;
// Message is valid only during the callback.
// Sinks must copy Message if they need to keep it after returning.

// Registers a sink and returns its handle. Returns 0 on invalid sink.
uint32 RegisterLogSink(FLogSink Sink);

// Unregisters a sink by handle. Invalid handles are ignored.
void UnregisterLogSink(uint32 SinkHandle);

// Broadcasts a preformatted message to default outputs and sinks.
void LogMessage(const char* Message);

// Variadic helpers used by UE_LOG.
void LogV(const char* Format, va_list Args);
void Log(const char* Format, ...);
} // namespace NLogging

#define UE_LOG(Format, ...) \
    ::NLogging::Log((Format), ##__VA_ARGS__)
