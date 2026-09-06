#include "DriftSalvage/ExplosionSystem.h"

#include <algorithm>
#include <cmath>

#include "Component/PrimitiveComponent.h"
#include "Component/ShapeComponent.h"
#include "Component/SubUVComponent.h"
#include "Core/ActorTags.h"
#include "Core/Logging/Log.h"
#include "Core/SoundManager.h"
#include "GameFramework/Actor.h"
#include "GameFramework/World.h"

namespace
{
    bool AABBIntersects(const FAABB& A, const FAABB& B)
    {
        return A.Min.X <= B.Max.X && A.Max.X >= B.Min.X &&
               A.Min.Y <= B.Max.Y && A.Max.Y >= B.Min.Y &&
               A.Min.Z <= B.Max.Z && A.Max.Z >= B.Min.Z;
    }

    // 작은 helper — CollisionSystem의 ComputeAABBDepenetration과 동일 로직.
    FVector ComputeDepenetration(const FAABB& Mover, const FAABB& Blocker)
    {
        if (!AABBIntersects(Mover, Blocker))
        {
            return FVector::ZeroVector;
        }

        const FVector MoverCenter = Mover.GetCenter();
        const FVector BlockerCenter = Blocker.GetCenter();

        const float PushX = std::min(Mover.Max.X - Blocker.Min.X, Blocker.Max.X - Mover.Min.X);
        const float PushY = std::min(Mover.Max.Y - Blocker.Min.Y, Blocker.Max.Y - Mover.Min.Y);
        const float PushZ = std::min(Mover.Max.Z - Blocker.Min.Z, Blocker.Max.Z - Mover.Min.Z);

        constexpr float Skin = 0.05f;

        if (PushX <= PushY && PushX <= PushZ)
        {
            return FVector((MoverCenter.X >= BlockerCenter.X ? 1.0f : -1.0f) * (PushX + Skin), 0.0f, 0.0f);
        }
        if (PushY <= PushX && PushY <= PushZ)
        {
            return FVector(0.0f, (MoverCenter.Y >= BlockerCenter.Y ? 1.0f : -1.0f) * (PushY + Skin), 0.0f);
        }
        return FVector(0.0f, 0.0f, (MoverCenter.Z >= BlockerCenter.Z ? 1.0f : -1.0f) * (PushZ + Skin));
    }

    bool ActorAlive(const AActor* Actor)
    {
        return Actor && Actor->IsActive() && !Actor->IsPendingDestroy() && !Actor->IsBeingDestroyed();
    }

    FAABB GetActorCollisionAABB(const AActor* Actor)
    {
        FAABB Bounds;
        Bounds.Reset();
        if (!Actor)
        {
            return Bounds;
        }

        for (UPrimitiveComponent* Primitive : Actor->GetPrimitiveComponents())
        {
            if (!Primitive || !Primitive->IsActive())
            {
                continue;
            }

            if (!Cast<UShapeComponent>(Primitive))
            {
                continue;
            }

            const FAABB& B = Primitive->GetWorldAABB();
            if (B.IsValid())
            {
                Bounds.ExpandToInclude(B);
            }
        }
        return Bounds;
    }

    float ClampF(float Value, float Min, float Max)
    {
        return std::max(Min, std::min(Value, Max));
    }

    FVector ClosestPointOnAABB(const FVector& Point, const FAABB& Box)
    {
        return FVector(
            ClampF(Point.X, Box.Min.X, Box.Max.X),
            ClampF(Point.Y, Box.Min.Y, Box.Max.Y),
            ClampF(Point.Z, Box.Min.Z, Box.Max.Z));
    }

    float PointAABBDistanceSq(const FVector& Point, const FAABB& Box)
    {
        return FVector::DistSquared(Point, ClosestPointOnAABB(Point, Box));
    }

    float DistanceToActorCollision(const AActor* Actor, const FVector& Point)
    {
        const FAABB Bounds = GetActorCollisionAABB(Actor);
        if (Bounds.IsValid())
        {
            return std::sqrt(PointAABBDistanceSq(Point, Bounds));
        }

        return Actor ? FVector::Distance(Actor->GetActorLocation(), Point) : 0.0f;
    }

