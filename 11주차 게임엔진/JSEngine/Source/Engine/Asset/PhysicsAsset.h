#pragma once

#include "Core/CoreMinimal.h"
#include "Engine/Geometry/Transform.h"
#include "Object/FName.h"
#include "Object/Object.h"
#include "Generated/PhysicsAsset.generated.h"

class USkeleton;

enum class EPhysicsBodyShape : uint8
{
    Box = 0,
    Sphere,
    Capsule
};

struct FPhysicsBody
{
    FName Name;
    int32 BoneIndex = -1;

    EPhysicsBodyShape Shape = EPhysicsBodyShape::Capsule;
    FTransform LocalTransform = FTransform::Identity;

    FVector BoxExtent = FVector::ZeroVector;
    float SphereRadius = 0.0f;
    float CapsuleRadius = 0.0f;
    float CapsuleHalfHeight = 0.0f;
};

struct FPhysicsConstraint
{
    FName Name;
    int32 ParentBodyIndex = -1;
    int32 ChildBodyIndex = -1;

    FTransform ParentLocalFrame = FTransform::Identity;
    FTransform ChildLocalFrame = FTransform::Identity;
};

struct FPhysicsAssetData
{
    FString PathFileName;
    FString SkeletonAssetPath;

    TArray<FPhysicsBody> Bodies;
    TArray<FPhysicsConstraint> Constraints;

    void Clear();
    bool HasValidBodyData() const;
};

UCLASS()
class UPhysicsAsset : public UObject
{
public:
    GENERATED_BODY()

    UPhysicsAsset() = default;
    ~UPhysicsAsset() override;

    void SetPhysicsAssetData(FPhysicsAssetData* InPhysicsAssetData);

    FPhysicsAssetData* GetPhysicsAssetData();
    const FPhysicsAssetData* GetPhysicsAssetData() const;

    void SetSkeleton(USkeleton* InSkeleton);
    USkeleton* GetSkeleton() const;

    const FString& GetAssetPathFileName() const;
    const FString& GetSkeletonAssetPath() const;

    const TArray<FPhysicsBody>& GetBodies() const;
    const TArray<FPhysicsConstraint>& GetConstraints() const;

    const FPhysicsBody* GetBody(int32 BodyIndex) const;
    int32 FindBodyIndex(const FName& Name) const;
    const FPhysicsBody* FindBody(const FName& Name) const;

    bool HasValidPhysicsData() const;

private:
    FPhysicsAssetData* PhysicsAssetData = nullptr;
    USkeleton* Skeleton = nullptr;
};
