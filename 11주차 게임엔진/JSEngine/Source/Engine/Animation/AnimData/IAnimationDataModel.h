#pragma once

#include "Object/FName.h"
#include "Animation/AnimData/AnimTypes.h"
#include "Asset/CurveFloatAsset.h"

struct FBoneAnimationTrack
{
    FRawAnimSequenceTrack InternalTrackData;

    int32 BoneTreeIndex = -1;
    FName Name;
};

struct FAnimationCurveData
{
    TArray<FFloatCurve> FloatCurves;
};