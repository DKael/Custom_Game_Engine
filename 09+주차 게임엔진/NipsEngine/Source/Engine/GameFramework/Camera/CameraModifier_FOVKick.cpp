#include "GameFramework/Camera/CameraModifier_FOVKick.h"

#include "Math/Utils.h"

#include <cmath>

FFOVKickModifier::FFOVKickModifier(float InAddFovDegrees, float InDuration)
    : FTimedCameraModifier(MathUtil::Clamp(InDuration, 0.0f, 10.0f))
    , AddFovRadians(MathUtil::DegreesToRadians(InAddFovDegrees))
{
    Priority = 32;
}

bool FFOVKickModifier::ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV)
{
    // The base class handles duration and normalized progress.
    float NormalizedTime = 1.0f;
    if (!Advance(DeltaTime, NormalizedTime))
    {
        return false;
    }

    // One smooth pulse over the lifetime:
    //  sin(pi*t) => 0 at t=0, max at t=0.5, back to 0 at t=1.
    const float Pulse = std::sin(MathUtil::PI * NormalizedTime);
    InOutPOV.FOV += AddFovRadians * Pulse;
    return true;
}
