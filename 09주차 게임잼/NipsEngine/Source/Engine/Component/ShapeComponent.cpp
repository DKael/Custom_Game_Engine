#include "ShapeComponent.h"

#include <algorithm>
#include <cmath>

#include "Asset/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "StaticMeshComponent.h"

DEFINE_CLASS(UShapeComponent, UPrimitiveComponent)

DEFINE_CLASS(UBoxComponent, UShapeComponent)
REGISTER_FACTORY(UBoxComponent)

DEFINE_CLASS(USphereComponent, UShapeComponent)
REGISTER_FACTORY(USphereComponent)

DEFINE_CLASS(UCapsuleComponent, UShapeComponent)
REGISTER_FACTORY(UCapsuleComponent)

namespace
{
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

    float CapsuleSegmentHalfHeight(float HalfHeight, float Radius)
    {
        return std::max(0.0f, HalfHeight - Radius);
    }

    bool IsNearlyEqual(float A, float B, float Tolerance = 1.e-4f)
    {
        return std::fabs(A - B) <= Tolerance;
    }

    float ClampExtent(float Value)
    {
        return std::max(0.0f, Value);
    }

    float ComputeMeshBoundingSphereRadius(const UStaticMesh* StaticMesh, const FVector& Center, const FVector& FallbackExtent)
    {
        if (StaticMesh == nullptr || StaticMesh->GetVertices().empty())
        {
            return std::max({ FallbackExtent.X, FallbackExtent.Y, FallbackExtent.Z });
        }

        float RadiusSquared = 0.0f;
        for (const FNormalVertex& Vertex : StaticMesh->GetVertices())
        {
            RadiusSquared = std::max(RadiusSquared, (Vertex.Position - Center).SizeSquared());
        }

        return std::sqrt(RadiusSquared);
    }

    float ComputeCapsuleRadiusForAxis(const UStaticMesh* StaticMesh, const FVector& Center, const FVector& FallbackExtent, int32 Axis)
    {
        if (StaticMesh == nullptr || StaticMesh->GetVertices().empty())
        {
            if (Axis == 0)
            {
                return std::max(FallbackExtent.Y, FallbackExtent.Z);
            }
            if (Axis == 1)
            {
                return std::max(FallbackExtent.X, FallbackExtent.Z);
            }
            return std::max(FallbackExtent.X, FallbackExtent.Y);
        }

        float RadiusSquared = 0.0f;
        for (const FNormalVertex& Vertex : StaticMesh->GetVertices())
        {
            const FVector Delta = Vertex.Position - Center;
            if (Axis == 0)
            {
                RadiusSquared = std::max(RadiusSquared, Delta.Y * Delta.Y + Delta.Z * Delta.Z);
            }
            else if (Axis == 1)
            {
                RadiusSquared = std::max(RadiusSquared, Delta.X * Delta.X + Delta.Z * Delta.Z);
            }
            else
            {
                RadiusSquared = std::max(RadiusSquared, Delta.X * Delta.X + Delta.Y * Delta.Y);
            }
        }

        return std::sqrt(RadiusSquared);
    }
}

void UShapeComponent::Serialize(FArchive& Ar)
{
    UPrimitiveComponent::Serialize(Ar);

    Ar << "GenerateOverlapEvents" << bGenerateOverlapEvents;
    Ar << "BlockComponent" << bBlockComponent;
    Ar << "Movable" << bMovable;
    Ar << "ShapeColor" << ShapeColor;
}

void UShapeComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    UPrimitiveComponent::GetEditableProperties(OutProps);

    OutProps.push_back({ "Generate Overlap Events", EPropertyType::Bool, &bGenerateOverlapEvents });
    OutProps.push_back({ "Block Component", EPropertyType::Bool, &bBlockComponent });
    OutProps.push_back({ "Movable", EPropertyType::Bool, &bMovable });
    OutProps.push_back({ "Shape Color", EPropertyType::Color, &ShapeColor });
}

void UShapeComponent::PostEditProperty(const char* PropertyName)
{
    UPrimitiveComponent::PostEditProperty(PropertyName);
    NotifySpatialIndexDirty();
}

