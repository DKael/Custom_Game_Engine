#pragma once
#include <unordered_set>

#include "Component/ShapeComponent.h"
#include "Spatial/WorldSpatialIndex.h"

class UWorld;

struct FCollisionPair
{
    UShapeComponent* A = nullptr;
    UShapeComponent* B = nullptr;

    FCollisionPair() = default;

    FCollisionPair(UShapeComponent* InA, UShapeComponent* InB)
    {
        if (InA < InB)
        {
            A = InA;
            B = InB;
        }
        else
        {
            A = InB;
            B = InA;
        }
    }

    bool operator==(const FCollisionPair& Other) const
    {
        return A == Other.A && B == Other.B;
    }
};

struct FCollisionPairHash
{
    size_t operator()(const FCollisionPair& Pair) const
    {
        const size_t A = reinterpret_cast<size_t>(Pair.A);
        const size_t B = reinterpret_cast<size_t>(Pair.B);
        return (A >> 4) ^ (B << 4);
    }
};

struct FCollisionDebugContact
{
    FVector Location = FVector::ZeroVector;
    FVector Normal = FVector::ZeroVector;
    FAABB OverlapBounds;
    bool bHasOverlapBounds = false;

    UShapeComponent* A = nullptr;
    UShapeComponent* B = nullptr;
};

// LineTrace 결과 시각화용 — 한 프레임 동안만 유효.
// CollisionSystem::Tick 시작 시 매 프레임 비워진다.
struct FCollisionDebugLine
{
    FVector Start = FVector::ZeroVector;
    FVector End = FVector::ZeroVector;
    FColor Color = FColor::White();
};

class FCollisionSystem
{
public:
    void Tick(UWorld* World, float DeltaTime);
    void Reset();

    const TArray<FCollisionDebugContact>& GetDebugContacts() const
    {
        return DebugContacts;
    }

    const TArray<FCollisionDebugLine>& GetDebugLines() const
    {
        return DebugLines;
    }

    // --- LineTrace API (Drift Salvage 마우스 밀치기용) ---
    //
    // World 안의 모든 PrimitiveComponent를 검사해 가장 가까운 Hit을 OutHit에 채운다.
    // Block/Overlap 구분 없이 가시적인 모든 PrimitiveComponent를 검사한다 — 마우스 밀치기는
    // Trash/Hazard/Rock 등 Tag에 무관하게 클릭한 대상을 알아내야 하기 때문.
    //
    // @param Start, End  광선의 시작점/끝점 (월드 좌표)
    // @param OutHit      가장 가까운 Hit 정보가 채워진다. 미스 시 OutHit.bHit == false
    // @param IgnoreTag   해당 Tag의 액터는 검사에서 제외 (보통 자기 자신 = "Boat")
    // @param bDrawDebug  true면 1프레임 디버그 라인 추가 (Hit이면 빨강, 미스면 초록)
    // @return            Hit 발생 여부
    bool LineTraceSingle(
        UWorld* World,
        const FVector& Start,
        const FVector& End,
        FHitResult& OutHit,
        const FString& IgnoreTag = "",
        bool bDrawDebug = false);

    // 외부에서 디버그 라인을 한 프레임 동안 그리고 싶을 때 사용.
    // (예: 게임 로직이 자기 진행 방향을 시각화하는 용도)
    void AddDebugLine(const FVector& Start, const FVector& End, const FColor& Color);

private:
    void CollectShapeComponents(UWorld* World, TArray<UShapeComponent*>& OutShapes);
    
    bool ShouldTestPair(const UShapeComponent* A, const UShapeComponent* B) const;
    bool AreOverlapping(UShapeComponent* A, UShapeComponent* B) const;

    bool AABBOverlap(UShapeComponent* A, UShapeComponent* B) const;
    bool SphereSphere(UShapeComponent* A, UShapeComponent* B) const;
    bool SphereBox(UShapeComponent* Sphere, UShapeComponent* Box) const;
    bool BoxBox(UShapeComponent* A, UShapeComponent* B) const;
    bool CapsuleCapsule(UShapeComponent* A, UShapeComponent* B) const;
    bool SphereCapsule(UShapeComponent* Sphere, UShapeComponent* Capsule) const;
    bool BoxCapsule(UShapeComponent* Box, UShapeComponent* Capsule) const;

    void HandleBeginOverlap(UShapeComponent* A, UShapeComponent* B);
    void HandleEndOverlap(UShapeComponent* A, UShapeComponent* B);
    void HandleHit(UShapeComponent* A, UShapeComponent* B);
    void ResolveBlockingOverlap(UShapeComponent* A, UShapeComponent* B);
    
    void AddDebugContact(UShapeComponent* A, UShapeComponent* B);
    FVector ComputeDebugContactLocation(UShapeComponent* A, UShapeComponent* B) const;
    bool ComputeDebugOverlapBounds(UShapeComponent* A, UShapeComponent* B, FAABB& OutBounds) const;
    FCollisionEvent MakeCollisionEvent(UShapeComponent* Self, UShapeComponent* Other, bool bBlockingHit) const;
    void LogCollisionEvent(const char* EventName, const FCollisionEvent& Event) const;
    
private:
    bool bHasInitializedOverlaps = false;
    TSet<FCollisionPair, FCollisionPairHash> PreviousOverlaps;
    TArray<FCollisionDebugContact> DebugContacts;
    TArray<FCollisionDebugLine>    DebugLines;  // 매 Tick 시작 시 클리어

    TArray<UPrimitiveComponent*> CollisionCandidatePrimitives;
    FWorldSpatialIndex::FPrimitiveSphereQueryScratch CollisionSphereQueryScratch;
};