    FVector GetActorPushOrigin(const AActor* Actor)
    {
        const FAABB Bounds = GetActorCollisionAABB(Actor);
        if (Bounds.IsValid())
        {
            return Bounds.GetCenter();
        }

        return Actor ? Actor->GetActorLocation() : FVector::ZeroVector;
    }
}

void FExplosionSystem::Reset()
{
    Pushed.clear();
    Pending.clear();
    PendingPushes.clear();
    HazardSubUVEffects.clear();
    DebugRings.clear();
    BindDefaultExplosionEvents();
}

bool FExplosionSystem::IsCollectibleTag(const AActor* Actor) const
{
    return Actor &&
           (Actor->CompareTag(ActorTags::Trash) ||
            Actor->CompareTag(ActorTags::Resource) ||
            Actor->CompareTag(ActorTags::Recyclable) ||
            Actor->CompareTag(ActorTags::Premium));
}

bool FExplosionSystem::IsHazardTag(const AActor* Actor) const
{
    return Actor && Actor->CompareTag(ActorTags::Hazard);
}

bool FExplosionSystem::IsRockTag(const AActor* Actor) const
{
    return Actor && Actor->CompareTag(ActorTags::Rock);
}

FExplosionSystem::FPushedActor* FExplosionSystem::FindEntry(AActor* Actor)
{
    for (FPushedActor& Entry : Pushed)
    {
        if (Entry.Actor == Actor)
        {
            return &Entry;
        }
    }
    return nullptr;
}

FExplosionSystem::FPendingExplosion* FExplosionSystem::FindPendingExplosion(AActor* Hazard)
{
    for (FPendingExplosion& E : Pending)
    {
        if (E.SourceHazard.Get() == Hazard)
        {
            return &E;
        }
    }
    return nullptr;
}

const FExplosionSystem::FPendingExplosion* FExplosionSystem::FindPendingExplosion(const AActor* Hazard) const
{
    for (const FPendingExplosion& E : Pending)
    {
        if (E.SourceHazard.Get() == Hazard)
        {
            return &E;
        }
    }
    return nullptr;
}

FExplosionSystem::FPendingPush* FExplosionSystem::FindPendingPush(AActor* Actor)
{
    for (FPendingPush& Entry : PendingPushes)
    {
        if (Entry.Actor == Actor)
        {
            return &Entry;
        }
    }
    return nullptr;
}

const FExplosionSystem::FPendingPush* FExplosionSystem::FindPendingPush(const AActor* Actor) const
{
    for (const FPendingPush& Entry : PendingPushes)
    {
        if (Entry.Actor == Actor)
        {
            return &Entry;
        }
    }
    return nullptr;
}

bool FExplosionSystem::IsActorPushed(const AActor* Actor) const
{
    if (!Actor)
    {
        return false;
    }
    for (const FPushedActor& Entry : Pushed)
    {
        if (Entry.Actor == Actor)
        {
            return true;
        }
    }
    if (FindPendingPush(Actor))
    {
        return true;
    }
    return false;
}

void FExplosionSystem::ApplyKnockback(AActor* Actor, const FVector& Velocity)
{
    if (!ActorAlive(Actor) || IsRockTag(Actor) || Velocity.IsNearlyZero())
    {
        return;
    }

    if (FPushedActor* Existing = FindEntry(Actor))
    {
        Existing->Velocity += Velocity;
    }
    else
    {
        FPushedActor Entry;
        Entry.Actor = Actor;
        Entry.Velocity = Velocity;
        Pushed.push_back(Entry);
    }
}

bool FExplosionSystem::IsActorInsideRadius(const AActor* Actor, const FVector& Center, float InRadius) const
{
    if (!Actor || InRadius <= 0.0f)
    {
        return false;
    }

    const float RadiusSq = InRadius * InRadius;
    const FAABB Bounds = GetActorCollisionAABB(Actor);
    if (Bounds.IsValid())
    {
        return PointAABBDistanceSq(Center, Bounds) <= RadiusSq;
    }

    return FVector::DistSquared(Actor->GetActorLocation(), Center) <= RadiusSq;
}

