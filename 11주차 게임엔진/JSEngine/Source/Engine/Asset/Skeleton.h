#pragma once

#include "Object/Object.h"
#include "SkeletonTypes.h"
#include "Generated/Skeleton.generated.h"

UCLASS()
class USkeleton : public UObject
{
public:
    GENERATED_BODY()

    USkeleton() = default;
    ~USkeleton() override;

    void SetSkeletonData(FSkeletonData* InSkeletonData);

    FSkeletonData* GetSkeletonData();
    const FSkeletonData* GetSkeletonData() const;

    const FString& GetAssetPathFileName() const;

    const TArray<FSkeletonBone>& GetBones() const;
    const TArray<FSkeletonSocket>& GetSockets() const;
    const TArray<FSkeletonCurveMetaData>& GetCurveMetaData() const;
    const TArray<FString>& GetCompatibleSkeletons() const;
    TArray<FString>* GetMutableCompatibleSkeletons();

    int32 FindBoneIndex(const FName& BoneName) const;
    int32 FindBoneIndex(const FString& BoneName) const;
    const FSkeletonBone* GetBoneInfo(int32 BoneIndex) const;

    const FMatrix& GetLocalBindTransform(int32 BoneIndex) const;
    const FMatrix& GetGlobalBindTransform(int32 BoneIndex) const;
    const FMatrix& GetInverseBindPose(int32 BoneIndex) const;

    const FSkeletonSocket* FindSocket(const FName& Name) const;
    bool HasSocket(const FName& Name) const;

    const FSkeletonCurveMetaData* FindCurveMetaData(const FName& Name) const;
    bool HasCurveMetaData(const FName& Name) const;

    bool HasValidSkeletonData() const;

private:
    FSkeletonData* SkeletonData = nullptr;
};