bool UShapeComponent::FitToStaticMesh(UStaticMeshComponent* StaticMeshComponent)
{
    if (StaticMeshComponent == nullptr || !StaticMeshComponent->HasValidMesh())
    {
        return false;
    }

    UStaticMesh* StaticMesh = StaticMeshComponent->GetStaticMesh();
    if (StaticMesh == nullptr)
    {
        return false;
    }

    const FAABB& LocalBounds = StaticMesh->GetLocalBounds();
    if (!LocalBounds.IsValid())
    {
        return false;
    }

    const FVector Center = LocalBounds.GetCenter();
    const FVector Extent(
        ClampExtent(LocalBounds.GetExtent().X),
        ClampExtent(LocalBounds.GetExtent().Y),
        ClampExtent(LocalBounds.GetExtent().Z));

    SetRelativeLocation(Center);
    SetRelativeScale(FVector::OneVector);

    if (UBoxComponent* Box = Cast<UBoxComponent>(this))
    {
        SetRelativeRotation(FVector::ZeroVector);
        Box->SetBoxExtent(Extent);
        return true;
    }

    if (USphereComponent* Sphere = Cast<USphereComponent>(this))
    {
        SetRelativeRotation(FVector::ZeroVector);
        Sphere->SetSphereRadius(ComputeMeshBoundingSphereRadius(StaticMesh, Center, Extent));
        return true;
    }

    if (UCapsuleComponent* Capsule = Cast<UCapsuleComponent>(this))
    {
        int32 HeightAxis = 2;
        float HalfHeight = Extent.Z;
        FVector CapsuleRotation = FVector::ZeroVector;

        if (Extent.X >= Extent.Y && Extent.X >= Extent.Z)
        {
            HeightAxis = 0;
            HalfHeight = Extent.X;
            CapsuleRotation = FVector(0.0f, 90.0f, 0.0f);
        }
        else if (Extent.Y >= Extent.X && Extent.Y >= Extent.Z)
        {
            HeightAxis = 1;
            HalfHeight = Extent.Y;
            CapsuleRotation = FVector(-90.0f, 0.0f, 0.0f);
        }

        const float Radius = ComputeCapsuleRadiusForAxis(StaticMesh, Center, Extent, HeightAxis);
        Capsule->SetRelativeRotation(CapsuleRotation);
        Capsule->SetCapsuleSize(Radius, std::max(HalfHeight, Radius));
        return true;
    }

    return false;
}

bool UShapeComponent::RaycastMesh(const FRay& Ray, FHitResult& OutHitResult)
{
    float HitT = 0.0f;
    if (!GetWorldAABB().IntersectRay(Ray, HitT))
    {
        OutHitResult.Reset();
        return false;
    }

    OutHitResult.bHit = true;
    OutHitResult.HitComponent = this;
    OutHitResult.Distance = HitT;
    OutHitResult.Location = Ray.Origin + Ray.Direction * HitT;
    OutHitResult.Normal = (OutHitResult.Location - GetWorldLocation()).GetSafeNormal();
    OutHitResult.FaceIndex = 0;
    return true;
}

bool UShapeComponent::IsOverlappingActor(const AActor* Other) const
{
    if (!Other)
    {
        return false;
    }
    
    for (const FOverlapInfo& Info : OverlapInfos)
    {
        if (Info.OverlapActor == Other)
        {
            return true;
        }
    }
    
    return false;
}

void UShapeComponent::AddOverlap(UPrimitiveComponent* OtherComponent)
{
    if (!OtherComponent)
    {
        return;
    }
    
    AActor* OtherActor = OtherComponent->GetOwner();
    if (!OtherActor)
    {
        return;
    }
    for (const FOverlapInfo& Info : OverlapInfos)
    {
        if (Info.OverlapComponent == OtherComponent)
        {
            return;
        }
    }
    
    FOverlapInfo NewInfo;
    NewInfo.OverlapComponent = OtherComponent;
    NewInfo.OverlapActor = OtherActor;
    OverlapInfos.push_back(NewInfo);
}

void UShapeComponent::RemoveOverlap(UPrimitiveComponent* OtherComponent)
{
    if (!OtherComponent)
    {
        return;
    }

    for (auto It = OverlapInfos.begin(); It != OverlapInfos.end(); ++It)
    {
        if (It->OverlapComponent == OtherComponent)
        {
            OverlapInfos.erase(It);
            return;
        }
    }
}

void UShapeComponent::ClearOverlapInfos()
{
    OverlapInfos.clear();
}

