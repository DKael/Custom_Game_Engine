#include "PhysicsAsset.h"

void FPhysicsAssetData::Clear()
{
    PathFileName.clear();
    SkeletonAssetPath.clear();
    Bodies.clear();
    Constraints.clear();
}

bool FPhysicsAssetData::HasValidBodyData() const
{
    return !Bodies.empty();
}

UPhysicsAsset::~UPhysicsAsset()
{
    delete PhysicsAssetData;
    PhysicsAssetData = nullptr;
    Skeleton = nullptr;
}

void UPhysicsAsset::SetPhysicsAssetData(FPhysicsAssetData* InPhysicsAssetData)
{
    if (PhysicsAssetData == InPhysicsAssetData)
    {
        return;
    }

    delete PhysicsAssetData;
    PhysicsAssetData = InPhysicsAssetData;
}

FPhysicsAssetData* UPhysicsAsset::GetPhysicsAssetData()
{
    return PhysicsAssetData;
}

const FPhysicsAssetData* UPhysicsAsset::GetPhysicsAssetData() const
{
    return PhysicsAssetData;
}

void UPhysicsAsset::SetSkeleton(USkeleton* InSkeleton)
{
    Skeleton = InSkeleton;
}

USkeleton* UPhysicsAsset::GetSkeleton() const
{
    return Skeleton;
}

const FString& UPhysicsAsset::GetAssetPathFileName() const
{
    static const FString Empty = {};
    return PhysicsAssetData ? PhysicsAssetData->PathFileName : Empty;
}

const FString& UPhysicsAsset::GetSkeletonAssetPath() const
{
    static const FString Empty = {};
    return PhysicsAssetData ? PhysicsAssetData->SkeletonAssetPath : Empty;
}

const TArray<FPhysicsBody>& UPhysicsAsset::GetBodies() const
{
    static const TArray<FPhysicsBody> Empty = {};
    return PhysicsAssetData ? PhysicsAssetData->Bodies : Empty;
}

const TArray<FPhysicsConstraint>& UPhysicsAsset::GetConstraints() const
{
    static const TArray<FPhysicsConstraint> Empty = {};
    return PhysicsAssetData ? PhysicsAssetData->Constraints : Empty;
}

const FPhysicsBody* UPhysicsAsset::GetBody(int32 BodyIndex) const
{
    if (!PhysicsAssetData || BodyIndex < 0 || BodyIndex >= static_cast<int32>(PhysicsAssetData->Bodies.size()))
    {
        return nullptr;
    }

    return &PhysicsAssetData->Bodies[BodyIndex];
}

int32 UPhysicsAsset::FindBodyIndex(const FName& Name) const
{
    if (!PhysicsAssetData || !Name.IsValid())
    {
        return -1;
    }

    for (int32 BodyIndex = 0; BodyIndex < static_cast<int32>(PhysicsAssetData->Bodies.size()); ++BodyIndex)
    {
        if (PhysicsAssetData->Bodies[BodyIndex].Name == Name)
        {
            return BodyIndex;
        }
    }

    return -1;
}

const FPhysicsBody* UPhysicsAsset::FindBody(const FName& Name) const
{
    return GetBody(FindBodyIndex(Name));
}

bool UPhysicsAsset::HasValidPhysicsData() const
{
    return PhysicsAssetData != nullptr && PhysicsAssetData->HasValidBodyData();
}
