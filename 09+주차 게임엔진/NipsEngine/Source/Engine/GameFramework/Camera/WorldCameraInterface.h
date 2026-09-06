#pragma once

#include "GameFramework/Camera/CameraTypes.h"

class AActor;
struct FCameraShakeParams;
class UWorld;

// Unified entry point for gameplay-facing camera/hit-feel commands.
// This keeps Lua and high-level gameplay code independent from low-level manager layout.
//
// Why this exists:
// - Lua/gameplay code should call one stable API surface.
// - Internal implementations (camera manager, time manager) can change without
//   forcing script-level API churn.
// - This class is intentionally thin: it forwards commands to UWorld-owned systems.
class FWorldCameraInterface
{
public:
    FWorldCameraInterface() = default;
    explicit FWorldCameraInterface(UWorld* InOwnerWorld);

    // The interface does not own world lifetime.
    // Must be called whenever world ownership context changes
    // (constructor/PostDuplicate/BeginPlay paths).
    void SetOwnerWorld(UWorld* InOwnerWorld);
    // Reserved lifecycle hook for deterministic world transitions.
    // Current implementation has no transient internal state.
    void Reset();

    // Camera view target control.
    // Immediate switch to target camera source.
    void SetViewTarget(AActor* NewTarget);
    // Blended switch to target camera source.
    // Blend interpolation is implemented inside PlayerCameraManager.
    void SetViewTargetWithBlend(AActor* NewTarget, float BlendTime);
    void SetViewTargetWithBlend(AActor* NewTarget, float BlendTime, ECameraBlendFunction BlendFunction);

    // Camera modifiers.
    // These spawn transient modifiers that affect only per-frame FinalPOV.
    void AddCameraShake(const FCameraShakeParams& Params);
    void AddFOVKick(float AddFovDegrees, float Duration);

    // Screen effects.
    // Values are consumed by postprocess/screen-effects pass downstream.
    void FadeIn(float Duration);
    void FadeOut(float Duration);
    void SetLetterBox(float Amount, float BlendTime);
    void SetVignette(float Intensity, float Radius, float Softness);
    void EnableGammaCorrection(bool bEnabled);

    // Time-dilation hit-feel controls.
    // Forwarded to world time manager through UWorld wrappers.
    void SetBaseTimeDilation(float InTimeDilation);
    void HitStop(float Duration, float TimeScale = 0.05f);
    void Slomo(float TimeScale, float Duration);

private:
    // Non-owning pointer. Lifetime is managed by UWorld.
    UWorld* OwnerWorld = nullptr;
};
