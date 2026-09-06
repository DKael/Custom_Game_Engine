#pragma once

#include "Core/CoreMinimal.h"
#include "Math/Rotator.h"
#include "Object/FName.h"

struct FSkeletonBone
{
    FName Name;
    FString ExportName;
    int32 ParentIndex = -1;

    FMatrix LocalBindTransform = FMatrix::Identity;
    FMatrix GlobalBindTransform = FMatrix::Identity;
    FMatrix InverseBindPose = FMatrix::Identity;

    FAABB BoneBounds;
};

struct FSkeletonSocket
{
    FName Name;
    int32 BoneIndex = -1;

    FVector RelativeLocation = FVector::ZeroVector;
    FRotator RelativeRotation;
    FVector RelativeScale = FVector(1.0f, 1.0f, 1.0f);

    FMatrix GetRelativeTransform() const;
};

struct FSkeletonCurveMetaData
{
    FName Name;

    bool bMorphTarget = false;
    bool bMaterial = false;
};

struct FSkeletonData
{
    FString PathFileName;

    TArray<FSkeletonBone> Bones;
    TArray<FSkeletonSocket> Sockets;
    TArray<FSkeletonCurveMetaData> CurveMetaData;
    TArray<FString> CompatibleSkeletons;

    void Clear();
    bool HasValidBoneData() const;
};