void UShapeComponent::GetOverlappingActors(TArray<AActor*>& OutActors, const FString& TagFilter) const
{
    OutActors.clear();

    // 중복 액터 방지용 (같은 액터의 여러 컴포넌트가 겹친 경우)
    for (const FOverlapInfo& Info : OverlapInfos)
    {
        if (!Info.OverlapActor)
        {
            continue;
        }

        if (!TagFilter.empty() && Info.OverlapActor->GetTag() != TagFilter)
        {
            continue;
        }

        // 이미 들어있는지 확인
        bool bAlready = false;
        for (AActor* Existing : OutActors)
        {
            if (Existing == Info.OverlapActor)
            {
                bAlready = true;
                break;
            }
        }

        if (!bAlready)
        {
            OutActors.push_back(Info.OverlapActor);
        }
    }
}

void UShapeComponent::DispatchBeginOverlap(const FCollisionEvent& Event)
{
    OnComponentBeginOverlap.Broadcast(Event);
}

void UShapeComponent::DispatchEndOverlap(const FCollisionEvent& Event)
{
    OnComponentEndOverlap.Broadcast(Event);
}

void UShapeComponent::DispatchHit(const FCollisionEvent& Event)
{
    OnComponentHit.Broadcast(Event);
}

// --- Box ---
void UBoxComponent::Serialize(FArchive& Ar)
{
    UShapeComponent::Serialize(Ar);
    Ar << "BoxExtent" << BoxExtent;

    if (Ar.IsLoading() &&
        IsNearlyEqual(BoxExtent.X, 50.0f) &&
        IsNearlyEqual(BoxExtent.Y, 50.0f) &&
        IsNearlyEqual(BoxExtent.Z, 50.0f))
    {
        BoxExtent = FVector(1.0f, 1.0f, 1.0f);
    }
}

void UBoxComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    UShapeComponent::GetEditableProperties(OutProps);
    OutProps.push_back({ "Box Extent", EPropertyType::Vec3, &BoxExtent });
}

void UBoxComponent::PostEditProperty(const char* PropertyName)
{
    UShapeComponent::PostEditProperty(PropertyName);
    NotifySpatialIndexDirty();
}

void UBoxComponent::UpdateWorldAABB() const
{
    const FAABB LocalAABB(-BoxExtent, BoxExtent);
    WorldAABB = FAABB::TransformAABB(LocalAABB, GetWorldMatrix());
}

// --- Sphere ---
void USphereComponent::Serialize(FArchive& Ar)
{
    UShapeComponent::Serialize(Ar);
    Ar << "SphereRadius" << SphereRadius;
    Ar << "MinRadius"    << MinRadius;
    Ar << "MaxRadius"    << MaxRadius;
    Ar << "GrowthRate"   << GrowthRate;

    if (Ar.IsLoading() && IsNearlyEqual(SphereRadius, 50.0f))
    {
        SphereRadius = 1.0f;
    }
}

void USphereComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    UShapeComponent::GetEditableProperties(OutProps);

    FPropertyDescriptor RadiusProp;
    RadiusProp.Name = "Sphere Radius";
    RadiusProp.Type = EPropertyType::Float;
    RadiusProp.ValuePtr = &SphereRadius;
    RadiusProp.Min = 0.0f;
    RadiusProp.Max = 100000.0f;
    RadiusProp.Speed = 1.0f;
    OutProps.push_back(RadiusProp);

    // 회수 범위 동적 확장 파라미터 (기획팀이 에디터에서 조절)
    OutProps.push_back({ "Min Radius",   EPropertyType::Float, &MinRadius,   0.0f, 100000.0f, 0.1f });
    OutProps.push_back({ "Max Radius",   EPropertyType::Float, &MaxRadius,   0.0f, 100000.0f, 0.1f });
    OutProps.push_back({ "Growth Rate",  EPropertyType::Float, &GrowthRate,  0.0f, 1000.0f,   0.1f });
}

void USphereComponent::PostEditProperty(const char* PropertyName)
{
    UShapeComponent::PostEditProperty(PropertyName);

    if (SphereRadius < 0.0f)
    {
        SphereRadius = 0.0f;
    }

    // Min/Max 일관성 보정
    if (MinRadius < 0.0f) MinRadius = 0.0f;
    if (MaxRadius < MinRadius) MaxRadius = MinRadius;

    NotifySpatialIndexDirty();
}

