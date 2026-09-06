#pragma once
#include <functional>

#include "Core/Containers/String.h"
#include "PrimitiveComponent.h"
#include "Engine/Core/Delegate/DelegateMacros.h"

class AActor;
class UShapeComponent;
class UStaticMeshComponent;

struct FOverlapInfo
{
    UPrimitiveComponent* OverlapComponent = nullptr;
    AActor* OverlapActor = nullptr;
    
    bool IsValid() const
    {
        return OverlapComponent != nullptr && OverlapActor != nullptr;
    }
};

struct FCollisionEvent
{
    UShapeComponent* SelfComponent = nullptr;
    AActor* SelfActor = nullptr;
    UShapeComponent* OtherComponent = nullptr;
    AActor* OtherActor = nullptr;

    FString SelfTag = "Untagged";
    FString OtherTag = "Untagged";

    FVector Location = FVector::ZeroVector;
    FVector Normal = FVector::ZeroVector;
    FAABB OverlapBounds;
    bool bHasOverlapBounds = false;
    bool bBlockingHit = false;

    bool SelfHasTag(const FString& Tag) const
    {
        return SelfTag == Tag;
    }

    bool OtherHasTag(const FString& Tag) const
    {
        return OtherTag == Tag;
    }
};

using FCollisionEventCallback = std::function<void(const FCollisionEvent&)>;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnHitDelegate, const FCollisionEvent&);
class UShapeComponent : public UPrimitiveComponent
{
public:
    DECLARE_CLASS(UShapeComponent, UPrimitiveComponent);
    
    UShapeComponent() = default;
    
    void Serialize(FArchive& Ar) override;
    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
    void PostEditProperty(const char* PropertyName) override;

    bool FitToStaticMesh(UStaticMeshComponent* StaticMeshComponent);

    EPrimitiveType GetPrimitiveType() const override
    {
        return EPrimitiveType::EPT_CollisionShape;
    }
    
    bool RaycastMesh(const FRay& Ray, FHitResult& OutHitResult) override;
    
    bool GetGenerateOverlapEvents() const {return bGenerateOverlapEvents;}
    void SetGenerateOverlapEvents(bool bInGenerate) {bGenerateOverlapEvents = bInGenerate;}

    bool GetBlockComponent() const {return bBlockComponent;}
    void SetBlockComponent(bool bInBlock) {bBlockComponent = bInBlock;}

    // Block-vs-Block 충돌 해소 시 자기가 밀려도 되는지 여부.
    // true (기본 false). MovementComponent가 없어도 이 플래그가 true면 Resolve에서 push 처리.
    // Drift Salvage의 Boat는 true, Rock은 false.
    bool GetMovable() const { return bMovable; }
    void SetMovable(bool bInMovable) { bMovable = bInMovable; }

    const FColor& GetShapeColor() const { return ShapeColor; }
    void SetShapeColor(const FColor& InColor) { ShapeColor = InColor; }

    bool GetRuntimeDebugVisible() const { return bRuntimeDebugVisible; }
    void SetRuntimeDebugVisible(bool bInVisible) { bRuntimeDebugVisible = bInVisible; }
    const FColor& GetRuntimeDebugColor() const { return RuntimeDebugColor; }
    void SetRuntimeDebugColor(const FColor& InColor) { RuntimeDebugColor = InColor; }
    
    const TArray<FOverlapInfo>& GetOverlapInfos() const { return OverlapInfos; } 
    
    bool IsOverlappingActor(const AActor* Actor) const;
    void AddOverlap(UPrimitiveComponent* OverlapComponent);
    void RemoveOverlap(UPrimitiveComponent* OverlapComponent);
    void ClearOverlapInfos();

    // 현재 이 컴포넌트와 overlap 중인 모든 액터를 OutActors에 채운다.
    // TagFilter가 비어있지 않으면 해당 Tag와 일치하는 액터만 반환한다.
    // 예) Boat가 Space를 떼는 순간 회수 가능 액터를 한 번에 가져오기 위함.
    void GetOverlappingActors(TArray<AActor*>& OutActors, const FString& TagFilter = "") const;

    void DispatchBeginOverlap(const FCollisionEvent& Event);
    void DispatchEndOverlap(const FCollisionEvent& Event);
    void DispatchHit(const FCollisionEvent& Event);

