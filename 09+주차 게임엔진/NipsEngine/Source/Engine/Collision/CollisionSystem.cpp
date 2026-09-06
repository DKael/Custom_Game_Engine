#include "CollisionSystem.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "Component/Movement/MovementComponent.h"
#include "Component/Script/ScriptComponent.h"
#include "Core/ActorTags.h"
#include "Core/Logging/Stats.h"
#include "Core/Logging/Log.h"
#include "Core/SoundManager.h"
#include "DriftSalvage/ExplosionSystem.h"
#include "Engine/Scripting/LuaBinder.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/World.h"

namespace
{
    FVector ComputeAABBDepenetration(const FAABB& MovingBox, const FAABB& BlockingBox);

    struct FOrientedBoxData
    {
        FVector Center = FVector::ZeroVector;
        FVector Axis[3] = { FVector::ForwardVector, FVector::RightVector, FVector::UpVector };
        float Extent[3] = { 0.0f, 0.0f, 0.0f };
    };

    struct FCapsuleData
    {
        FVector SegmentStart = FVector::ZeroVector;
        FVector SegmentEnd = FVector::ZeroVector;
        FVector Up = FVector::UpVector;
        float Radius = 0.0f;
        float HalfHeight = 0.0f;
        float SegmentHalfHeight = 0.0f;
    };

    bool HasMovementComponent(const AActor* Actor)
    {
        if (!Actor)
        {
            return false;
        }

        for (UActorComponent* Component : Actor->GetComponents())
        {
            if (Cast<UMovementComponent>(Component))
            {
                return true;
            }
        }

        return false;
    }

    bool IsFixedCollisionActor(const AActor* Actor)
    {
        return Actor && (Actor->CompareTag(ActorTags::Rock) || Actor->CompareTag(ActorTags::Tire));
    }

    bool IsForcedMovableActor(const AActor* Actor)
    {
        return Actor && Actor->CompareTag(ActorTags::Boat);
    }

    bool IsCollisionActorAlive(const AActor* Actor)
    {
        return Actor && Actor->IsActive() && !Actor->IsPendingDestroy() && !Actor->IsBeingDestroyed();
    }

    bool IsCollisionShapeAlive(UShapeComponent* Shape)
    {
        return Shape && Shape->IsActive() && IsCollisionActorAlive(Shape->GetOwner());
    }

    bool IsBoatRockPair(const UShapeComponent* A, const UShapeComponent* B)
    {
        const AActor* ActorA = A ? A->GetOwner() : nullptr;
        const AActor* ActorB = B ? B->GetOwner() : nullptr;
        if (!ActorA || !ActorB)
        {
            return false;
        }

        return (ActorA->CompareTag(ActorTags::Boat) && ActorB->CompareTag(ActorTags::Rock)) ||
               (ActorA->CompareTag(ActorTags::Rock) && ActorB->CompareTag(ActorTags::Boat));
    }

    bool IsBoatHazardPair(const UShapeComponent* A, const UShapeComponent* B)
    {
        const AActor* ActorA = A ? A->GetOwner() : nullptr;
        const AActor* ActorB = B ? B->GetOwner() : nullptr;
        if (!ActorA || !ActorB)
        {
            return false;
        }

        return (ActorA->CompareTag(ActorTags::Boat) && ActorB->CompareTag(ActorTags::Hazard)) ||
               (ActorA->CompareTag(ActorTags::Hazard) && ActorB->CompareTag(ActorTags::Boat));
    }

    bool IsBoatLighthousePair(const UShapeComponent* A, const UShapeComponent* B)
    {
        const AActor* ActorA = A ? A->GetOwner() : nullptr;
        const AActor* ActorB = B ? B->GetOwner() : nullptr;
        if (!ActorA || !ActorB)
        {
            return false;
        }

        return (ActorA->CompareTag(ActorTags::Boat) && ActorB->CompareTag(ActorTags::Lighthouse)) ||
               (ActorA->CompareTag(ActorTags::Lighthouse) && ActorB->CompareTag(ActorTags::Boat));
    }

    bool IsCollectibleActor(const AActor* Actor)
    {
        return Actor &&
               (Actor->CompareTag(ActorTags::Trash) ||
                Actor->CompareTag(ActorTags::Resource) ||
                Actor->CompareTag(ActorTags::Recyclable) ||
                Actor->CompareTag(ActorTags::Premium));
    }

    bool IsBoatCollectiblePair(const UShapeComponent* A, const UShapeComponent* B)
    {
        const AActor* ActorA = A ? A->GetOwner() : nullptr;
        const AActor* ActorB = B ? B->GetOwner() : nullptr;
        if (!ActorA || !ActorB)
        {
            return false;
        }

        return (ActorA->CompareTag(ActorTags::Boat) && IsCollectibleActor(ActorB)) ||
               (ActorB->CompareTag(ActorTags::Boat) && IsCollectibleActor(ActorA));
    }

    AActor* GetCollectibleFromBoatPair(UShapeComponent* A, UShapeComponent* B)
    {
        AActor* ActorA = A ? A->GetOwner() : nullptr;
        AActor* ActorB = B ? B->GetOwner() : nullptr;

        if (ActorA && ActorA->CompareTag(ActorTags::Boat) && IsCollectibleActor(ActorB))
        {
            return ActorB;
        }

        if (ActorB && ActorB->CompareTag(ActorTags::Boat) && IsCollectibleActor(ActorA))
        {
            return ActorA;
        }

        return nullptr;
    }

    void ApplyBoatRockKnockback(UShapeComponent* A, UShapeComponent* B)
    {
        if (!IsBoatRockPair(A, B))
        {
            return;
        }

        AActor* ActorA = A ? A->GetOwner() : nullptr;
        AActor* ActorB = B ? B->GetOwner() : nullptr;
        if (!ActorA || !ActorB)
        {
            return;
        }

        UShapeComponent* BoatShape = ActorA->CompareTag(ActorTags::Boat) ? A : B;
        UShapeComponent* RockShape = ActorA->CompareTag(ActorTags::Rock) ? A : B;
        AActor* BoatActor = BoatShape ? BoatShape->GetOwner() : nullptr;
        if (!BoatShape || !RockShape || !BoatActor)
        {
            return;
        }

        LuaBinder::ApplyDriftSalvageDamage(1);
        FSoundManager::Get().PlaySFX("RockCollision.mp3" ,2.f);

        FVector PushForA = ComputeAABBDepenetration(A->GetWorldAABB(), B->GetWorldAABB());
        FVector Knockback = (BoatShape == A) ? PushForA : (PushForA * -1.0f);
        Knockback = Knockback.GetSafeNormal2D();

        if (Knockback.IsNearlyZero())
        {
            Knockback = (BoatShape->GetWorldLocation() - RockShape->GetWorldLocation()).GetSafeNormal2D();
        }

        if (Knockback.IsNearlyZero())
        {
            return;
        }

        // 즉시 offset 대신 ExplosionSystem에 push velocity 등록 → 매 프레임 감속하며 부드럽게 밀림.
        constexpr float BoatRockKnockbackSpeed = 6.0f;
        if (UWorld* World = BoatActor->GetFocusedWorld())
        {
            World->GetExplosionSystem().ApplyKnockback(BoatActor, Knockback * BoatRockKnockbackSpeed);
        }

    }

