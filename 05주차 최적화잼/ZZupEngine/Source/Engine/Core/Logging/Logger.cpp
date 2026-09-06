#include "Logger.h"
#include <cstdarg>
#include <cstdio>
#include <iostream>

// 구독자 리스트 초기화
std::vector<FLogger::LogSubscriber> FLogger::Subscribers;

void FLogger::Log(const char* Format, ...)
{
    // 가변 인자를 문자열로 포맷팅
    char Buffer[2048];
    va_list Args;
    va_start(Args, Format);
    vsnprintf(Buffer, sizeof(Buffer), Format, Args);
    va_end(Args);

    std::string LogMessage(Buffer);

    // 1. 기본 표준 출력(콘솔창)에 로그 출력
    std::cout << LogMessage << std::endl;

    // 2. 등록된 구독자(에디터 콘솔 위젯 등)들에게 메세지 전달
    for (const auto& Subscriber : Subscribers)
    {
        if (Subscriber)
        {
            Subscriber(LogMessage);
        }
    }
}

void FLogger::AddSubscriber(LogSubscriber Subscriber)
{
    Subscribers.push_back(Subscriber);
}