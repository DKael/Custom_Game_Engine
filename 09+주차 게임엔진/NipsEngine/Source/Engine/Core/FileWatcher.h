#pragma once

#include "Core/Containers/Array.h"
#include "Core/Containers/String.h"
#include "Core/CoreTypes.h"
#include <Windows.h>
#include <atomic>
#include <deque>
#include <mutex>
#include <thread>

class FFileWatcher
{
public:
    FFileWatcher() = default;
    ~FFileWatcher();

    bool Start(const FWString& InDirectory, bool bInRecursive = true);
    void Stop();

    TArray<FWString> DequeueChangedFiles();

private:
    // The watcher thread only collects changed file paths.
    // Compilation/reload happens later on the render thread with the engine's D3D device access pattern.
    void WatchLoop();
    void EnqueueChangedFile(const FWString& InFilePath);

private:
    FWString WatchedDirectory;
    bool bRecursive = true;
    std::thread WatcherThread;
    std::atomic<bool> bStopRequested = false;
    HANDLE DirectoryHandle = INVALID_HANDLE_VALUE;
    std::mutex ChangedFilesMutex;
    std::deque<FWString> ChangedFiles;
};
