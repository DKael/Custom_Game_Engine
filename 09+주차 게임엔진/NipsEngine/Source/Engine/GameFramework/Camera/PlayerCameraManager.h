#pragma once

#include "Core/Containers/Array.h"
#include "GameFramework/Camera/CameraModifier.h"
#include "GameFramework/Camera/CameraTypes.h"

#include <memory>

class AActor;
struct FCameraShakeParams;
class FViewportCamera;
class UWorld;

// Simplified Unreal-like player camera manager.
//
// Runtime order per frame:
// 1) Update transient screen effects (fade/letterbox animation).
// 2) Resolve current/pending view targets and calculate base final POV.
// 3) Apply camera modifiers (shake, fov kick, ...).
// 4) Push final POV into viewport camera for renderer consumption.
class FPlayerCameraManager
{
public:
    FPlayerCameraManager() = default;
    explicit FPlayerCameraManager(UWorld* InOwnerWorld);

    // The manager does not own world lifetime; this is a non-owning reference.
    void SetOwnerWorld(UWorld* InOwnerWorld);

    // Clears transient camera state so BeginPlay/EndPlay/start-over paths are deterministic.
    void Reset();

    // Main per-frame update entry using unscaled delta time.
    void Update(float UnscaledDeltaTime);

    // Copies calculated final POV into the runtime viewport camera object.
    void ApplyToViewportCamera(FViewportCamera& InOutCamera) const;

    // Immediate target switch.
    void SetViewTarget(AActor* NewTarget);
    // Time-blended target switch.
    void SetViewTargetWithBlend(AActor* NewTarget, float BlendTime);
    void SetViewTargetWithBlend(AActor* NewTarget, float BlendTime, ECameraBlendFunction BlendFunction);

    AActor* GetViewTarget() const { return CurrentViewTarget.Target; }
    const FMinimalViewInfo& GetFinalPOV() const { return FinalPOV; }
    const FCameraScreenEffectSettings& GetScreenEffectSettings() const { return ScreenEffects; }

    // Spawns transient modifiers. They affect only final POV per frame.
    void AddCameraShake(const FCameraShakeParams& Params);
    void AddFOVKick(float AddFovDegrees, float Duration);

    // Post-process control helpers.
    void FadeIn(float Duration, FVector Color = {0.f, 0.f, 0.f});
    void FadeOut(float Duration, FVector Color = { 0.f, 0.f, 0.f });
    void SetLetterBox(float Amount, float BlendTime);
    void SetVignette(float Intensity, float Radius, float Softness, FVector Color = FVector::ZeroVector, float BlendTime = 0.0f);
    void EnableGammaCorrection(bool bEnabled);

private:
    struct FBlendState;
    struct FAnimatedScalar;

    // Evaluates current/pending camera targets and produces base FinalPOV.
    void UpdateViewTargets(float UnscaledDeltaTime);
    // Advances fade/letterbox animation and writes into ScreenEffects.
    void UpdateTransientScreenEffects(float UnscaledDeltaTime);
    // Applies sorted camera modifiers on top of base FinalPOV.
    void ApplyModifiers(float UnscaledDeltaTime);

    // Reads camera component data into a view target POV snapshot.
    bool RefreshViewTargetPOV(FViewTarget& InOutViewTarget) const;
    // Fallback policy when requested target is invalid or missing camera component.
    AActor* ResolveDefaultViewTarget() const;
    AActor* ResolveActiveViewTarget(AActor* RequestedTarget) const;
    // Blend helper for transform/FOV/projection fields.
    FMinimalViewInfo BlendPOV(const FMinimalViewInfo& From, const FMinimalViewInfo& To, float Alpha) const;

    // Internal state-reset helpers for deterministic lifecycle transitions.
    void ResetViewTargets();
    void ResetScreenEffects();

    // Scalar animation helpers for fade/letterbox.
    void SetAnimatedScalarImmediate(FAnimatedScalar& InOutState, float Value);
    void StartAnimatedScalar(FAnimatedScalar& InOutState, float StartValue, float TargetValue, float Duration);
    void TickAnimatedScalar(FAnimatedScalar& InOutState, float UnscaledDeltaTime);

    // View-target blend state machine helpers.
    void BeginViewTargetBlend(float BlendTime, ECameraBlendFunction BlendFunction);
    void CancelViewTargetBlend();
    void FinalizeViewTargetBlend();
    float EvaluateViewTargetBlendAlpha() const;

private:
    // Non-owning world pointer.
    UWorld* OwnerWorld = nullptr;

    // Active and pending view target snapshots.
    FViewTarget CurrentViewTarget;
    FViewTarget PendingViewTarget;
    // Final, post-modifier POV used by renderer through viewport camera.
    FMinimalViewInfo FinalPOV;

    // Active camera blend timeline between CurrentViewTarget and PendingViewTarget.
    struct FBlendState
    {
        bool bActive = false;
        float Duration = 0.0f;
        float Elapsed = 0.0f;
        ECameraBlendFunction Function = ECameraBlendFunction::SmoothStep;
    };

    // Generic scalar timeline used by fade and letterbox interpolation.
    struct FAnimatedScalar
    {
        float Current = 0.0f;
        float Start = 0.0f;
        float Target = 0.0f;
        float Duration = 0.0f;
        float Elapsed = 0.0f;
        bool bActive = false;
    };

    // Hard clamps to keep user/script inputs in safe ranges.
    static constexpr float MaxViewTargetBlendDuration = 60.0f;
    static constexpr float MaxScreenAnimationDuration = 60.0f;
    static constexpr float MaxLetterBoxAmount = 0.45f;
    static constexpr float DefaultGamma = 2.2f;

    FBlendState ViewTargetBlend;
    FAnimatedScalar FadeAnimation;
    FAnimatedScalar LetterBoxAnimation;
    FAnimatedScalar VignetteIntensityAnimation;
    FAnimatedScalar VignetteRadiusAnimation;

    // Values consumed by post-process presentation pass.
    FCameraScreenEffectSettings ScreenEffects;
    // Sorted each frame by priority and removed automatically on completion.
    TArray<std::unique_ptr<FCameraModifier>> Modifiers;
};