    void ApplyBoatHazardExplosion(UShapeComponent* A, UShapeComponent* B)
    {
        if (!IsBoatHazardPair(A, B))
        {
            return;
        }

        AActor* ActorA = A ? A->GetOwner() : nullptr;
        AActor* ActorB = B ? B->GetOwner() : nullptr;
        if (!ActorA || !ActorB)
        {
            return;
        }

        UShapeComponent* BoatShape = ActorA->CompareTag(ActorTags::Boat) ? A : B;
        UShapeComponent* HazardShape = ActorA->CompareTag(ActorTags::Hazard) ? A : B;
        AActor* BoatActor = BoatShape ? BoatShape->GetOwner() : nullptr;
        AActor* HazardActor = HazardShape ? HazardShape->GetOwner() : nullptr;
        if (!BoatActor || !HazardActor || HazardActor->IsPendingDestroy())
        {
            return;
        }

        // Boat가 이미 폭발에 떠밀리는 중이면 새 Hazard 충돌 무시 — Boat가 도미노 도화선이 되어
        // 모든 Hazard를 한 번에 폭파시키는 현상을 막는다. 멈춘 뒤에야 다음 Hazard와 충돌.
        if (UWorld* W = BoatActor->GetFocusedWorld())
        {
            if (W->GetExplosionSystem().IsActorPushed(BoatActor))
            {
                return;
            }
        }

        // 1) Boat 강한 push velocity (Rock보다 큼). 즉시 offset 대신 ExplosionSystem에 등록.
        FVector PushForA = ComputeAABBDepenetration(A->GetWorldAABB(), B->GetWorldAABB());
        FVector Knockback = (BoatShape == A) ? PushForA : (PushForA * -1.0f);
        Knockback = Knockback.GetSafeNormal2D();
        if (Knockback.IsNearlyZero())
        {
            Knockback = (BoatShape->GetWorldLocation() - HazardShape->GetWorldLocation()).GetSafeNormal2D();
        }

        // 2) Hazard가 사라지기 전에 폭발 이벤트를 보낸다. SourceHazard를 넘겨 자기 자신은 연쇄 큐에서 제외한다.
        const FVector ExplosionCenter = HazardActor->GetActorLocation();
        UWorld* World = HazardActor->GetFocusedWorld();
        LuaBinder::ApplyDriftSalvageDamage(2);

        if (World)
        {
            constexpr float BoatHazardKnockbackSpeed = 14.0f;
            if (!Knockback.IsNearlyZero())
            {
                World->GetExplosionSystem().ApplyKnockback(BoatActor, Knockback * BoatHazardKnockbackSpeed);
            }
            // 3) 폭발 트리거: 반경 안의 Trash/Resource/Recyclable/Premium에 push, 다른 Hazard는 거리 비례 딜레이로 연쇄.
            World->GetExplosionSystem().TriggerExplosion(World, HazardActor, ExplosionCenter);
        }
    }

    void TriggerBoatLighthouseGameOver(UShapeComponent* A, UShapeComponent* B)
    {
        // Boat-Lighthouse overlap에서는 별도 엔딩 플래그만 세운다. HUD의 OnUpdate가 이를 소비해서
        // 카메라 전환·GoalIn SFX·보트 자동 전진·GameOver UI까지 한 호흡으로 처리한다.
        // 체력 0 침몰 시퀀스(StartHealthZeroGameOverSequence)는 의도적으로 우회한다.
        if (!IsBoatLighthousePair(A, B))
        {
            return;
        }

        LuaBinder::RequestLighthouseEnding();
    }

    void CollectBoatOverlapTarget(UShapeComponent* A, UShapeComponent* B)
    {
        AActor* Collectible = GetCollectibleFromBoatPair(A, B);
        if (!Collectible || Collectible->IsPendingDestroy())
        {
            return;
        }

        // Boat 또는 collectible이 폭발/충돌 push 중이면 이 overlap은 회수로 보지 않는다.
        // 폭발은 삭제가 아니라 밀어내기 효과여야 하므로, 움직임이 끝난 뒤의 입력 수집만 허용한다.
        AActor* BoatActor = (A && A->GetOwner() && A->GetOwner()->CompareTag(ActorTags::Boat))
                                ? A->GetOwner()
                                : (B ? B->GetOwner() : nullptr);
        if (BoatActor)
        {
            if (UWorld* W = BoatActor->GetFocusedWorld())
            {
                if (W->GetExplosionSystem().IsActorPushed(BoatActor) ||
                    W->GetExplosionSystem().IsActorPushed(Collectible))
                {
                    return;
                }
            }
        }

        if (LuaBinder::TryApplyDriftSalvagePickup(Collectible->GetTag()))
        {
            FSoundManager::Get().PlaySFX("Pick.mp3", 0.3f);
            UE_LOG("[CollectByBoatOverlap] name=%s tag=%s",
                   *Collectible->GetName(),
                   Collectible->GetTag().c_str());
            Collectible->Destroy();
        }
    }

    void DispatchScriptOverlapBegin(const FCollisionEvent& Event)
    {
        if (!Event.SelfActor)
        {
            return;
        }

        for (UActorComponent* Component : Event.SelfActor->GetComponents())
        {
            if (UScriptComponent* Script = Cast<UScriptComponent>(Component))
            {
                Script->OnOverlapBegin(Event.OtherActor);
            }
        }
    }

    void DispatchScriptOverlapEnd(const FCollisionEvent& Event)
    {
        if (!Event.SelfActor)
        {
            return;
        }

        for (UActorComponent* Component : Event.SelfActor->GetComponents())
        {
            if (UScriptComponent* Script = Cast<UScriptComponent>(Component))
            {
                Script->OnOverlapEnd(Event.OtherActor);
            }
        }
    }

