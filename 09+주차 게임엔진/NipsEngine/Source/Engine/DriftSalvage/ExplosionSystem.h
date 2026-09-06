#pragma once

#include "Core/Containers/Array.h"
#include "Core/Delegate/MulticastDelegate.h"
#include "Math/Vector.h"
#include "Object/WeakObjectPtr.h"

class UWorld;
class AActor;
class USubUVComponent;

// 폭발/충돌로 떠밀린 actor의 push 속도를 추적하면서
// 매 tick: 위치 갱신 + Rock 슬라이드 + collectible 끼리 밀어내기 + 마찰 감속을 처리한다.
//
// 이름은 "Explosion"이지만 Boat-Rock 같은 일반 충돌 푸시도 ApplyKnockback으로 받아서
// 같은 메커니즘으로 자연스럽게 감속한다.
class FExplosionSystem
{
public:
    void Tick(UWorld* World, float DeltaTime);
    void Reset();

    // Hazard 폭발: Center 반경 안의 회수 가능 actor에게 바깥 방향 push.
    // 같은 반경의 다른 Hazard는 거리에 비례한 딜레이로 연쇄 폭발 예약된다.
    void TriggerExplosion(UWorld* World, const FVector& Center);
    void TriggerExplosion(UWorld* World, AActor* SourceHazard, const FVector& Center);
    void CancelHazardCollectionInterference(AActor* Hazard);

    // 임의의 actor에 push 속도를 부여한다 (Boat 충돌 등에서 사용).
    // 이미 떠밀리는 중이면 속도가 누적된다.
    void ApplyKnockback(AActor* Actor, const FVector& Velocity);

    // 떠밀리는 중인지 검사. 떠밀리는 동안 collect/연쇄 트리거를 막는 데 쓰임.
    bool IsActorPushed(const AActor* Actor) const;

    // --- 디버그 시각화용 ---
    struct FDebugRing
    {
        FVector Center = FVector::ZeroVector;
        float   Radius = 0.0f;
        float   Age = 0.0f;
        float   Lifetime = 0.4f;
    };
    const TArray<FDebugRing>& GetDebugRings() const { return DebugRings; }

private:
    struct FPushedActor
    {
        AActor* Actor = nullptr;
        FVector Velocity = FVector::ZeroVector;
    };

    struct FPendingExplosion
    {
        TWeakObjectPtr<AActor> SourceHazard; // 시간이 만료되면 이 actor가 폭발/destroy 된다.
        FVector Center = FVector::ZeroVector;
        float   TimeRemaining = 0.0f;
    };

    struct FPendingPush
    {
        AActor* Actor = nullptr;
        FVector Velocity = FVector::ZeroVector;
        float   TimeRemaining = 0.0f;
    };

    struct FHazardSubUVEffect
    {
        TWeakObjectPtr<AActor> Actor;
        TArray<USubUVComponent*> SubUVs;
        float Age = 0.0f;
        float MaxLifetime = 2.0f;
    };

    using FOnExplosionDelegate = TMulticastDelegate<void(UWorld*, AActor*, const FVector&)>;

    void BindDefaultExplosionEvents();
    void PlayExplosionSound(UWorld* World, AActor* SourceHazard, const FVector& Center);
    void StartHazardSubUVEffect(UWorld* World, AActor* SourceHazard, const FVector& Center);
    void TickHazardSubUVEffects(float DeltaTime);
    bool IsHazardSubUVEffectActive(const AActor* Actor) const;

    void TriggerImmediate(UWorld* World, AActor* SourceHazard, const FVector& Center);
    void EnqueuePushToCollectibles(UWorld* World, const FVector& Center);
    void EnqueueChainExplosions(UWorld* World, AActor* SourceHazard, const FVector& Center);

    void HandleRockCollision(AActor* Actor, FVector& Velocity);
    void HandleCollectibleCollision(AActor* Actor, FVector& Velocity);

    bool IsActorInsideRadius(const AActor* Actor, const FVector& Center, float InRadius) const;
    bool IsCollectibleTag(const AActor* Actor) const;
    bool IsHazardTag(const AActor* Actor) const;
    bool IsRockTag(const AActor* Actor) const;
    FPushedActor* FindEntry(AActor* Actor);
    FPendingExplosion* FindPendingExplosion(AActor* Hazard);
    const FPendingExplosion* FindPendingExplosion(const AActor* Hazard) const;
    FPendingPush* FindPendingPush(AActor* Actor);
    const FPendingPush* FindPendingPush(const AActor* Actor) const;

    TArray<FPushedActor> Pushed;
    TArray<FPendingExplosion> Pending;
    TArray<FPendingPush> PendingPushes;
    TArray<FHazardSubUVEffect> HazardSubUVEffects;
    TArray<FDebugRing> DebugRings;
    FOnExplosionDelegate OnExplosion;

    // 폭발 파라미터
    float Radius              = 20.0f; // 폭발 영햠 범위
    float InitialSpeed        = 5.5f; // 주변 Collectible이 처음 받는 밀림 속도
    float Damping             = 4.0f; // 밀리는 물체의 감속
    float MinSpeed            = 0.15f; // 밀림 종료
    float ChainShockwaveSpeed = 40.0f; // 거리 기반 딜레이
    float DebugRingLifetime   = 2.0f;
};
