#pragma once
#include "Core/CoreMinimal.h"

/*
* 현재 등록한 funtion의 핸들
* 여러 개의 함수를 바인딩할 떄 해당 함수가 유효한지 확인하기 위한 용도라 MulticastDelegate에서만 사용됨
*/
struct FDelegateHandle
{
    static constexpr uint64 InvalidID = 0;
public:
    FDelegateHandle() = default;

    // 컴파일러에 따른 묵시적 형변환을 막음
    explicit FDelegateHandle(uint64 ID) : HandleID(ID) {};

public:
    bool IsValid() const { return HandleID != InvalidID; }
    void Reset() { HandleID = InvalidID; }

    bool operator==(const FDelegateHandle& Other) const
    {
        return HandleID == Other.HandleID;
    }

private:
    uint64 HandleID = { };
};