    void DispatchScriptHit(const FCollisionEvent& Event)
    {
        if (!Event.SelfActor)
        {
            return;
        }

        for (UActorComponent* Component : Event.SelfActor->GetComponents())
        {
            if (UScriptComponent* Script = Cast<UScriptComponent>(Component))
            {
                Script->OnHit(Event.OtherActor);
            }
        }
    }

    float Clamp(float Value, float Min, float Max)
    {
        return std::max(Min, std::min(Value, Max));
    }

    bool AABBIntersects(const FAABB& A, const FAABB& B)
    {
        return A.Min.X <= B.Max.X && A.Max.X >= B.Min.X &&
               A.Min.Y <= B.Max.Y && A.Max.Y >= B.Min.Y &&
               A.Min.Z <= B.Max.Z && A.Max.Z >= B.Min.Z;
    }

    FVector ComputeAABBDepenetration(const FAABB& MovingBox, const FAABB& BlockingBox)
    {
        if (!AABBIntersects(MovingBox, BlockingBox))
        {
            return FVector::ZeroVector;
        }

        const FVector MovingCenter = MovingBox.GetCenter();
        const FVector BlockingCenter = BlockingBox.GetCenter();

        const float PushX = std::min(MovingBox.Max.X - BlockingBox.Min.X, BlockingBox.Max.X - MovingBox.Min.X);
        const float PushY = std::min(MovingBox.Max.Y - BlockingBox.Min.Y, BlockingBox.Max.Y - MovingBox.Min.Y);
        const float PushZ = std::min(MovingBox.Max.Z - BlockingBox.Min.Z, BlockingBox.Max.Z - MovingBox.Min.Z);

        constexpr float Skin = 0.1f;

        if (PushX <= PushY && PushX <= PushZ)
        {
            return FVector((MovingCenter.X >= BlockingCenter.X ? 1.0f : -1.0f) * (PushX + Skin), 0.0f, 0.0f);
        }

        if (PushY <= PushX && PushY <= PushZ)
        {
            return FVector(0.0f, (MovingCenter.Y >= BlockingCenter.Y ? 1.0f : -1.0f) * (PushY + Skin), 0.0f);
        }

        return FVector(0.0f, 0.0f, (MovingCenter.Z >= BlockingCenter.Z ? 1.0f : -1.0f) * (PushZ + Skin));
    }

    float MaxAbs3(const FVector& V)
    {
        return std::max({ std::fabs(V.X), std::fabs(V.Y), std::fabs(V.Z) });
    }

    float CapsuleRadiusScale(const FVector& Scale)
    {
        return std::max(std::fabs(Scale.X), std::fabs(Scale.Y));
    }

    float CapsuleHeightScale(const FVector& Scale)
    {
        return std::fabs(Scale.Z);
    }

    FOrientedBoxData BuildOrientedBoxData(const UBoxComponent* Box)
    {
        FOrientedBoxData Data;
        const FVector Scale = Box->GetWorldAxisScale();
        const FVector Extent = Box->GetBoxExtent();

        Data.Center = Box->GetWorldLocation();
        Data.Axis[0] = Box->GetForwardVector();
        Data.Axis[1] = Box->GetRightVector();
        Data.Axis[2] = Box->GetUpVector();
        Data.Extent[0] = std::fabs(Extent.X * Scale.X);
        Data.Extent[1] = std::fabs(Extent.Y * Scale.Y);
        Data.Extent[2] = std::fabs(Extent.Z * Scale.Z);

        return Data;
    }

    FVector ClosestPointOnAABB(const FVector& Point, const FAABB& Box)
    {
        return FVector(
            Clamp(Point.X, Box.Min.X, Box.Max.X),
            Clamp(Point.Y, Box.Min.Y, Box.Max.Y),
            Clamp(Point.Z, Box.Min.Z, Box.Max.Z)
        );
    }

    float PointAABBDistanceSq(const FVector& Point, const FAABB& Box)
    {
        return FVector::DistSquared(Point, ClosestPointOnAABB(Point, Box));
    }

    bool SegmentIntersectsAABB(const FVector& SegmentStart, const FVector& SegmentEnd, const FAABB& Box)
    {
        float TMin = 0.0f;
        float TMax = 1.0f;
        const FVector Direction = SegmentEnd - SegmentStart;

        for (int32 AxisIndex = 0; AxisIndex < 3; ++AxisIndex)
        {
            const float Start = SegmentStart[AxisIndex];
            const float Dir = Direction[AxisIndex];
            const float Min = Box.Min[AxisIndex];
            const float Max = Box.Max[AxisIndex];

            if (std::fabs(Dir) <= 0.0001f)
            {
                if (Start < Min || Start > Max)
                {
                    return false;
                }

                continue;
            }

            const float InvDir = 1.0f / Dir;
            float T1 = (Min - Start) * InvDir;
            float T2 = (Max - Start) * InvDir;

            if (T1 > T2)
            {
                std::swap(T1, T2);
            }

            TMin = std::max(TMin, T1);
            TMax = std::min(TMax, T2);

            if (TMin > TMax)
            {
                return false;
            }
        }

        return true;
    }

    FVector WorldToOrientedBoxSpace(const FVector& Point, const FOrientedBoxData& Box)
    {
        const FVector Delta = Point - Box.Center;
        return FVector(
            Delta.DotProduct(Box.Axis[0]),
            Delta.DotProduct(Box.Axis[1]),
            Delta.DotProduct(Box.Axis[2])
        );
    }

    FVector OrientedBoxSpaceToWorld(const FVector& Point, const FOrientedBoxData& Box)
    {
        return Box.Center +
            Box.Axis[0] * Point.X +
            Box.Axis[1] * Point.Y +
            Box.Axis[2] * Point.Z;
    }

    FVector ClosestPointOnOrientedBox(const FVector& Point, const FOrientedBoxData& Box)
    {
        const FVector LocalPoint = WorldToOrientedBoxSpace(Point, Box);
        const FVector LocalClosest(
            Clamp(LocalPoint.X, -Box.Extent[0], Box.Extent[0]),
            Clamp(LocalPoint.Y, -Box.Extent[1], Box.Extent[1]),
            Clamp(LocalPoint.Z, -Box.Extent[2], Box.Extent[2])
        );

        return OrientedBoxSpaceToWorld(LocalClosest, Box);
    }

