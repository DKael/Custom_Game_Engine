#pragma once

#include "Math/Quat.h"
#include "Math/Utils.h"
#include "Math/Vector.h"
#include "Render/Common/ViewTypes.h"

struct FMinimalViewInfo
{
    FVector Location = FVector::ZeroVector;
    FQuat Rotation = FQuat::Identity;
    float FOV = MathUtil::DegreesToRadians(60.0f);
    float NearClip = 0.1f;
    float FarClip = 1000.0f;
    bool bOrthographic = false;
    float OrthoHeight = 10.0f;
};

struct FViewTarget
{
    class AActor* Target = nullptr;
    FMinimalViewInfo POV;
};

using FCameraScreenEffectSettings = FScreenEffectSettings;

enum class ECameraBlendFunction : uint8
{
    Linear = 0,
    SmoothStep,
    EaseIn,
    EaseOut,
    EaseInOut
};