void FExplosionSystem::TriggerExplosion(UWorld* World, const FVector& Center)
{
    TriggerExplosion(World, nullptr, Center);
}

void FExplosionSystem::TriggerExplosion(UWorld* World, AActor* SourceHazard, const FVector& Center)
{
    if (!World)
    {
        return;
    }
    TriggerImmediate(World, SourceHazard, Center);
}

void FExplosionSystem::CancelHazardCollectionInterference(AActor* Hazard)
{
    if (!Hazard)
    {
        return;
    }

    for (size_t i = 0; i < Pending.size();)
    {
        AActor* Source = Pending[i].SourceHazard.Get();
        if (!Source || Source == Hazard)
        {
            Pending.erase(Pending.begin() + i);
            continue;
        }

        ++i;
    }

    for (size_t i = 0; i < HazardSubUVEffects.size();)
    {
        FHazardSubUVEffect& Effect = HazardSubUVEffects[i];
        AActor* EffectActor = Effect.Actor.Get();
        if (!EffectActor || EffectActor == Hazard)
        {
            for (USubUVComponent* SubUV : Effect.SubUVs)
            {
                if (!SubUV)
                {
                    continue;
                }

                SubUV->SetFrameIndex(0);
                SubUV->SetVisibility(false);
                SubUV->SetActive(false);
            }

            HazardSubUVEffects.erase(HazardSubUVEffects.begin() + i);
            continue;
        }

        ++i;
    }

    for (UActorComponent* Component : Hazard->GetComponents())
    {
        if (USubUVComponent* SubUV = Cast<USubUVComponent>(Component))
        {
            SubUV->SetFrameIndex(0);
            SubUV->SetVisibility(false);
            SubUV->SetActive(false);
            continue;
        }

        UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Component);
        if (Primitive)
        {
            Primitive->SetVisibility(true);
            Primitive->SetActive(true);
        }
    }
}

void FExplosionSystem::BindDefaultExplosionEvents()
{
    OnExplosion.RemoveAll();
    OnExplosion.Add([this](UWorld* World, AActor* SourceHazard, const FVector& Center)
    {
        PlayExplosionSound(World, SourceHazard, Center);
    });
    OnExplosion.Add([this](UWorld* World, AActor* SourceHazard, const FVector& Center)
    {
        StartHazardSubUVEffect(World, SourceHazard, Center);
    });
}

void FExplosionSystem::PlayExplosionSound(UWorld* World, AActor* SourceHazard, const FVector& Center)
{
    (void)World;
    (void)SourceHazard;
    (void)Center;

    FSoundManager::Get().PlaySFX("Boom.mp3", 2.f);
}

bool FExplosionSystem::IsHazardSubUVEffectActive(const AActor* Actor) const
{
    if (!Actor)
    {
        return false;
    }

    for (const FHazardSubUVEffect& Effect : HazardSubUVEffects)
    {
        if (Effect.Actor.Get() == Actor)
        {
            return true;
        }
    }
    return false;
}

