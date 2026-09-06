#pragma once
#include "Object.h"

/*
 * UUID 기반 약한 참조 포인터.
 * 대상 객체가 소멸된 뒤에도 dangling pointer를 만들지 않고 nullptr을 반환한다.
 * shared_ptr 대신 GUObjectArray + UUID로 생존 여부를 판단한다.
 */
template<typename T>
class TWeakObjectPtr
{
public:
    TWeakObjectPtr() = default;

    explicit TWeakObjectPtr(T* InObject)
    {
        if (InObject)
            UUID = InObject->GetUUID();
    }

    T* Get() const
    {
        if (UUID == 0) return nullptr;
        UObject* Found = UObjectManager::Get().FindByUUID(UUID);
        return Cast<T>(Found);
    }

    bool IsValid() const { return Get() != nullptr; }
    explicit operator bool() const { return IsValid(); }

private:
    uint32 UUID = 0;
};