    FOnHitDelegate OnHit;
    FOnHitDelegate OnComponentBeginOverlap;
    FOnHitDelegate OnComponentEndOverlap;
    FOnHitDelegate OnComponentHit;
    
protected:
    bool bGenerateOverlapEvents = true;
    bool bBlockComponent = false;
    bool bMovable = false;

    FColor ShapeColor = FColor::Green();
    bool bRuntimeDebugVisible = false;
    FColor RuntimeDebugColor = FColor(0, 220, 255);

    TArray<FOverlapInfo> OverlapInfos;
};

class UBoxComponent : public UShapeComponent
{
public:;
    DECLARE_CLASS(UBoxComponent, UShapeComponent);
 
    UBoxComponent() = default;
    
    void Serialize(FArchive& Ar) override;
    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
    void PostEditProperty(const char* PropertyName) override;
    
    void UpdateWorldAABB() const override;
    
    const FVector& GetBoxExtent() const {return BoxExtent;}
    void SetBoxExtent(const FVector& InExtent)
    {
        BoxExtent = InExtent;
        NotifySpatialIndexDirty();
    }
    
private:
    FVector BoxExtent = FVector{1.0f, 1.0f, 1.0f};
};

class USphereComponent : public UShapeComponent
{
public:;
    DECLARE_CLASS(USphereComponent, UShapeComponent);

    void Serialize(FArchive& Ar) override;
    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
    void PostEditProperty(const char* PropertyName) override;

    void UpdateWorldAABB() const override;

    float GetSphereRadius() const { return SphereRadius; }
    void SetSphereRadius(float InRadius)
    {
        // MinRadius/MaxRadius 안에 클램프. 게임 코드가 매 프레임 호출해도 안전.
        if (InRadius < MinRadius) InRadius = MinRadius;
        if (InRadius > MaxRadius) InRadius = MaxRadius;
        SphereRadius = InRadius;
        NotifySpatialIndexDirty();
    }

    // --- Drift Salvage 회수 범위 시스템 ---
    // Boat의 Space 키를 누르고 있는 동안 GrowthRate 만큼 Radius가 커지고
    // 키를 떼는 순간 GetActorsInRadius로 회수 가능 액터를 모은 뒤 MinRadius로 복원.
    float GetMinRadius() const { return MinRadius; }
    float GetMaxRadius() const { return MaxRadius; }
    float GetGrowthRate() const { return GrowthRate; }

    void SetMinRadius(float InMin) { MinRadius = InMin; }
    void SetMaxRadius(float InMax) { MaxRadius = InMax; }
    void SetGrowthRate(float InRate) { GrowthRate = InRate; }

    // SphereRadius += GrowthRate * DeltaTime, MaxRadius 넘지 않도록 클램프.
    // 보트 컨트롤러가 Space 키 누르고 있을 때 매 프레임 호출.
    // 반환: 클램프 후의 새 반경.
    float GrowRadius(float DeltaTime);

    // Radius를 MinRadius로 복원. 보트 컨트롤러가 Space를 뗀 직후 호출.
    void ResetRadius() { SetSphereRadius(MinRadius); }

    // 현재 Sphere 영역과 Overlap 중인 모든 액터를 수집.
    // CollisionSystem이 매 Tick에 OverlapInfos를 갱신하므로 여기서는 그것을 그대로 활용.
    // TagFilter 비면 모두, 채워주면 그 Tag만.
    void GetActorsInRadius(TArray<AActor*>& OutActors, const FString& TagFilter = "") const;

private:
    float SphereRadius = 1.0f;

    // 회수 범위 동적 변경용 파라미터 (기획서 기준 기본값)
    float MinRadius   = 1.0f;
    float MaxRadius   = 10.0f;
    float GrowthRate  = 4.0f;  // units / second
};

class UCapsuleComponent : public UShapeComponent
{
public:;
    DECLARE_CLASS(UCapsuleComponent, UShapeComponent);
    
    UCapsuleComponent() = default;
    
    void Serialize(FArchive& Ar) override;
    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
    void PostEditProperty(const char* PropertyName) override;

    void UpdateWorldAABB() const override;
    
    float GetCapsuleHalfHeight() const { return CapsuleHalfHeight; }
    float GetCapsuleRadius() const { return CapsuleRadius; }

    void SetCapsuleSize(float InRadius, float InHalfHeight)
    {
        CapsuleRadius = InRadius;
        CapsuleHalfHeight = InHalfHeight;
        NotifySpatialIndexDirty();
    }
    
private:
    float CapsuleHalfHeight = 0.25f;
    float CapsuleRadius = 0.125f;
};