void FExplosionSystem::StartHazardSubUVEffect(UWorld* World, AActor* SourceHazard, const FVector& Center)
{
    if (!World || !SourceHazard)
    {
        return;
    }

    if (IsHazardSubUVEffectActive(SourceHazard))
    {
        return;
    }

    FHazardSubUVEffect Effect;
    Effect.Actor = TWeakObjectPtr<AActor>(SourceHazard);
    Effect.Age = 0.0f;
    Effect.MaxLifetime = 0.2f;

    for (UActorComponent* Component : SourceHazard->GetComponents())
    {
        UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Component);
        if (!Primitive)
        {
            continue;
        }

        USubUVComponent* SubUV = Cast<USubUVComponent>(Primitive);
        if (!SubUV)
        {
            Primitive->SetVisibility(false);
            if (UShapeComponent* Shape = Cast<UShapeComponent>(Primitive))
            {
                Shape->ClearOverlapInfos();
                Shape->SetActive(false);
            }
            continue;
        }

        if (!SubUV->GetParticle() && SubUV->GetParticleName().IsValid())
        {
            SubUV->SetParticle(SubUV->GetParticleName());
        }

        if (!SubUV->GetParticle())
        {
            SubUV->SetVisibility(false);
            continue;
        }

        const float FrameRate = std::max(1.0f, SubUV->GetFrameRate());
        const FParticleResource* Particle = SubUV->GetParticle();
        const float TotalFrames = static_cast<float>(std::max(1u, Particle->Columns * Particle->Rows));

        SubUV->SetActive(true);
        SubUV->SetVisibility(true);
        SubUV->SetEnableCull(false);
        SubUV->SetLoop(false);
        SubUV->Play();

        Effect.SubUVs.push_back(SubUV);
        Effect.MaxLifetime = std::max(Effect.MaxLifetime, (TotalFrames / FrameRate) + 0.2f);
    }

    if (Effect.SubUVs.empty())
    {
        SourceHazard->Destroy();
        return;
    }

    HazardSubUVEffects.push_back(Effect);

    UE_LOG("[ExplosionSubUV] source=%s center=(%.2f, %.2f, %.2f)",
           *SourceHazard->GetName(),
           Center.X,
           Center.Y,
           Center.Z);
}

void FExplosionSystem::TickHazardSubUVEffects(float DeltaTime)
{
    for (size_t i = 0; i < HazardSubUVEffects.size();)
    {
        FHazardSubUVEffect& Effect = HazardSubUVEffects[i];
        Effect.Age += DeltaTime;

        AActor* Actor = Effect.Actor.Get();
        if (!ActorAlive(Actor))
        {
            HazardSubUVEffects.erase(HazardSubUVEffects.begin() + i);
            continue;
        }

        bool bAllAnimationsFinished = !Effect.SubUVs.empty();
        for (USubUVComponent* SubUV : Effect.SubUVs)
        {
            if (SubUV && !SubUV->IsFinished())
            {
                bAllAnimationsFinished = false;
                break;
            }
        }

        const bool bExpiredByTime = Effect.Age >= Effect.MaxLifetime;
        if (bAllAnimationsFinished || bExpiredByTime)
        {
            Actor->Destroy();
            HazardSubUVEffects.erase(HazardSubUVEffects.begin() + i);
            continue;
        }

        ++i;
    }
}

void FExplosionSystem::TriggerImmediate(UWorld* World, AActor* SourceHazard, const FVector& Center)
{
    UE_LOG("[Explosion] center=(%.2f, %.2f, %.2f) radius=%.2f",
           Center.X, Center.Y, Center.Z, Radius);

    OnExplosion.Broadcast(World, SourceHazard, Center);

    // 디버그 ring 추가 — 한 번 폭발할 때마다 시각화.
    FDebugRing Ring;
    Ring.Center = Center;
    Ring.Radius = Radius;
    Ring.Age = 0.0f;
    Ring.Lifetime = DebugRingLifetime;
    DebugRings.push_back(Ring);

    EnqueuePushToCollectibles(World, Center);
    EnqueueChainExplosions(World, SourceHazard, Center);
}

void FExplosionSystem::EnqueuePushToCollectibles(UWorld* World, const FVector& Center)
{
    const float SafeSpeed = std::max(0.01f, ChainShockwaveSpeed);

    for (AActor* Actor : World->GetActors())
    {
        if (!ActorAlive(Actor) || !IsCollectibleTag(Actor))
        {
            continue;
        }

        if (!IsActorInsideRadius(Actor, Center, Radius))
        {
            continue;
        }

        const FVector PushOrigin = GetActorPushOrigin(Actor);
        FVector Dir = (PushOrigin - Center).GetSafeNormal2D();
        if (Dir.IsNearlyZero())
        {
            Dir = FVector(1.0f, 0.0f, 0.0f);
        }

        const float Distance = DistanceToActorCollision(Actor, Center);
        const float Delay = Distance / SafeSpeed;
        const FVector Velocity = Dir * InitialSpeed;

        if (FPendingPush* Existing = FindPendingPush(Actor))
        {
            if (Delay < Existing->TimeRemaining)
            {
                Existing->Velocity = Velocity;
                Existing->TimeRemaining = Delay;
            }
        }
        else
        {
            FPendingPush PendingPush;
            PendingPush.Actor = Actor;
            PendingPush.Velocity = Velocity;
            PendingPush.TimeRemaining = Delay;
            PendingPushes.push_back(PendingPush);
        }

        UE_LOG("[ExplosionPushQueued] name=%s tag=%s dist=%.2f delay=%.2f",
               *Actor->GetName(),
               Actor->GetTag().c_str(),
               Distance,
               Delay);
    }
}

