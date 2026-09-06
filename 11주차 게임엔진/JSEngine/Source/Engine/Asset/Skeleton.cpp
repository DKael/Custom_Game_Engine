#include "Skeleton.h"

#include "Engine/Geometry/Transform.h"

FMatrix FSkeletonSocket::GetRelativeTransform() const
{
    return FTransform(RelativeRotation, RelativeLocation, RelativeScale).ToMatrixWithScale();
}

void FSkeletonData::Clear()
{
    PathFileName.clear();
    Bones.clear();
    Sockets.clear();
    CurveMetaData.clear();
    CompatibleSkeletons.clear();
}

bool FSkeletonData::HasValidBoneData() const
{
    return !Bones.empty();
}

USkeleton::~USkeleton()
{
    delete SkeletonData;
    SkeletonData = nullptr;
}

void USkeleton::SetSkeletonData(FSkeletonData* InSkeletonData)
{
    if (SkeletonData == InSkeletonData)
    {
        return;
    }

    delete SkeletonData;
    SkeletonData = InSkeletonData;
}

FSkeletonData* USkeleton::GetSkeletonData()
{
    return SkeletonData;
}

const FSkeletonData* USkeleton::GetSkeletonData() const
{
    return SkeletonData;
}

const FString& USkeleton::GetAssetPathFileName() const
{
    static const FString Empty = {};
    return SkeletonData ? SkeletonData->PathFileName : Empty;
}

const TArray<FSkeletonBone>& USkeleton::GetBones() const
{
    static const TArray<FSkeletonBone> Empty = {};
    return SkeletonData ? SkeletonData->Bones : Empty;
}

const TArray<FSkeletonSocket>& USkeleton::GetSockets() const
{
    static const TArray<FSkeletonSocket> Empty = {};
    return SkeletonData ? SkeletonData->Sockets : Empty;
}

const TArray<FSkeletonCurveMetaData>& USkeleton::GetCurveMetaData() const
{
    static const TArray<FSkeletonCurveMetaData> Empty = {};
    return SkeletonData ? SkeletonData->CurveMetaData : Empty;
}

const TArray<FString>& USkeleton::GetCompatibleSkeletons() const
{
    static const TArray<FString> Empty = {};
    return SkeletonData ? SkeletonData->CompatibleSkeletons : Empty;
}

TArray<FString>* USkeleton::GetMutableCompatibleSkeletons()
{
    return SkeletonData ? &SkeletonData->CompatibleSkeletons : nullptr;
}

int32 USkeleton::FindBoneIndex(const FName& BoneName) const
{
    if (!SkeletonData || !BoneName.IsValid())
    {
        return -1;
    }

    for (int32 BoneIndex = 0; BoneIndex < static_cast<int32>(SkeletonData->Bones.size()); ++BoneIndex)
    {
        if (SkeletonData->Bones[BoneIndex].Name == BoneName)
        {
            return BoneIndex;
        }
    }

    return -1;
}

int32 USkeleton::FindBoneIndex(const FString& BoneName) const
{
    return FindBoneIndex(FName(BoneName));
}

const FSkeletonBone* USkeleton::GetBoneInfo(int32 BoneIndex) const
{
    if (!SkeletonData || BoneIndex < 0 || BoneIndex >= static_cast<int32>(SkeletonData->Bones.size()))
    {
        return nullptr;
    }

    return &SkeletonData->Bones[BoneIndex];
}

const FMatrix& USkeleton::GetLocalBindTransform(int32 BoneIndex) const
{
    static const FMatrix Identity = FMatrix::Identity;
    const FSkeletonBone* Bone = GetBoneInfo(BoneIndex);
    return Bone ? Bone->LocalBindTransform : Identity;
}

const FMatrix& USkeleton::GetGlobalBindTransform(int32 BoneIndex) const
{
    static const FMatrix Identity = FMatrix::Identity;
    const FSkeletonBone* Bone = GetBoneInfo(BoneIndex);
    return Bone ? Bone->GlobalBindTransform : Identity;
}

const FMatrix& USkeleton::GetInverseBindPose(int32 BoneIndex) const
{
    static const FMatrix Identity = FMatrix::Identity;
    const FSkeletonBone* Bone = GetBoneInfo(BoneIndex);
    return Bone ? Bone->InverseBindPose : Identity;
}

const FSkeletonSocket* USkeleton::FindSocket(const FName& Name) const
{
    if (!SkeletonData || !Name.IsValid())
    {
        return nullptr;
    }

    for (const FSkeletonSocket& Socket : SkeletonData->Sockets)
    {
        if (Socket.Name == Name)
        {
            return &Socket;
        }
    }

    return nullptr;
}

bool USkeleton::HasSocket(const FName& Name) const
{
    return FindSocket(Name) != nullptr;
}

const FSkeletonCurveMetaData* USkeleton::FindCurveMetaData(const FName& Name) const
{
    if (!SkeletonData || !Name.IsValid())
    {
        return nullptr;
    }

    for (const FSkeletonCurveMetaData& MetaData : SkeletonData->CurveMetaData)
    {
        if (MetaData.Name == Name)
        {
            return &MetaData;
        }
    }

    return nullptr;
}

bool USkeleton::HasCurveMetaData(const FName& Name) const
{
    return FindCurveMetaData(Name) != nullptr;
}

bool USkeleton::HasValidSkeletonData() const
{
    return SkeletonData != nullptr && SkeletonData->HasValidBoneData();
}