    FVector ClosestPointOnSegment(const FVector& Point, const FVector& SegmentStart, const FVector& SegmentEnd)
    {
        const FVector Segment = SegmentEnd - SegmentStart;
        const float SegmentLengthSq = Segment.SizeSquared();
        if (SegmentLengthSq <= 0.0001f)
        {
            return SegmentStart;
        }

        const float T = Clamp((Point - SegmentStart).DotProduct(Segment) / SegmentLengthSq, 0.0f, 1.0f);
        return SegmentStart + Segment * T;
    }

    float SegmentSegmentDistanceSq(
        const FVector& P1,
        const FVector& Q1,
        const FVector& P2,
        const FVector& Q2)
    {
        const FVector D1 = Q1 - P1;
        const FVector D2 = Q2 - P2;
        const FVector R = P1 - P2;
        const float A = D1.DotProduct(D1);
        const float E = D2.DotProduct(D2);
        const float F = D2.DotProduct(R);

        float S = 0.0f;
        float T = 0.0f;

        if (A <= 0.0001f && E <= 0.0001f)
        {
            return FVector::DistSquared(P1, P2);
        }

        if (A <= 0.0001f)
        {
            T = Clamp(F / E, 0.0f, 1.0f);
        }
        else
        {
            const float C = D1.DotProduct(R);
            if (E <= 0.0001f)
            {
                S = Clamp(-C / A, 0.0f, 1.0f);
            }
            else
            {
                const float B = D1.DotProduct(D2);
                const float Denom = A * E - B * B;

                if (Denom != 0.0f)
                {
                    S = Clamp((B * F - C * E) / Denom, 0.0f, 1.0f);
                }

                T = (B * S + F) / E;

                if (T < 0.0f)
                {
                    T = 0.0f;
                    S = Clamp(-C / A, 0.0f, 1.0f);
                }
                else if (T > 1.0f)
                {
                    T = 1.0f;
                    S = Clamp((B - C) / A, 0.0f, 1.0f);
                }
            }
        }

        const FVector Closest1 = P1 + D1 * S;
        const FVector Closest2 = P2 + D2 * T;
        return FVector::DistSquared(Closest1, Closest2);
    }

    FCapsuleData BuildCapsuleData(const UCapsuleComponent* Capsule)
    {
        FCapsuleData Data;
        const FVector WorldScale = Capsule->GetWorldAxisScale();

        Data.Radius = Capsule->GetCapsuleRadius() * CapsuleRadiusScale(WorldScale);
        Data.HalfHeight = Capsule->GetCapsuleHalfHeight() * CapsuleHeightScale(WorldScale);
        Data.HalfHeight = std::max(Data.HalfHeight, Data.Radius);
        Data.SegmentHalfHeight = std::max(0.0f, Data.HalfHeight - Data.Radius);
        Data.Up = Capsule->GetUpVector();

        const FVector Center = Capsule->GetWorldLocation();
        Data.SegmentStart = Center - Data.Up * Data.SegmentHalfHeight;
        Data.SegmentEnd = Center + Data.Up * Data.SegmentHalfHeight;

        return Data;
    }

    float SegmentAABBDistanceSq(const FVector& SegmentStart, const FVector& SegmentEnd, const FAABB& Box)
    {
        if (SegmentIntersectsAABB(SegmentStart, SegmentEnd, Box))
        {
            return 0.0f;
        }

        float MinDistanceSq = std::min(
            PointAABBDistanceSq(SegmentStart, Box),
            PointAABBDistanceSq(SegmentEnd, Box));

        const FVector V[8] =
        {
            FVector(Box.Min.X, Box.Min.Y, Box.Min.Z),
            FVector(Box.Max.X, Box.Min.Y, Box.Min.Z),
            FVector(Box.Min.X, Box.Max.Y, Box.Min.Z),
            FVector(Box.Max.X, Box.Max.Y, Box.Min.Z),
            FVector(Box.Min.X, Box.Min.Y, Box.Max.Z),
            FVector(Box.Max.X, Box.Min.Y, Box.Max.Z),
            FVector(Box.Min.X, Box.Max.Y, Box.Max.Z),
            FVector(Box.Max.X, Box.Max.Y, Box.Max.Z),
        };

        const int32 Edges[12][2] =
        {
            {0, 1}, {1, 3}, {3, 2}, {2, 0},
            {4, 5}, {5, 7}, {7, 6}, {6, 4},
            {0, 4}, {1, 5}, {2, 6}, {3, 7},
        };

        for (const auto& Edge : Edges)
        {
            MinDistanceSq = std::min(
                MinDistanceSq,
                SegmentSegmentDistanceSq(SegmentStart, SegmentEnd, V[Edge[0]], V[Edge[1]]));
        }

        return MinDistanceSq;
    }

    float SegmentOrientedBoxDistanceSq(
        const FVector& SegmentStart,
        const FVector& SegmentEnd,
        const FOrientedBoxData& Box)
    {
        const FVector LocalStart = WorldToOrientedBoxSpace(SegmentStart, Box);
        const FVector LocalEnd = WorldToOrientedBoxSpace(SegmentEnd, Box);
        const FAABB LocalBox(
            FVector(-Box.Extent[0], -Box.Extent[1], -Box.Extent[2]),
            FVector(Box.Extent[0], Box.Extent[1], Box.Extent[2])
        );

        return SegmentAABBDistanceSq(LocalStart, LocalEnd, LocalBox);
    }

