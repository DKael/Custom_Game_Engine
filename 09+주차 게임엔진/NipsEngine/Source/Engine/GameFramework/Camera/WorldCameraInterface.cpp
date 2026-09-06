#include "GameFramework/Camera/WorldCameraInterface.h"

#include "GameFramework/World.h"

FWorldCameraInterface::FWorldCameraInterface(UWorld* InOwnerWorld)
{
    // Keep constructor lightweight and deterministic.
    SetOwnerWorld(InOwnerWorld);
}

void FWorldCameraInterface::SetOwnerWorld(UWorld* InOwnerWorld)
{
    // Non-owning assignment only. No allocation/no ownership transfer.
    OwnerWorld = InOwnerWorld;
}

void FWorldCameraInterface::Reset()
{
    // Keep pointer ownership unchanged; just leave a deterministic call site
    // in BeginPlay/EndPlay/PostDuplicate lifecycle paths.
}

void FWorldCameraInterface::SetViewTarget(AActor* NewTarget)
{
    if (OwnerWorld)
    {
        // Camera routing stays centralized in PlayerCameraManager.
        OwnerWorld->GetPlayerCameraManager().SetViewTarget(NewTarget);
    }
}

void FWorldCameraInterface::SetViewTargetWithBlend(AActor* NewTarget, float BlendTime)
{
    SetViewTargetWithBlend(NewTarget, BlendTime, ECameraBlendFunction::SmoothStep);
}

void FWorldCameraInterface::SetViewTargetWithBlend(AActor* NewTarget, float BlendTime, ECameraBlendFunction BlendFunction)
{
    if (OwnerWorld)
    {
        // Blend algorithm (location/rotation/FOV) is owned by camera manager.
        OwnerWorld->GetPlayerCameraManager().SetViewTargetWithBlend(NewTarget, BlendTime, BlendFunction);
    }
}

void FWorldCameraInterface::AddCameraShake(const FCameraShakeParams& Params)
{
    if (OwnerWorld)
    {
        // Adds transient shake modifier; does not mutate source actor transform.
        OwnerWorld->GetPlayerCameraManager().AddCameraShake(Params);
    }
}

void FWorldCameraInterface::AddFOVKick(float AddFovDegrees, float Duration)
{
    if (OwnerWorld)
    {
        // Adds transient FOV pulse modifier.
        OwnerWorld->GetPlayerCameraManager().AddFOVKick(AddFovDegrees, Duration);
    }
}

void FWorldCameraInterface::FadeIn(float Duration)
{
    if (OwnerWorld)
    {
        // Forward to camera manager screen-effect animation state.
        OwnerWorld->GetPlayerCameraManager().FadeIn(Duration);
    }
}

void FWorldCameraInterface::FadeOut(float Duration)
{
    if (OwnerWorld)
    {
        // Forward to camera manager screen-effect animation state.
        OwnerWorld->GetPlayerCameraManager().FadeOut(Duration);
    }
}

void FWorldCameraInterface::SetLetterBox(float Amount, float BlendTime)
{
    if (OwnerWorld)
    {
        // Forward letterbox scalar and blend parameters.
        OwnerWorld->GetPlayerCameraManager().SetLetterBox(Amount, BlendTime);
    }
}

void FWorldCameraInterface::SetVignette(float Intensity, float Radius, float Softness)
{
    if (OwnerWorld)
    {
        // Forward vignette parameters to screen-effect settings.
        OwnerWorld->GetPlayerCameraManager().SetVignette(Intensity, Radius, Softness);
    }
}

void FWorldCameraInterface::EnableGammaCorrection(bool bEnabled)
{
    if (OwnerWorld)
    {
        // Toggle flag used by postprocess shader path.
        OwnerWorld->GetPlayerCameraManager().EnableGammaCorrection(bEnabled);
    }
}

void FWorldCameraInterface::SetBaseTimeDilation(float InTimeDilation)
{
    if (OwnerWorld)
    {
        // Global gameplay time scale baseline.
        OwnerWorld->SetBaseTimeDilation(InTimeDilation);
    }
}

void FWorldCameraInterface::HitStop(float Duration, float TimeScale)
{
    if (OwnerWorld)
    {
        // TimeManager resolves hit-stop priority over slomo.
        OwnerWorld->StartHitStop(Duration, TimeScale);
    }
}

void FWorldCameraInterface::Slomo(float TimeScale, float Duration)
{
    if (OwnerWorld)
    {
        // Non-blocking slomo request driven by per-frame time update.
        OwnerWorld->StartSlomo(TimeScale, Duration);
    }
}
