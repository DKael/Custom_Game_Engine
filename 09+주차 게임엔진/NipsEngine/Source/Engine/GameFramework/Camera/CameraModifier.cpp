#include "GameFramework/Camera/CameraModifier.h"

#include "Math/Utils.h"

#include <algorithm>

FCameraModifier::~FCameraModifier() = default;

FTimedCameraModifier::FTimedCameraModifier(float InDurationSeconds)
    : DurationSeconds(std::max(0.0f, InDurationSeconds))
{
}

bool FTimedCameraModifier::Advance(float DeltaTime, float& OutNormalizedTime)
{
    // Invalid lifetime or previously disabled modifier expires immediately.
    if (bDisabled || DurationSeconds <= 0.0f)
    {
        bDisabled = true;
        OutNormalizedTime = 1.0f;
        return false;
    }

    // Lifetime is driven by manager update time; negative deltas are clamped out.
    ElapsedSeconds = std::max(0.0f, ElapsedSeconds + DeltaTime);
    OutNormalizedTime = MathUtil::Clamp(ElapsedSeconds / DurationSeconds, 0.0f, 1.0f);
    if (ElapsedSeconds >= DurationSeconds)
    {
        // Mark disabled so manager removes this modifier in the same frame.
        bDisabled = true;
        return false;
    }

    return true;
}