    bool OrientedBoxOrientedBoxOverlap(const FOrientedBoxData& A, const FOrientedBoxData& B)
    {
        constexpr float Epsilon = 1e-4f;

        float R[3][3];
        float AbsR[3][3];

        for (int32 i = 0; i < 3; ++i)
        {
            for (int32 j = 0; j < 3; ++j)
            {
                R[i][j] = A.Axis[i].DotProduct(B.Axis[j]); // B의 축이 A의 축 기준으로 얼마나 기울어져 있는지
                AbsR[i][j] = std::fabs(R[i][j]) + Epsilon; // 절댓값 저장. (+Epsilon은 부동소수점 오차 보정)
            }
        }

        const FVector CenterDelta = B.Center - A.Center;
        const float T[3] =
        {
            CenterDelta.DotProduct(A.Axis[0]), // B의 중심이 A.X축 방향으로 얼마나 떨어져 있는가
            CenterDelta.DotProduct(A.Axis[1]),
            CenterDelta.DotProduct(A.Axis[2])
        };

        float RA = 0.0f;
        float RB = 0.0f;

        // A의 축 3개 검사 (basis test)
        for (int32 i = 0; i < 3; ++i)
        {
            RA = A.Extent[i];
            RB = B.Extent[0] * AbsR[i][0] + B.Extent[1] * AbsR[i][1] + B.Extent[2] * AbsR[i][2];
            if (std::fabs(T[i]) > RA + RB) // 두 중심 사이 거리 > A의 투영 반지름 + B의 투영 반지름 -> 투영 구간 떨어짐 -> 충돌 X
            {
                return false;
            }
        }
        // B의 축 3개 검사 (basis test)
        for (int32 j = 0; j < 3; ++j)
        {
            RA = A.Extent[0] * AbsR[0][j] + A.Extent[1] * AbsR[1][j] + A.Extent[2] * AbsR[2][j];
            RB = B.Extent[j];
            if (std::fabs(T[0] * R[0][j] + T[1] * R[1][j] + T[2] * R[2][j]) > RA + RB)
            {
                return false;
            }
        }
        // A축 x B축 (edge test)
        for (int32 i = 0; i < 3; ++i)
        {
            for (int32 j = 0; j < 3; ++j)
            {
                RA = A.Extent[(i + 1) % 3] * AbsR[(i + 2) % 3][j] +
                     A.Extent[(i + 2) % 3] * AbsR[(i + 1) % 3][j];
                RB = B.Extent[(j + 1) % 3] * AbsR[i][(j + 2) % 3] +
                     B.Extent[(j + 2) % 3] * AbsR[i][(j + 1) % 3];

                const float Distance = std::fabs(
                    T[(i + 2) % 3] * R[(i + 1) % 3][j] -
                    T[(i + 1) % 3] * R[(i + 2) % 3][j]
                );

                if (Distance > RA + RB)
                {
                    return false;
                }
            }
        }

        return true;
    }

    FVector AABBIntersectionCenter(const FAABB& A, const FAABB& B)
    {
        FVector Min(
            std::max(A.Min.X, B.Min.X),
            std::max(A.Min.Y, B.Min.Y),
            std::max(A.Min.Z, B.Min.Z)
        );

        FVector Max(
            std::min(A.Max.X, B.Max.X),
            std::min(A.Max.Y, B.Max.Y),
            std::min(A.Max.Z, B.Max.Z)
        );

        return (Min + Max) * 0.5f;
    }

    bool BuildAABBIntersection(const FAABB& A, const FAABB& B, FAABB& OutIntersection)
    {
        if (!AABBIntersects(A, B))
        {
            return false;
        }

        OutIntersection.Min = FVector(
            std::max(A.Min.X, B.Min.X),
            std::max(A.Min.Y, B.Min.Y),
            std::max(A.Min.Z, B.Min.Z)
        );

        OutIntersection.Max = FVector(
            std::min(A.Max.X, B.Max.X),
            std::min(A.Max.Y, B.Max.Y),
            std::min(A.Max.Z, B.Max.Z)
        );

        return OutIntersection.IsValid();
    }
}

void FCollisionSystem::Tick(UWorld* World, float DeltaTime)
{
    SCOPE_STAT("CollisionSystem.Tick");
    (void)DeltaTime;

    DebugContacts.clear();
    DebugLines.clear();

    TArray<UShapeComponent*> Shapes;
    CollectShapeComponents(World, Shapes);

    TSet<FCollisionPair, FCollisionPairHash> CurrentOverlaps;
    TSet<UShapeComponent*> LiveShapes;
    LiveShapes.reserve(Shapes.size());
    for (UShapeComponent* Shape : Shapes)
    {
        LiveShapes.insert(Shape);
    }

    for (UShapeComponent* A : Shapes)
    {
        const FAABB& ShapeBounds = A->GetWorldAABB();
        const FVector QueryCenter = ShapeBounds.GetCenter();
        const float QueryRadius = (ShapeBounds.Max - ShapeBounds.Min).Size() * 0.5f;

        CollisionCandidatePrimitives.clear();
        World->GetSpatialIndex().SphereQueryPrimitives(
            QueryCenter,
            QueryRadius,
            CollisionCandidatePrimitives,
            CollisionSphereQueryScratch);

        for (UPrimitiveComponent* CandidatePrimitive : CollisionCandidatePrimitives)
        {
            UShapeComponent* B = Cast<UShapeComponent>(CandidatePrimitive);
            if (!B || A >= B || LiveShapes.find(B) == LiveShapes.end())
            {
                continue;
            }

            if (!ShouldTestPair(A, B) || !AreOverlapping(A, B))
            {
                continue;
            }

            AddDebugContact(A, B);

            FCollisionPair Pair(A, B);
            CurrentOverlaps.insert(Pair);

            if (bHasInitializedOverlaps && PreviousOverlaps.find(Pair) == PreviousOverlaps.end())
            {
                HandleBeginOverlap(A, B);

                if (A->GetBlockComponent() && B->GetBlockComponent())
                {
                    HandleHit(A, B);
                }
            }

            if (!IsBoatLighthousePair(A, B) &&
                ((A->GetBlockComponent() && B->GetBlockComponent()) || IsBoatRockPair(A, B)))
            {
                ResolveBlockingOverlap(A, B);
            }
        }
    }

    for (const FCollisionPair& Pair : PreviousOverlaps)
    {
        const bool bAAlive = LiveShapes.find(Pair.A) != LiveShapes.end();
        const bool bBAlive = LiveShapes.find(Pair.B) != LiveShapes.end();

        if (!bAAlive && !bBAlive)
        {
            continue;
        }

        if (!bAAlive)
        {
            Pair.B->RemoveOverlap(Pair.A);
            continue;
        }

        if (!bBAlive)
        {
            Pair.A->RemoveOverlap(Pair.B);
            continue;
        }

        if (CurrentOverlaps.find(Pair) == CurrentOverlaps.end())
        {
            HandleEndOverlap(Pair.A, Pair.B);
        }
    }

    PreviousOverlaps.clear();
    for (const FCollisionPair& Pair : CurrentOverlaps)
    {
        if (IsCollisionShapeAlive(Pair.A) && IsCollisionShapeAlive(Pair.B))
        {
            PreviousOverlaps.insert(Pair);
        }
    }

    bHasInitializedOverlaps = true;
}