void FExplosionSystem::EnqueueChainExplosions(UWorld* World, AActor* SourceHazard, const FVector& Center)
{
    const float SafeSpeed = std::max(0.01f, ChainShockwaveSpeed);

    for (AActor* Actor : World->GetActors())
    {
        if (!ActorAlive(Actor) || !IsHazardTag(Actor))
        {
            continue;
        }

        if (Actor == SourceHazard)
        {
            continue;
        }

        if (!IsActorInsideRadius(Actor, Center, Radius))
        {
            continue;
        }

        const FVector ActorPos = Actor->GetActorLocation();
        const float Distance = DistanceToActorCollision(Actor, Center);
        const float Delay = Distance / SafeSpeed;

        if (FPendingExplosion* Existing = FindPendingExplosion(Actor))
        {
            if (Delay < Existing->TimeRemaining)
            {
                Existing->Center = ActorPos;
                Existing->TimeRemaining = Delay;
            }
            continue;
        }

        FPendingExplosion Pend;
        Pend.SourceHazard = TWeakObjectPtr<AActor>(Actor);
        Pend.Center = ActorPos;
        Pend.TimeRemaining = Delay;
        Pending.push_back(Pend);
        UE_LOG("[ExplosionChain] name=%s dist=%.2f delay=%.2f",
               *Actor->GetName(),
               Distance,
               Pend.TimeRemaining);
    }
}

void FExplosionSystem::HandleRockCollision(AActor* Actor, FVector& Velocity)
{
    if (!ActorAlive(Actor))
    {
        return;
    }

    UWorld* World = Actor->GetFocusedWorld();
    if (!World)
    {
        return;
    }

    const FAABB MoverBox = GetActorCollisionAABB(Actor);
    if (!MoverBox.IsValid())
    {
        return;
    }

    for (AActor* Other : World->GetActors())
    {
        if (!ActorAlive(Other) || Other == Actor || !IsRockTag(Other))
        {
            continue;
        }

        const FAABB RockBox = GetActorCollisionAABB(Other);
        if (!RockBox.IsValid())
        {
            continue;
        }

        const FVector Push = ComputeDepenetration(MoverBox, RockBox);
        if (Push.IsNearlyZero())
        {
            continue;
        }

        Actor->AddActorWorldOffset(Push);

        const FVector Normal = Push.GetSafeNormal();
        if (!Normal.IsNearlyZero())
        {
            const float Into = FVector::DotProduct(Velocity, Normal);
            if (Into < 0.0f)
            {
                Velocity = Velocity - Normal * Into; // 법선 성분 제거 → 슬라이드
            }
        }
    }
}