float USphereComponent::GrowRadius(float DeltaTime)
{
    // 새 반경 계산. SetSphereRadius가 자동으로 Min/Max에 클램프.
    SetSphereRadius(SphereRadius + GrowthRate * DeltaTime);
    return SphereRadius;
}

void USphereComponent::GetActorsInRadius(TArray<AActor*>& OutActors, const FString& TagFilter) const
{
    // 부모 헬퍼를 그대로 사용 — Sphere의 OverlapInfos는 CollisionSystem이 매 Tick 갱신
    GetOverlappingActors(OutActors, TagFilter);
}

void USphereComponent::UpdateWorldAABB() const
{
    const FVector Center = GetWorldLocation();
    const float WorldRadius = SphereRadius * MaxAbs3(GetWorldAxisScale());
    const FVector Extent(WorldRadius, WorldRadius, WorldRadius);
    
    WorldAABB = FAABB(Center - Extent, Center + Extent);
}

// --- Capsule ---
void UCapsuleComponent::Serialize(FArchive& Ar)
{
    UShapeComponent::Serialize(Ar);
    Ar << "CapsuleHalfHeight" << CapsuleHalfHeight;
    Ar << "CapsuleRadius" << CapsuleRadius;

    if (Ar.IsLoading() &&
        IsNearlyEqual(CapsuleHalfHeight, 20.0f) &&
        IsNearlyEqual(CapsuleRadius, 5.0f))
    {
        CapsuleHalfHeight = 1.0f;
        CapsuleRadius = 1.0f;
    }
}

void UCapsuleComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    UShapeComponent::GetEditableProperties(OutProps);

    FPropertyDescriptor HalfHeightProp;
    HalfHeightProp.Name = "Capsule Half Height";
    HalfHeightProp.Type = EPropertyType::Float;
    HalfHeightProp.ValuePtr = &CapsuleHalfHeight;
    HalfHeightProp.Min = 0.0f;
    HalfHeightProp.Max = 100000.0f;
    HalfHeightProp.Speed = 1.0f;
    OutProps.push_back(HalfHeightProp);

    FPropertyDescriptor RadiusProp;
    RadiusProp.Name = "Capsule Radius";
    RadiusProp.Type = EPropertyType::Float;
    RadiusProp.ValuePtr = &CapsuleRadius;
    RadiusProp.Min = 0.0f;
    RadiusProp.Max = 100000.0f;
    RadiusProp.Speed = 1.0f;
    OutProps.push_back(RadiusProp);
}

void UCapsuleComponent::PostEditProperty(const char* PropertyName)
{
    UShapeComponent::PostEditProperty(PropertyName);

    if (CapsuleRadius < 0.0f)
    {
        CapsuleRadius = 0.0f;
    }

    if (CapsuleHalfHeight < CapsuleRadius)
    {
        CapsuleHalfHeight = CapsuleRadius;
    }

    NotifySpatialIndexDirty();
}

void UCapsuleComponent::UpdateWorldAABB() const
{
    const FVector WorldScale = GetWorldAxisScale();
    const float WorldRadius = CapsuleRadius * CapsuleRadiusScale(WorldScale);
    const float WorldHalfHeight = std::max(CapsuleHalfHeight * CapsuleHeightScale(WorldScale), WorldRadius);
    const float SegmentHalfHeight = CapsuleSegmentHalfHeight(WorldHalfHeight, WorldRadius);
    const FVector Center = GetWorldLocation();
    const FVector Up = GetUpVector();
    const FVector SegmentStart = Center - Up * SegmentHalfHeight;
    const FVector SegmentEnd = Center + Up * SegmentHalfHeight;
    const FVector RadiusExtent(WorldRadius, WorldRadius, WorldRadius);

    WorldAABB = FAABB(
        FVector(
            std::min(SegmentStart.X, SegmentEnd.X),
            std::min(SegmentStart.Y, SegmentEnd.Y),
            std::min(SegmentStart.Z, SegmentEnd.Z)) - RadiusExtent,
        FVector(
            std::max(SegmentStart.X, SegmentEnd.X),
            std::max(SegmentStart.Y, SegmentEnd.Y),
            std::max(SegmentStart.Z, SegmentEnd.Z)) + RadiusExtent);
}