void FCollisionSystem::Reset()
{
    bHasInitializedOverlaps = false;
    PreviousOverlaps.clear();
    DebugContacts.clear();
    DebugLines.clear();
    CollisionCandidatePrimitives.clear();
    CollisionSphereQueryScratch.ObjectIndices.clear();
    CollisionSphereQueryScratch.BVHScratch.TraversalStack.clear();
}

void FCollisionSystem::CollectShapeComponents(UWorld* World, TArray<UShapeComponent*>& OutShapes)
{
    OutShapes.clear();

    if (!World)
    {
        return;
    }

    for (AActor* Actor : World->GetActors())
    {
        if (!IsCollisionActorAlive(Actor))
        {
            continue;
        }

        for (UActorComponent* Component : Actor->GetComponents())
        {
            UShapeComponent* Shape = Cast<UShapeComponent>(Component);
            if (!Shape || !Shape->IsActive())
            {
                continue;
            }

            OutShapes.push_back(Shape);
        }
    }
}

bool FCollisionSystem::ShouldTestPair(const UShapeComponent* A, const UShapeComponent* B) const
{
    if (!A || !B || A == B)
    {
        return false;
    }

    if (A->GetOwner() == B->GetOwner())
    {
        return false;
    }

    return A->GetGenerateOverlapEvents() || B->GetGenerateOverlapEvents() ||
           A->GetBlockComponent() || B->GetBlockComponent() ||
           IsBoatRockPair(A, B) || IsBoatCollectiblePair(A, B) || IsBoatLighthousePair(A, B);
}

bool FCollisionSystem::AreOverlapping(UShapeComponent* A, UShapeComponent* B) const
{
    if (!AABBOverlap(A, B))
    {
        return false;
    }

    if (Cast<USphereComponent>(A) && Cast<USphereComponent>(B))
    {
        return SphereSphere(A, B);
    }

    if (Cast<USphereComponent>(A) && Cast<UBoxComponent>(B))
    {
        return SphereBox(A, B);
    }

    if (Cast<UBoxComponent>(A) && Cast<USphereComponent>(B))
    {
        return SphereBox(B, A);
    }

    if (Cast<UBoxComponent>(A) && Cast<UBoxComponent>(B))
    {
        return BoxBox(A, B);
    }

    if (Cast<UCapsuleComponent>(A) && Cast<UCapsuleComponent>(B))
    {
        return CapsuleCapsule(A, B);
    }

    if (Cast<USphereComponent>(A) && Cast<UCapsuleComponent>(B))
    {
        return SphereCapsule(A, B);
    }

    if (Cast<UCapsuleComponent>(A) && Cast<USphereComponent>(B))
    {
        return SphereCapsule(B, A);
    }

    if (Cast<UBoxComponent>(A) && Cast<UCapsuleComponent>(B))
    {
        return BoxCapsule(A, B);
    }

    if (Cast<UCapsuleComponent>(A) && Cast<UBoxComponent>(B))
    {
        return BoxCapsule(B, A);
    }

    return false;
}

bool FCollisionSystem::AABBOverlap(UShapeComponent* A, UShapeComponent* B) const
{
    return AABBIntersects(A->GetWorldAABB(), B->GetWorldAABB());
}

bool FCollisionSystem::SphereSphere(UShapeComponent* A, UShapeComponent* B) const
{
    USphereComponent* SphereA = Cast<USphereComponent>(A);
    USphereComponent* SphereB = Cast<USphereComponent>(B);

    const float RadiusA = SphereA->GetSphereRadius() * MaxAbs3(SphereA->GetWorldAxisScale());
    const float RadiusB = SphereB->GetSphereRadius() * MaxAbs3(SphereB->GetWorldAxisScale());

    const float RadiusSum = RadiusA + RadiusB;
    const float DistSq = FVector::DistSquared(SphereA->GetWorldLocation(), SphereB->GetWorldLocation());

    return DistSq <= RadiusSum * RadiusSum;
}

bool FCollisionSystem::SphereBox(UShapeComponent* Sphere, UShapeComponent* Box) const
{
    USphereComponent* SphereComp = Cast<USphereComponent>(Sphere);
    UBoxComponent* BoxComp = Cast<UBoxComponent>(Box);

    if (!SphereComp || !BoxComp)
    {
        return false;
    }

    const FVector SphereCenter = SphereComp->GetWorldLocation();
    const float SphereRadius = SphereComp->GetSphereRadius() * MaxAbs3(SphereComp->GetWorldAxisScale());

    const FVector Closest = ClosestPointOnOrientedBox(SphereCenter, BuildOrientedBoxData(BoxComp));
    const float DistSq = FVector::DistSquared(SphereCenter, Closest);

    return DistSq <= SphereRadius * SphereRadius;
}

bool FCollisionSystem::BoxBox(UShapeComponent* A, UShapeComponent* B) const
{
    UBoxComponent* BoxA = Cast<UBoxComponent>(A);
    UBoxComponent* BoxB = Cast<UBoxComponent>(B);
    if (!BoxA || !BoxB)
    {
        return false;
    }

    return OrientedBoxOrientedBoxOverlap(BuildOrientedBoxData(BoxA), BuildOrientedBoxData(BoxB));
}

bool FCollisionSystem::CapsuleCapsule(UShapeComponent* A, UShapeComponent* B) const
{
    UCapsuleComponent* CapsuleA = Cast<UCapsuleComponent>(A);
    UCapsuleComponent* CapsuleB = Cast<UCapsuleComponent>(B);
    if (!CapsuleA || !CapsuleB)
    {
        return false;
    }

    const FCapsuleData DataA = BuildCapsuleData(CapsuleA);
    const FCapsuleData DataB = BuildCapsuleData(CapsuleB);

    const float RadiusSum = DataA.Radius + DataB.Radius;
    return SegmentSegmentDistanceSq(
        DataA.SegmentStart,
        DataA.SegmentEnd,
        DataB.SegmentStart,
        DataB.SegmentEnd) <= RadiusSum * RadiusSum;
}

bool FCollisionSystem::SphereCapsule(UShapeComponent* Sphere, UShapeComponent* Capsule) const
{
    USphereComponent* SphereComp = Cast<USphereComponent>(Sphere);
    UCapsuleComponent* CapsuleComp = Cast<UCapsuleComponent>(Capsule);
    if (!SphereComp || !CapsuleComp)
    {
        return false;
    }

    const FCapsuleData CapsuleData = BuildCapsuleData(CapsuleComp);

    const float SphereRadius = SphereComp->GetSphereRadius() * MaxAbs3(SphereComp->GetWorldAxisScale());
    const FVector Closest = ClosestPointOnSegment(
        SphereComp->GetWorldLocation(),
        CapsuleData.SegmentStart,
        CapsuleData.SegmentEnd);
    const float RadiusSum = SphereRadius + CapsuleData.Radius;

    return FVector::DistSquared(SphereComp->GetWorldLocation(), Closest) <= RadiusSum * RadiusSum;
}

