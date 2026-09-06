#pragma once

#include "GameFramework/Camera/CameraModifier.h"

// Temporary FOV pulse for impact feel.
// Uses a sine-shaped curve: 0 -> peak -> 0.
class FFOVKickModifier final : public FTimedCameraModifier
{
public:
    // AddFovDegrees: maximum additive FOV at pulse apex.
    // Duration: full pulse duration in seconds.
    FFOVKickModifier(float InAddFovDegrees, float InDuration);

    bool ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV) override;

private:
    // Stored in radians because engine FOV is handled in radians.
    float AddFovRadians = 0.0f;
};
