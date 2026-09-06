#pragma once
#include "Core/CoreTypes.h"
#include "Core/Containers/Array.h"
#include "Core/Containers/Map.h"
#include "Core/Containers/String.h"
#include <cstdarg>
#include <cstdlib>
#include <functional>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_dx11.h"
#include "ImGui/imgui_impl_win32.h"

#include "Editor/UI/EditorWidget.h"

class FEditorConsoleWidget : public FEditorWidget
{
public:
    FEditorConsoleWidget();
    ~FEditorConsoleWidget();

    static void AddLog(const char* fmt, ...);
    static void AddLogMessage(const char* Message);

    virtual void Render(float DeltaTime) override;

    void Clear()
    {
        std::lock_guard<std::mutex> MessageLock(MessageMutex);
        for (int32 i = 0; i < Messages.Size; i++) free(Messages[i]);
        Messages.clear();

        std::lock_guard<std::mutex> PendingLock(PendingMessageMutex);
        PendingMessages.clear();
    }
    static void ClearHistory()
    {
        std::lock_guard<std::mutex> Lock(HistoryMutex);
        for (int32 i = 0; i < History.Size; i++) free(History[i]);
        History.clear();
    }

private:
    char InputBuf[256]{};
    static ImVector<char*> Messages;
    static ImVector<char*> History;
    static std::mutex MessageMutex;
    static std::mutex PendingMessageMutex;
    static std::mutex HistoryMutex;
    static TArray<FString> PendingMessages;
    static uint32 LogSinkHandle;
    static int32 ActiveWidgetCount;
    int32 HistoryPos = -1;
    ImGuiTextFilter Filter;
    static bool AutoScroll;
    static bool ScrollToBottom;

    // 백틱(`) 키로 포커스 요청 시 true — 다음 InputText 렌더링 직전에 SetKeyboardFocusHere 호출
    bool bRequestFocusInput = false;

    //Command Dispatch System
    using CommandFn = std::function<void(const TArray<FString>& args)>;
    TMap<FString, CommandFn> Commands;

    void RegisterCommand(const FString& Name, CommandFn Fn);
    void ExecCommand(const char* CommandLine);
    static void DrainPendingLogs();
    static int32 TextEditCallback(ImGuiInputTextCallbackData* Data);

private:
    void CmdStat(const TArray<FString>& Args);
    void CmdShadowFilter(const TArray<FString>& Args);
};