bool FCollisionSystem::BoxCapsule(UShapeComponent* Box, UShapeComponent* Capsule) const
{
    UBoxComponent* BoxComp = Cast<UBoxComponent>(Box);
    UCapsuleComponent* CapsuleComp = Cast<UCapsuleComponent>(Capsule);
    if (!BoxComp || !CapsuleComp)
    {
        return false;
    }

    const FCapsuleData CapsuleData = BuildCapsuleData(CapsuleComp);

    return SegmentOrientedBoxDistanceSq(
        CapsuleData.SegmentStart,
        CapsuleData.SegmentEnd,
        BuildOrientedBoxData(BoxComp)) <= CapsuleData.Radius * CapsuleData.Radius;
}

void FCollisionSystem::HandleBeginOverlap(UShapeComponent* A, UShapeComponent* B)
{
    if (!A || !B)
    {
        return;
    }

    A->AddOverlap(B);
    B->AddOverlap(A);

    ApplyBoatRockKnockback(A, B);
    ApplyBoatHazardExplosion(A, B);
    TriggerBoatLighthouseGameOver(A, B);
    CollectBoatOverlapTarget(A, B);

    if (A->GetGenerateOverlapEvents())
    {
        const FCollisionEvent Event = MakeCollisionEvent(A, B, false);
        LogCollisionEvent("BeginOverlap", Event);
        A->DispatchBeginOverlap(Event);
        DispatchScriptOverlapBegin(Event);
    }

    if (B->GetGenerateOverlapEvents())
    {
        const FCollisionEvent Event = MakeCollisionEvent(B, A, false);
        LogCollisionEvent("BeginOverlap", Event);
        B->DispatchBeginOverlap(Event);
        DispatchScriptOverlapBegin(Event);
    }
}

void FCollisionSystem::HandleEndOverlap(UShapeComponent* A, UShapeComponent* B)
{
    if (!A || !B)
    {
        return;
    }

    A->RemoveOverlap(B);
    B->RemoveOverlap(A);

    if (A->GetGenerateOverlapEvents())
    {
        const FCollisionEvent Event = MakeCollisionEvent(A, B, false);
        LogCollisionEvent("EndOverlap", Event);
        A->DispatchEndOverlap(Event);
        DispatchScriptOverlapEnd(Event);
    }

    if (B->GetGenerateOverlapEvents())
    {
        const FCollisionEvent Event = MakeCollisionEvent(B, A, false);
        LogCollisionEvent("EndOverlap", Event);
        B->DispatchEndOverlap(Event);
        DispatchScriptOverlapEnd(Event);
    }
}

void FCollisionSystem::HandleHit(UShapeComponent* A, UShapeComponent* B)
{
    if (!A || !B)
    {
        return;
    }

    if (A->GetBlockComponent())
    {
        const FCollisionEvent Event = MakeCollisionEvent(A, B, true);
        LogCollisionEvent("Hit", Event);
        A->DispatchHit(Event);
        DispatchScriptHit(Event);
    }

    if (B->GetBlockComponent())
    {
        const FCollisionEvent Event = MakeCollisionEvent(B, A, true);
        LogCollisionEvent("Hit", Event);
        B->DispatchHit(Event);
        DispatchScriptHit(Event);
    }
}

void FCollisionSystem::ResolveBlockingOverlap(UShapeComponent* A, UShapeComponent* B)
{
    if (!A || !B)
    {
        return;
    }

    AActor* ActorA = A->GetOwner();
    AActor* ActorB = B->GetOwner();
    if (!ActorA || !ActorB)
    {
        return;
    }

    // 두 가지 movable 판정을 OR로 묶음:
    //   1) MovementComponent가 붙어 있는 액터 (기존 동작)
    //   2) ShapeComponent의 bMovable 플래그 (수동 지정)
    // -> Boat에 MovementComponent가 없어도 Shape의 Movable=true면 push 적용.
    const bool bAMovable = !IsFixedCollisionActor(ActorA) &&
                           (HasMovementComponent(ActorA) || A->GetMovable() || IsForcedMovableActor(ActorA));
    const bool bBMovable = !IsFixedCollisionActor(ActorB) &&
                           (HasMovementComponent(ActorB) || B->GetMovable() || IsForcedMovableActor(ActorB));
    if (!bAMovable && !bBMovable)
    {
        return;
    }

    const FVector PushA = ComputeAABBDepenetration(A->GetWorldAABB(), B->GetWorldAABB());
    if (PushA.IsNearlyZero())
    {
        return;
    }

    if (bAMovable && bBMovable)
    {
        ActorA->AddActorWorldOffset(PushA * 0.5f);
        ActorB->AddActorWorldOffset(PushA * -0.5f);
        return;
    }

    if (bAMovable)
    {
        ActorA->AddActorWorldOffset(PushA);
        return;
    }

    ActorB->AddActorWorldOffset(PushA * -1.0f);
}

void FCollisionSystem::AddDebugContact(UShapeComponent* A, UShapeComponent* B)
{
    if (!A || !B)
    {
        return;
    }

    FCollisionDebugContact Contact;
    Contact.A = A;
    Contact.B = B;
    Contact.Location = ComputeDebugContactLocation(A, B);
    Contact.bHasOverlapBounds = ComputeDebugOverlapBounds(A, B, Contact.OverlapBounds);

    FVector Delta = B->GetWorldLocation() - A->GetWorldLocation();
    Contact.Normal = Delta.GetSafeNormal();

    DebugContacts.push_back(Contact);
}

