#pragma once

#include "Core/Containers/Array.h"
#include "Math/Color.h"
#include "Math/Vector.h"

class UWorld;
class AActor;

class FCollectionSystem
{
public:
    void Tick(UWorld* World, float DeltaTime);
    void Reset();

    bool IsRingVisible() const { return bSpaceHeld && CurrentRadius > 0.0f; }
    const FVector& GetRingCenter() const { return RingCenter; }
    float GetRingRadius() const { return CurrentRadius; }
    const FColor& GetRingColor() const { return RingColor; }

private:
    void Press();
    void Release(UWorld* World);

    void TickRing(UWorld* World, float DeltaTime);
    void TickFlights(float DeltaTime);

    bool IsActorInFlight(const AActor* Actor) const;
    AActor* FindBoatActor(UWorld* World) const;
    bool IsCollectibleActor(const AActor* Actor) const;
    bool IsActorInsideRadius(const AActor* Actor, const FVector& Center, float Radius) const;

    struct FFlight
    {
        AActor* Actor = nullptr;
        FVector Start = FVector::ZeroVector;
        FVector End = FVector::ZeroVector;
        FVector Control = FVector::ZeroVector;
        float Elapsed = 0.0f;
        float Duration = 0.0f;
        float CargoWeight = 0.0f;
    };

    bool bSpaceHeld = false;
    bool bPrevSpace = false;

    FVector RingCenter = FVector::ZeroVector;
    FColor RingColor = FColor(102, 255, 77);

    float CurrentRadius = 0.0f;
    float MinRadius = 1.0f;
    float MaxRadius = 10.0f;
    float GrowthRate = 4.0f;

    TArray<FFlight> Flights;
    float FlightSpeed = 20.0f;
    float FlightArcHeightRatio = 0.25f;
    float FlightMinDuration = 0.15f;
};
