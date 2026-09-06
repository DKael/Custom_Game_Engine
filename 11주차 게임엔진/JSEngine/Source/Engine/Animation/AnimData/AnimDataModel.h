#pragma once

#include "Object/Object.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "Animation/AnimData/FrameRate.h"
#include "Animation/AnimData/AnimNotifyTypes.h"

class UAnimDataModel : public UObject
{
public:
    TArray<FBoneAnimationTrack> BoneAnimationTracks;
    TArray<FAnimNotifyTrack> NotifyTracks;

	FFrameRate FrameRate;
    int32 NumberOfFrames = 0;
    int32 NumberOfKeys = 0;
    FAnimationCurveData CurveData;
};