FVector FCollisionSystem::ComputeDebugContactLocation(UShapeComponent* A, UShapeComponent* B) const
{
    if (USphereComponent* SphereA = Cast<USphereComponent>(A))
    {
        if (USphereComponent* SphereB = Cast<USphereComponent>(B))
        {
            const FVector CenterA = SphereA->GetWorldLocation();
            const FVector CenterB = SphereB->GetWorldLocation();
            FVector Dir = (CenterB - CenterA).GetSafeNormal();

            if (Dir.SizeSquared() <= 0.0001f)
            {
                return (CenterA + CenterB) * 0.5f;
            }

            const float RadiusA = SphereA->GetSphereRadius() * MaxAbs3(SphereA->GetWorldAxisScale());
            const float RadiusB = SphereB->GetSphereRadius() * MaxAbs3(SphereB->GetWorldAxisScale());

            const FVector PointA = CenterA + Dir * RadiusA;
            const FVector PointB = CenterB - Dir * RadiusB;
            return (PointA + PointB) * 0.5f;
        }

        if (UBoxComponent* BoxB = Cast<UBoxComponent>(B))
        {
            return ClosestPointOnOrientedBox(SphereA->GetWorldLocation(), BuildOrientedBoxData(BoxB));
        }
    }

    if (UBoxComponent* BoxA = Cast<UBoxComponent>(A))
    {
        if (USphereComponent* SphereB = Cast<USphereComponent>(B))
        {
            return ClosestPointOnOrientedBox(SphereB->GetWorldLocation(), BuildOrientedBoxData(BoxA));
        }
    }

    return AABBIntersectionCenter(A->GetWorldAABB(), B->GetWorldAABB());
}

bool FCollisionSystem::ComputeDebugOverlapBounds(UShapeComponent* A, UShapeComponent* B, FAABB& OutBounds) const
{
    if (!A || !B)
    {
        return false;
    }

    return BuildAABBIntersection(A->GetWorldAABB(), B->GetWorldAABB(), OutBounds);
}

FCollisionEvent FCollisionSystem::MakeCollisionEvent(UShapeComponent* Self, UShapeComponent* Other, bool bBlockingHit) const
{
    FCollisionEvent Event;
    Event.SelfComponent = Self;
    Event.SelfActor = Self ? Self->GetOwner() : nullptr;
    Event.OtherComponent = Other;
    Event.OtherActor = Other ? Other->GetOwner() : nullptr;
    Event.bBlockingHit = bBlockingHit;

    if (Event.SelfActor)
    {
        Event.SelfTag = Event.SelfActor->GetTag();
    }

    if (Event.OtherActor)
    {
        Event.OtherTag = Event.OtherActor->GetTag();
    }

    if (!Self || !Other)
    {
        return Event;
    }

    Event.Location = ComputeDebugContactLocation(Self, Other);
    Event.Normal = (Other->GetWorldLocation() - Self->GetWorldLocation()).GetSafeNormal();
    Event.bHasOverlapBounds = ComputeDebugOverlapBounds(Self, Other, Event.OverlapBounds);

    return Event;
}

// ============================================================
// LineTraceSingle - 마우스 밀치기 / 클릭 판정용 광선 추적
//
// 단순 구현: 월드의 모든 액터 → 모든 PrimitiveComponent를 순회하며
// AABB 1차 컬링 → UPrimitiveComponent::Raycast (정확 판정).
// 액터 수가 100개 미만이면 충분한 성능. BVH/Octree는 시간 절약 위해 미사용.
// ============================================================
bool FCollisionSystem::LineTraceSingle(
    UWorld* World,
    const FVector& Start,
    const FVector& End,
    FHitResult& OutHit,
    const FString& IgnoreTag,
    bool bDrawDebug)
{
    OutHit.Reset();

    if (!World)
    {
        return false;
    }

    const FVector Delta = End - Start;
    const float MaxDistance = Delta.Size();
    if (MaxDistance <= 0.0001f)
    {
        // 시작=끝이면 광선 정의 불가
        if (bDrawDebug)
        {
            AddDebugLine(Start, End, FColor::White());
        }
        return false;
    }

    const FVector Direction = Delta * (1.0f / MaxDistance);
    FRay Ray(Start, Direction);

    // 가장 가까운 hit을 추적
    float BestDistance = MaxDistance;
    FHitResult BestHit;

    for (AActor* Actor : World->GetActors())
    {
        if (!Actor || !Actor->IsActive())
        {
            continue;
        }

        // 자기 자신 (또는 지정 태그) 무시
        if (!IgnoreTag.empty() && Actor->GetTag() == IgnoreTag)
        {
            continue;
        }

        for (UActorComponent* Component : Actor->GetComponents())
        {
            UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(Component);
            if (!Prim || !Prim->IsActive())
            {
                continue;
            }

            // UPrimitiveComponent::Raycast 가 AABB 1차 컬링 + 도형별 RaycastMesh를 묶어 처리.
            // (StaticMesh의 경우 삼각형 단위 정확 판정, ShapeComponent는 AABB만)
            FHitResult LocalHit;
            if (!Prim->Raycast(Ray, LocalHit))
            {
                continue;
            }

            // 광선 길이 안의 hit만 채택
            if (!LocalHit.bHit || LocalHit.Distance > BestDistance)
            {
                continue;
            }

            BestDistance = LocalHit.Distance;
            BestHit = LocalHit;
            BestHit.HitComponent = Prim;
        }
    }

    OutHit = BestHit;

    if (bDrawDebug)
    {
        if (OutHit.bHit)
        {
            // 시작 → Hit지점은 빨강, Hit지점 → 끝은 옅은 회색
            AddDebugLine(Start, OutHit.Location, FColor::Red());
            AddDebugLine(OutHit.Location, End, FColor::Black());
        }
        else
        {
            AddDebugLine(Start, End, FColor::Green());
        }
    }

    return OutHit.bHit;
}

void FCollisionSystem::AddDebugLine(const FVector& Start, const FVector& End, const FColor& Color)
{
    FCollisionDebugLine Line;
    Line.Start = Start;
    Line.End   = End;
    Line.Color = Color;
    DebugLines.push_back(Line);
}

void FCollisionSystem::LogCollisionEvent(const char* EventName, const FCollisionEvent& Event) const
{
    const FString SelfActorName = Event.SelfActor ? Event.SelfActor->GetName() : FString("None");
    const FString OtherActorName = Event.OtherActor ? Event.OtherActor->GetName() : FString("None");

    UE_LOG(
        "[Collision] %s | Self=%s(tag=%s) Other=%s(tag=%s) Location=(%.2f, %.2f, %.2f) Normal=(%.2f, %.2f, %.2f) Blocking=%s",
        EventName ? EventName : "Unknown",
        SelfActorName.c_str(),
        Event.SelfTag.c_str(),
        OtherActorName.c_str(),
        Event.OtherTag.c_str(),
        Event.Location.X,
        Event.Location.Y,
        Event.Location.Z,
        Event.Normal.X,
        Event.Normal.Y,
        Event.Normal.Z,
        Event.bBlockingHit ? "true" : "false");
}

