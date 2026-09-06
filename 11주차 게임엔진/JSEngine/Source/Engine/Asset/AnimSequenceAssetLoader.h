#pragma once

#include "Asset/IAssetLoader.h"

class UAnimSequence;

struct FAnimSequenceAssetMetadata
{
    FString ObjectName;
    FString SkeletonAssetPath;
    int32 FrameRateNumerator = 0;
    int32 FrameRateDenominator = 0;
    int32 NumberOfFrames = 0;
    int32 NumberOfKeys = 0;
    int32 BoneTrackCount = 0;

    bool IsValid() const
    {
        return !ObjectName.empty()
            || !SkeletonAssetPath.empty()
            || FrameRateNumerator > 0
            || FrameRateDenominator > 0
            || NumberOfFrames > 0
            || NumberOfKeys > 0
            || BoneTrackCount > 0;
    }
};

class FAnimSequenceAssetLoader : public IAssetLoader
{
public:
    UAnimSequence* Load(const FString& Path) const;
    bool LoadMetadata(const FString& Path, FAnimSequenceAssetMetadata& OutMetadata) const;
    bool Save(const FString& Path, const UAnimSequence* Sequence) const;

    bool SupportsExtension(const FString& Extension) const override;
    FString GetLoaderName() const override;
};