void FExplosionSystem::HandleCollectibleCollision(AActor* Actor, FVector& Velocity)
{
    if (!ActorAlive(Actor))
    {
        return;
    }

    UWorld* World = Actor->GetFocusedWorld();
    if (!World)
    {
        return;
    }

    const FAABB MoverBox = GetActorCollisionAABB(Actor);
    if (!MoverBox.IsValid())
    {
        return;
    }

    for (AActor* Other : World->GetActors())
    {
        if (!ActorAlive(Other) || Other == Actor || !IsCollectibleTag(Other))
        {
            continue;
        }

        const FAABB OtherBox = GetActorCollisionAABB(Other);
        if (!OtherBox.IsValid())
        {
            continue;
        }

        const FVector Push = ComputeDepenetration(MoverBox, OtherBox);
        if (Push.IsNearlyZero())
        {
            continue;
        }

        // (1) 자기 자신을 빼고
        Actor->AddActorWorldOffset(Push);

        const FVector Normal = Push.GetSafeNormal();
        if (Normal.IsNearlyZero())
        {
            continue;
        }

        // (2) 부딪힌 쪽으로 normal 방향 운동량 일부 전달.
        const float Into = FVector::DotProduct(Velocity, Normal);
        if (Into < 0.0f)
        {
            constexpr float Transfer = 0.6f;
            const FVector TransferredV = Normal * Into * Transfer;
            ApplyKnockback(Other, TransferredV);

            // 자기 속도는 normal 성분 일부 잃음.
            Velocity = Velocity - Normal * (Into * Transfer);
        }
    }
}

void FExplosionSystem::Tick(UWorld* World, float DeltaTime)
{
    if (!World || DeltaTime <= 0.0f)
    {
        return;
    }

    // 1) 디버그 ring 노화.
    for (size_t i = 0; i < DebugRings.size();)
    {
        DebugRings[i].Age += DeltaTime;
        if (DebugRings[i].Age >= DebugRings[i].Lifetime)
        {
            DebugRings.erase(DebugRings.begin() + i);
            continue;
        }
        ++i;
    }

    TickHazardSubUVEffects(DeltaTime);

    // 2) 연쇄 폭발 타이머 처리. 시간이 다 된 항목은 폭발 + Hazard destroy.
    for (size_t i = 0; i < Pending.size();)
    {
        Pending[i].TimeRemaining -= DeltaTime;
        if (Pending[i].TimeRemaining <= 0.0f)
        {
            AActor* Source = Pending[i].SourceHazard.Get();
            const FVector Center = Pending[i].Center;
            Pending.erase(Pending.begin() + i);

            // 그 위치에서 다시 폭발 (반경 내 collectible push + 또 다른 Hazard 예약).
            if (ActorAlive(Source))
            {
                TriggerImmediate(World, Source, Center);
            }
            // 새 Pending이 추가되었을 수 있으므로 인덱스 재시작.
            i = 0;
            continue;
        }
        ++i;
    }

    // 3) Pushed actor 처리: 이동 + Rock 슬라이드 + collectible 도미노 + 감속.
    // Distance-delayed explosion pushes. Chain explosions are processed first above,
    // so a nearer Hazard can explode before farther collectibles start moving.
    for (size_t i = 0; i < PendingPushes.size();)
    {
        PendingPushes[i].TimeRemaining -= DeltaTime;
        if (PendingPushes[i].TimeRemaining <= 0.0f)
        {
            AActor* Actor = PendingPushes[i].Actor;
            const FVector Velocity = PendingPushes[i].Velocity;
            PendingPushes.erase(PendingPushes.begin() + i);

            if (ActorAlive(Actor))
            {
                ApplyKnockback(Actor, Velocity);
                UE_LOG("[ExplosionPush] name=%s tag=%s",
                       *Actor->GetName(),
                       Actor->GetTag().c_str());
            }
            continue;
        }
        ++i;
    }

    const float DampFactor = std::max(0.0f, 1.0f - Damping * DeltaTime);
    const float MinSpeedSq = MinSpeed * MinSpeed;

    for (size_t i = 0; i < Pushed.size();)
    {
        FPushedActor& Entry = Pushed[i];

        if (!ActorAlive(Entry.Actor))
        {
            Pushed.erase(Pushed.begin() + i);
            continue;
        }

        Entry.Actor->AddActorWorldOffset(Entry.Velocity * DeltaTime);

        HandleRockCollision(Entry.Actor, Entry.Velocity);
        HandleCollectibleCollision(Entry.Actor, Entry.Velocity);

        Entry.Velocity = Entry.Velocity * DampFactor;

        if (Entry.Velocity.SizeSquared() <= MinSpeedSq)
        {
            Pushed.erase(Pushed.begin() + i);
            continue;
        }

        ++i;
    }
}
