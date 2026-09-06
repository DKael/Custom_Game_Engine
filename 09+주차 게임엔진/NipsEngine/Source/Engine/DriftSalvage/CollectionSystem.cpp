#include "DriftSalvage/CollectionSystem.h"

#include <algorithm>
#include <cmath>

#include <Windows.h>

#include "Component/PrimitiveComponent.h"
#include "Component/ShapeComponent.h"
#include "Core/ActorTags.h"
#include "Core/Logging/Log.h"
#include "Core/SoundManager.h"
#include "Engine/Input/InputSystem.h"
#include "Engine/Math/Utils.h"
#include "Engine/Scripting/LuaBinder.h"
#include "GameFramework/Actor.h"
#include "GameFramework/World.h"

namespace
{
    float ClampF(float V, float Min, float Max)
    {
        return std::max(Min, std::min(V, Max));
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

    void PrepareHazardForCollection(UWorld* World, AActor* Actor)
    {
        if (!Actor || !Actor->CompareTag(ActorTags::Hazard))
        {
            return;
        }

        if (World)
        {
            World->GetExplosionSystem().CancelHazardCollectionInterference(Actor);
        }

        for (UActorComponent* Component : Actor->GetComponents())
        {
            UShapeComponent* Shape = Cast<UShapeComponent>(Component);
            if (!Shape)
            {
                continue;
            }

            Shape->ClearOverlapInfos();
            Shape->SetActive(false);
        }
    }
}

void FCollectionSystem::Tick(UWorld* World, float DeltaTime)
{
    if (!World)
    {
        return;
    }

    if (LuaBinder::IsUIMode())
    {
        bSpaceHeld = false;
        bPrevSpace = false;
        CurrentRadius = 0.0f;
        return;
    }

    InputSystem& Input = InputSystem::Get();
    const bool bSpaceNow = Input.GetKey(VK_SPACE);

    if (bSpaceNow && !bPrevSpace)
    {
        Press();
    }

    if (bSpaceHeld)
    {
        TickRing(World, DeltaTime);
    }

    if (!bSpaceNow && bPrevSpace)
    {
        Release(World);
    }

    if (!Flights.empty())
    {
        TickFlights(DeltaTime);
    }

    bPrevSpace = bSpaceNow;
}

void FCollectionSystem::Reset()
{
    bSpaceHeld = false;
    bPrevSpace = false;
    CurrentRadius = 0.0f;
    Flights.clear();
}

void FCollectionSystem::Press()
{
    bSpaceHeld = true;
    CurrentRadius = MinRadius;
}

void FCollectionSystem::TickRing(UWorld* World, float DeltaTime)
{
    AActor* Boat = FindBoatActor(World);
    if (!Boat)
    {
        return;
    }

    CurrentRadius = ClampF(CurrentRadius + GrowthRate * DeltaTime, MinRadius, MaxRadius);
    RingCenter = Boat->GetActorLocation();
}

void FCollectionSystem::Release(UWorld* World)
{
    bSpaceHeld = false;

    AActor* Boat = FindBoatActor(World);
    if (!Boat)
    {
        CurrentRadius = 0.0f;
        return;
    }

    const FVector CollectionCenter = Boat->GetActorLocation();
    const float CollectionRadius = CurrentRadius > 0.0f ? CurrentRadius : MinRadius;

    struct FCollectionCandidate
    {
        AActor* Actor = nullptr;
        float CargoWeight = 0.0f;
    };

    float ReservedWeight = 0.0f;
    for (const FFlight& Flight : Flights)
    {
        ReservedWeight += Flight.CargoWeight;
    }

    TArray<FCollectionCandidate> ActorsToCollect;
    for (AActor* Actor : World->GetActors())
    {
        if (!Actor || Actor == Boat || Actor->IsPendingDestroy() || !IsCollectibleActor(Actor))
        {
            continue;
        }

        if (IsActorInFlight(Actor))
        {
            continue;
        }

        if (IsActorInsideRadius(Actor, CollectionCenter, CollectionRadius))
        {
            if (!LuaBinder::CanApplyDriftSalvagePickup(Actor->GetTag(), ReservedWeight))
            {
                continue;
            }

            const float CargoWeight = LuaBinder::GetDriftSalvagePickupWeight(Actor->GetTag());
            ActorsToCollect.push_back({ Actor, CargoWeight });
            ReservedWeight += CargoWeight;
        }
    }

    for (const FCollectionCandidate& Candidate : ActorsToCollect)
    {
        AActor* Actor = Candidate.Actor;
        if (!Actor || Actor->IsPendingDestroy())
        {
            continue;
        }

        FFlight Flight;
        Flight.Actor = Actor;
        Flight.Start = Actor->GetActorLocation();
        Flight.End = CollectionCenter;

        const float Distance = FVector::Distance(Flight.Start, Flight.End);
        const float ArcHeight = Distance * FlightArcHeightRatio;
        const FVector Midpoint = (Flight.Start + Flight.End) * 0.5f;
        Flight.Control = Midpoint + FVector::UpVector * ArcHeight;

        Flight.Duration = std::max(FlightMinDuration,
                                   Distance / std::max(FlightSpeed, 0.0001f));
        Flight.Elapsed = 0.0f;
        Flight.CargoWeight = Candidate.CargoWeight;

        PrepareHazardForCollection(World, Actor);
        Flights.push_back(Flight);
    }

    CurrentRadius = 0.0f;
}

void FCollectionSystem::TickFlights(float DeltaTime)
{
    for (size_t i = 0; i < Flights.size();)
    {
        FFlight& Flight = Flights[i];
        AActor* Actor = Flight.Actor;

        if (!Actor || Actor->IsPendingDestroy())
        {
            Flights.erase(Flights.begin() + i);
            continue;
        }

        Flight.Elapsed += DeltaTime;
        const float t = MathUtil::Clamp(Flight.Elapsed / Flight.Duration, 0.0f, 1.0f);

        const float u = 1.0f - t;
        const FVector Position =
            Flight.Start * (u * u) +
            Flight.Control * (2.0f * u * t) +
            Flight.End * (t * t);

        Actor->SetActorLocation(Position);

        if (t >= 1.0f)
        {
            if (LuaBinder::TryApplyDriftSalvagePickup(Actor->GetTag()))
            {
                FSoundManager::Get().PlaySFX("Pick.mp3", 0.3f);
                UE_LOG("[CollectBySpace] name=%s tag=%s", *Actor->GetName(), Actor->GetTag().c_str());
                Actor->Destroy();
            }
            else
            {
                Actor->SetActorLocation(Flight.Start);
            }
            Flights.erase(Flights.begin() + i);
            continue;
        }

        ++i;
    }
}

bool FCollectionSystem::IsActorInFlight(const AActor* Actor) const
{
    for (const FFlight& Flight : Flights)
    {
        if (Flight.Actor == Actor)
        {
            return true;
        }
    }
    return false;
}

AActor* FCollectionSystem::FindBoatActor(UWorld* World) const
{
    if (!World)
    {
        return nullptr;
    }

    for (AActor* Actor : World->GetActors())
    {
        if (Actor && Actor->CompareTag(ActorTags::Boat) && !Actor->IsPendingDestroy())
        {
            return Actor;
        }
    }

    return nullptr;
}

bool FCollectionSystem::IsCollectibleActor(const AActor* Actor) const
{
    if (!Actor)
    {
        return false;
    }

    return Actor->CompareTag(ActorTags::Trash) ||
           Actor->CompareTag(ActorTags::Resource) ||
           Actor->CompareTag(ActorTags::Recyclable) ||
           Actor->CompareTag(ActorTags::Premium) ||
           Actor->CompareTag(ActorTags::Hazard);
}

bool FCollectionSystem::IsActorInsideRadius(const AActor* Actor, const FVector& Center, float Radius) const
{
    if (!Actor || Radius <= 0.0f)
    {
        return false;
    }

    const float RadiusSq = Radius * Radius;

    for (UPrimitiveComponent* Primitive : Actor->GetPrimitiveComponents())
    {
        if (!Primitive || !Primitive->IsActive())
        {
            continue;
        }

        const FAABB& Bounds = Primitive->GetWorldAABB();
        if (Bounds.IsValid() && PointAABBDistanceSq(Center, Bounds) <= RadiusSq)
        {
            return true;
        }
    }

    return FVector::DistSquared(Center, Actor->GetActorLocation()) <= RadiusSq;
}
