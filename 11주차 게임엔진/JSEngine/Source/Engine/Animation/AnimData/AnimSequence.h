#pragma once

#include "Animation/AnimData/AnimDataModel.h"
#include "Animation/AnimData/AnimSequenceBase.h"

class USkeleton;
class FAclAnimSequenceRuntimeData;

class UAnimSequence : public UAnimSequenceBase
{
public:
    ~UAnimSequence() override;

    void SetSkeleton(USkeleton* InSkeleton)
    {
        Skeleton = InSkeleton;
    }

    USkeleton* GetSkeleton() const
    {
        return Skeleton;
    }

    void SetSkeletonAssetPath(const FString& InSkeletonAssetPath)
    {
        SkeletonAssetPath = InSkeletonAssetPath;
    }

    const FString& GetSkeletonAssetPath() const
    {
        return SkeletonAssetPath;
    }

    const TArray<FAnimNotifyTrack>& GetNotifyTracks() const
    {
        static const TArray<FAnimNotifyTrack> EmptyTracks;
        return DataModel ? DataModel->NotifyTracks : EmptyTracks;
    }

    TArray<FAnimNotifyTrack>& GetMutableNotifyTracks()
    {
        static TArray<FAnimNotifyTrack> EmptyTracks;
        return DataModel ? DataModel->NotifyTracks : EmptyTracks;
    }

    bool HasAclRuntimeData() const;
    const FAclAnimSequenceRuntimeData* GetAclRuntimeData() const;
    FAclAnimSequenceRuntimeData* GetMutableAclRuntimeData();
    void SetAclRuntimeData(FAclAnimSequenceRuntimeData* InAclRuntimeData);
    void ClearAclRuntimeData();

    UAnimDataModel* DataModel = nullptr;
    FAclAnimSequenceRuntimeData* AclRuntimeData = nullptr;
    FString SkeletonAssetPath;
    USkeleton* Skeleton = nullptr;
};
