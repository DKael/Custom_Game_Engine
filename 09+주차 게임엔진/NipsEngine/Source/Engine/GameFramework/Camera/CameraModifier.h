#pragma once

#include "Core/CoreTypes.h"
#include "GameFramework/Camera/CameraTypes.h"

class FPlayerCameraManager;

// Base interface for camera post-view calculation adjustments.
// A modifier receives the current frame's temporary POV and can mutate it.
// This must never edit actor/component transforms directly.
class FCameraModifier
{
public:
    virtual ~FCameraModifier();

    // Returns true if the modifier is still active after this call.
    // Returns false to request removal from the manager list.
    virtual bool ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV) = 0;

    bool IsDisabled() const { return bDisabled; }
    uint8 GetPriority() const { return Priority; }
    void SetCameraOwner(FPlayerCameraManager* InOwner) { CameraOwner = InOwner; }

protected:
    // Non-owning back reference for querying manager state when needed.
    FPlayerCameraManager* CameraOwner = nullptr;
    // Reserved blend weight for future layering. (Current implementation uses full strength.)
    float Alpha = 1.0f;
    // Internal kill switch used by manager cleanup and self-expiration.
    bool bDisabled = false;
    // Smaller number means earlier evaluation. Higher priority is applied later.
    uint8 Priority = 128;
};

// Base helper for time-limited camera modifiers (shake, FOV kick, etc.).
class FTimedCameraModifier : public FCameraModifier
{
public:
    explicit FTimedCameraModifier(float InDurationSeconds);

protected:
    // Updates elapsed lifetime and normalized time [0,1].
    // Returns false when the modifier should be removed.
    bool Advance(float DeltaTime, float& OutNormalizedTime);
    // Time accumulator for periodic wave functions.
    float GetElapsedSeconds() const { return ElapsedSeconds; }
    float GetTotalDuration() const { return DurationSeconds; }

private:
    // Total life duration in seconds.
    float DurationSeconds = 0.0f;
    // Accumulated unscaled camera delta time.
    float ElapsedSeconds = 0.0f;
};
