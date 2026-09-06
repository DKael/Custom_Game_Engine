#pragma once
#include <functional>
#include <vector>
#include "DelegateHandle.h"
#include "Object/WeakObjectPtr.h"

// 템플릿 특수화를 위한 전방선언
template<typename>
class TMulticastDelegate;

// MutlicastDelegate는 반환값이 void
template<typename... Args>
class TMulticastDelegate<void(Args...)>
{
public:
    using HandlerType = std::function<void(Args...)>;

    // 일반 함수나 람다 등록
    FDelegateHandle Add(HandlerType handler)
    {
        uint64 id = NextID++;
        Handlers.push_back({ FDelegateHandle{id}, std::move(handler) });
        return FDelegateHandle{ id };
    }

    // UObject 기반 멤버 함수 바인딩 — 객체가 소멸된 경우 Broadcast 시 해당 항목을 무시한다.
    template<typename T>
    FDelegateHandle AddUObject(T* Instance, void (T::* Func)(Args...))
    {
        TWeakObjectPtr<T> Weak(Instance);
		return Add([Weak, Func](Args... args)
			{
				if (T* Obj = Weak.Get())
					(Obj->*Func)(std::forward<Args>(args)...);
			}
		);
    }

    template<typename T>
    FDelegateHandle AddUObject(const T* Instance, void (T::* Func)(Args...) const)
    {
        TWeakObjectPtr<T> Weak(const_cast<T*>(Instance));
		return Add([Weak, Func](Args... args)
			{
				if (const T* Obj = Weak.Get())
					(Obj->*Func)(std::forward<Args>(args)...);
			}
		);
    }

    // 아무것도 바인딩 되지 않아도 항상 안전, 실행 순서는 보장하지 않는다.
    // 단, 델리게이트를 사용하여 출력 변수 초기화시키지 말것
    void Broadcast(Args... args)
    {
        auto copy = Handlers;
        for (auto& entry : copy)
        {
            entry.Handler(std::forward<Args>(args)...);
        }
    }

    void Remove(FDelegateHandle Handle)
    {
        std::erase_if(Handlers, [&](const FEntry& e)
            {
                return e.HandleID == Handle;
            });
    }

    void RemoveAll()
    {
        Handlers.clear();
    }

private:
    struct FEntry
    {
        FDelegateHandle HandleID;
        HandlerType		Handler;
    };

    TArray<FEntry> Handlers;
    uint64				NextID = { 1 };
};
