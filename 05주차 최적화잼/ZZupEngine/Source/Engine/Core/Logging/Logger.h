#pragma once
#include "Core/CoreMinimal.h"
#include <functional>
#include <vector>
#include <string>

class FLogger
{
public:
    // 로그 문자열을 수신할 콜백 함수 타입
    using LogSubscriber = std::function<void(const std::string&)>;

    // 실제 로그를 처리하는 함수 (가변 인자)
    static void Log(const char* Format, ...);

    // 에디터 콘솔 위젯 등에서 로그를 받아볼 수 있도록 구독 등록
    static void AddSubscriber(LogSubscriber Subscriber);

private:
    static std::vector<LogSubscriber> Subscribers;
};

// 기존 에디터 종속 매크로를 대체할 새로운 전역 매크로
#define UE_LOG(Format, ...) \
    FLogger::Log(Format, ##__VA_ARGS__)