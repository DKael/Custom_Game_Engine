#include "GameFramework/Camera/PlayerCameraManager.h"

#include "Component/CameraComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Camera/CameraModifier_CameraShake.h"
#include "GameFramework/Camera/CameraModifier_FOVKick.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/World.h"
#include "Viewport/ViewportCamera.h"

#include <algorithm>

namespace
{
    // Common easing used for camera and screen-effect blends.
    float SmoothStep01(float Alpha)
    {
        const float ClampedAlpha = MathUtil::Clamp(Alpha, 0.0f, 1.0f);
        return ClampedAlpha * ClampedAlpha * (3.0f - 2.0f * ClampedAlpha);
    }

    float EaseIn01(float Alpha)
    {
        const float ClampedAlpha = MathUtil::Clamp(Alpha, 0.0f, 1.0f);
        return ClampedAlpha * ClampedAlpha;
    }

    float EaseOut01(float Alpha)
    {
        const float ClampedAlpha = MathUtil::Clamp(Alpha, 0.0f, 1.0f);
        const float Inverse = 1.0f - ClampedAlpha;
        return 1.0f - (Inverse * Inverse);
    }

    float EaseInOut01(float Alpha)
    {
        const float ClampedAlpha = MathUtil::Clamp(Alpha, 0.0f, 1.0f);
        if (ClampedAlpha < 0.5f)
        {
            return 2.0f * ClampedAlpha * ClampedAlpha;
        }

        const float Inverse = 1.0f - ClampedAlpha;
        return 1.0f - (2.0f * Inverse * Inverse);
    }

    // View-target validity check used before dereferencing actor/component pointers.
    bool IsValidViewActor(AActor* Actor)
    {
        return Actor != nullptr &&
            UObject::IsValid(Actor) &&
            !Actor->IsPendingDestroy() &&
            !Actor->IsBeingDestroyed();
    }

    // Retrieves the most relevant camera component on an actor.
    // Pawn path is prioritized because gameplay typically drives camera from pawn.
    UCameraComponent* FindCameraComponent(AActor* Actor)
    {
        if (!IsValidViewActor(Actor))
        {
            return nullptr;
        }

        if (APawn* Pawn = Cast<APawn>(Actor))
        {
            return Pawn->GetCameraComponent();
        }

        for (UActorComponent* Component : Actor->GetComponents())
        {
            if (UCameraComponent* CameraComponent = Cast<UCameraComponent>(Component))
            {
                return CameraComponent;
            }
        }

        return nullptr;
    }
}

FPlayerCameraManager::FPlayerCameraManager(UWorld* InOwnerWorld)
{
    SetOwnerWorld(InOwnerWorld);
}

void FPlayerCameraManager::SetOwnerWorld(UWorld* InOwnerWorld)
{
    OwnerWorld = InOwnerWorld;
}

void FPlayerCameraManager::Reset()
{
    // Reset in logical groups to keep lifecycle transitions easy to reason about.
    ResetViewTargets();
    ResetScreenEffects();
    SetAnimatedScalarImmediate(FadeAnimation, 0.0f);
    SetAnimatedScalarImmediate(LetterBoxAnimation, 0.0f);
    SetAnimatedScalarImmediate(VignetteIntensityAnimation, 0.0f);
    SetAnimatedScalarImmediate(VignetteRadiusAnimation, ScreenEffects.VignetteRadius);
    Modifiers.clear();
}

void FPlayerCameraManager::ResetViewTargets()
{
    // Keep this symmetrical with BeginPlay/EndPlay to avoid stale targets.
    CurrentViewTarget = {};
    PendingViewTarget = {};
    ViewTargetBlend = {};
    FinalPOV = FMinimalViewInfo();
}

void FPlayerCameraManager::ResetScreenEffects()
{
    // Post-process defaults.
    ScreenEffects = {};
    ScreenEffects.Gamma = DefaultGamma;
}

void FPlayerCameraManager::SetAnimatedScalarImmediate(FAnimatedScalar& InOutState, float Value)
{
    // Hard-set animation and stop any in-flight interpolation.
    InOutState.Current = Value;
    InOutState.Start = Value;
    InOutState.Target = Value;
    InOutState.Duration = 0.0f;
    InOutState.Elapsed = 0.0f;
    InOutState.bActive = false;
}

void FPlayerCameraManager::StartAnimatedScalar(FAnimatedScalar& InOutState, float StartValue, float TargetValue, float Duration)
{
    // Initialize animation from current visual value toward target.
    InOutState.Start = StartValue;
    InOutState.Current = StartValue;
    InOutState.Target = TargetValue;
    InOutState.Duration = MathUtil::Clamp(Duration, 0.0f, MaxScreenAnimationDuration);
    InOutState.Elapsed = 0.0f;
    InOutState.bActive = InOutState.Duration > 0.0f;

    if (!InOutState.bActive)
    {
        // Zero/negative duration means immediate set.
        InOutState.Current = TargetValue;
    }
}

void FPlayerCameraManager::TickAnimatedScalar(FAnimatedScalar& InOutState, float UnscaledDeltaTime)
{
    if (!InOutState.bActive)
    {
        return;
    }

    if (InOutState.Duration <= 0.0f)
    {
        // Safety path for invalid durations.
        SetAnimatedScalarImmediate(InOutState, InOutState.Target);
        return;
    }

    // SmoothStep for less mechanical UI/cinematic transitions.
    InOutState.Elapsed += UnscaledDeltaTime;
    const float Alpha = SmoothStep01(InOutState.Elapsed / InOutState.Duration);
    InOutState.Current = InOutState.Start + (InOutState.Target - InOutState.Start) * Alpha;
    if (InOutState.Elapsed >= InOutState.Duration)
    {
        InOutState.Current = InOutState.Target;
        InOutState.bActive = false;
    }
}

void FPlayerCameraManager::BeginViewTargetBlend(float BlendTime, ECameraBlendFunction BlendFunction)
{
    // Blend always starts from current frame state.
    ViewTargetBlend.bActive = true;
    ViewTargetBlend.Duration = MathUtil::Clamp(BlendTime, 0.0f, MaxViewTargetBlendDuration);
    ViewTargetBlend.Elapsed = 0.0f;
    ViewTargetBlend.Function = BlendFunction;
}

void FPlayerCameraManager::CancelViewTargetBlend()
{
    // Clearing pending target avoids stale cross-frame state.
    PendingViewTarget = {};
    ViewTargetBlend = {};
}

void FPlayerCameraManager::FinalizeViewTargetBlend()
{
    // Commit pending as current and clear blend machine.
    CurrentViewTarget = PendingViewTarget;
    CancelViewTargetBlend();
    FinalPOV = CurrentViewTarget.POV;
}

float FPlayerCameraManager::EvaluateViewTargetBlendAlpha() const
{
    // SmallNumber guard avoids divide-by-zero on unexpected durations.
    const float Duration = std::max(ViewTargetBlend.Duration, MathUtil::SmallNumber);
    const float RawAlpha = MathUtil::Clamp(ViewTargetBlend.Elapsed / Duration, 0.0f, 1.0f);

    switch (ViewTargetBlend.Function)
    {
    case ECameraBlendFunction::Linear:
        return RawAlpha;
    case ECameraBlendFunction::EaseIn:
        return EaseIn01(RawAlpha);
    case ECameraBlendFunction::EaseOut:
        return EaseOut01(RawAlpha);
    case ECameraBlendFunction::EaseInOut:
        return EaseInOut01(RawAlpha);
    case ECameraBlendFunction::SmoothStep:
    default:
        return SmoothStep01(RawAlpha);
    }
}

void FPlayerCameraManager::Update(float UnscaledDeltaTime)
{
    // Order matters:
    // screen effects -> base view target -> camera modifiers.
    UpdateTransientScreenEffects(UnscaledDeltaTime);
    UpdateViewTargets(UnscaledDeltaTime);
    ApplyModifiers(UnscaledDeltaTime);
}

void FPlayerCameraManager::ApplyToViewportCamera(FViewportCamera& InOutCamera) const
{
    // Renderer consumes viewport camera matrices, so we push FinalPOV here.
    InOutCamera.SetLocation(FinalPOV.Location);
    InOutCamera.SetRotation(FinalPOV.Rotation);
    InOutCamera.SetNearPlane(FinalPOV.NearClip);
    InOutCamera.SetFarPlane(FinalPOV.FarClip);

    if (FinalPOV.bOrthographic)
    {
        // Preserve projection mode from the active source camera.
        InOutCamera.SetProjectionType(EViewportProjectionType::Orthographic);
        InOutCamera.SetOrthoHeight(FinalPOV.OrthoHeight);
    }
    else
    {
        InOutCamera.SetProjectionType(EViewportProjectionType::Perspective);
        InOutCamera.SetFOV(FinalPOV.FOV);
    }
}

void FPlayerCameraManager::SetViewTarget(AActor* NewTarget)
{
    // Immediate switch path used when blend time is zero.
    CurrentViewTarget.Target = ResolveActiveViewTarget(NewTarget);
    RefreshViewTargetPOV(CurrentViewTarget);
    CancelViewTargetBlend();
    FinalPOV = CurrentViewTarget.POV;
}

void FPlayerCameraManager::SetViewTargetWithBlend(AActor* NewTarget, float BlendTime)
{
    SetViewTargetWithBlend(NewTarget, BlendTime, ECameraBlendFunction::SmoothStep);
}

void FPlayerCameraManager::SetViewTargetWithBlend(AActor* NewTarget, float BlendTime, ECameraBlendFunction BlendFunction)
{
    // Resolve once to avoid blending toward invalid/destroyed actors.
    AActor* ResolvedTarget = ResolveActiveViewTarget(NewTarget);
    if (BlendTime <= 0.0f || ResolvedTarget == CurrentViewTarget.Target)
    {
        SetViewTarget(ResolvedTarget);
        return;
    }

    if (CurrentViewTarget.Target == nullptr)
    {
        // If no current target exists, initialize from default before blending.
        CurrentViewTarget.Target = ResolveDefaultViewTarget();
        RefreshViewTargetPOV(CurrentViewTarget);
    }

    PendingViewTarget.Target = ResolvedTarget;
    RefreshViewTargetPOV(PendingViewTarget);
    BeginViewTargetBlend(BlendTime, BlendFunction);
}

void FPlayerCameraManager::AddCameraShake(const FCameraShakeParams& Params)
{
    // Modifier list owns lifetime via unique_ptr.
    std::unique_ptr<FCameraModifier> Modifier = std::make_unique<FCameraShakeModifier>(Params);
    Modifier->SetCameraOwner(this);
    Modifiers.push_back(std::move(Modifier));
}

void FPlayerCameraManager::AddFOVKick(float AddFovDegrees, float Duration)
{
    std::unique_ptr<FCameraModifier> Modifier = std::make_unique<FFOVKickModifier>(AddFovDegrees, Duration);
    Modifier->SetCameraOwner(this);
    Modifiers.push_back(std::move(Modifier));
}

void FPlayerCameraManager::FadeIn(float Duration, FVector Color)
{
    // Typical cinematic fade-in starts from fully black to clear.
    StartAnimatedScalar(FadeAnimation, 1.0f, 0.0f, Duration);
    ScreenEffects.FadeAmount = MathUtil::Clamp(FadeAnimation.Current, 0.0f, 1.0f);
    ScreenEffects.FadeColor = Color;
}

void FPlayerCameraManager::FadeOut(float Duration, FVector Color)
{
    // Fade-out starts from current value for chaining safety.
    StartAnimatedScalar(FadeAnimation, ScreenEffects.FadeAmount, 1.0f, Duration);
    ScreenEffects.FadeAmount = MathUtil::Clamp(FadeAnimation.Current, 0.0f, 1.0f);
    ScreenEffects.FadeColor = Color;
}

void FPlayerCameraManager::SetLetterBox(float Amount, float BlendTime)
{
    // Clamp to prevent bars from covering too much frame.
    const float ClampedAmount = MathUtil::Clamp(Amount, 0.0f, MaxLetterBoxAmount);
    if (BlendTime <= 0.0f)
    {
        SetAnimatedScalarImmediate(LetterBoxAnimation, ClampedAmount);
        ScreenEffects.LetterBoxAmount = ClampedAmount;
        return;
    }

    StartAnimatedScalar(LetterBoxAnimation, ScreenEffects.LetterBoxAmount, ClampedAmount, BlendTime);
}

void FPlayerCameraManager::SetVignette(float Intensity, float Radius, float Softness, FVector Color, float BlendTime)
{
    const float ClampedIntensity = MathUtil::Clamp(Intensity, 0.0f, 1.0f);
    const float ClampedRadius = MathUtil::Clamp(Radius, 0.0f, 3.f);

    if (BlendTime <= 0.0f)
    {
        SetAnimatedScalarImmediate(VignetteIntensityAnimation, ClampedIntensity);
        SetAnimatedScalarImmediate(VignetteRadiusAnimation, ClampedRadius);
        ScreenEffects.VignetteIntensity = ClampedIntensity;
        ScreenEffects.VignetteRadius = ClampedRadius;
    }
    else
    {
        StartAnimatedScalar(VignetteIntensityAnimation, ScreenEffects.VignetteIntensity, ClampedIntensity, BlendTime);
        StartAnimatedScalar(VignetteRadiusAnimation, ScreenEffects.VignetteRadius, ClampedRadius, BlendTime);
    }

    ScreenEffects.bVignetteEnabled = ScreenEffects.VignetteIntensity > 0.0f || ClampedIntensity > 0.0f;
    ScreenEffects.VignetteSoftness = Softness;
    ScreenEffects.VignetteColor = Color;
}

void FPlayerCameraManager::EnableGammaCorrection(bool bEnabled)
{
    ScreenEffects.bGammaCorrectionEnabled = bEnabled;
}

void FPlayerCameraManager::UpdateViewTargets(float UnscaledDeltaTime)
{
    // Refresh active target every frame so moving cameras remain live.
    CurrentViewTarget.Target = ResolveActiveViewTarget(CurrentViewTarget.Target);
    RefreshViewTargetPOV(CurrentViewTarget);
    FinalPOV = CurrentViewTarget.POV;

    if (!ViewTargetBlend.bActive)
    {
        return;
    }

    // During blend, both endpoints are refreshed each frame.
    PendingViewTarget.Target = ResolveActiveViewTarget(PendingViewTarget.Target);
    RefreshViewTargetPOV(PendingViewTarget);

    ViewTargetBlend.Elapsed += UnscaledDeltaTime;
    const float BlendAlpha = EvaluateViewTargetBlendAlpha();
    FinalPOV = BlendPOV(CurrentViewTarget.POV, PendingViewTarget.POV, BlendAlpha);

    if (ViewTargetBlend.Elapsed >= ViewTargetBlend.Duration)
    {
        // Finish with exact target values to avoid tiny interpolation residue.
        FinalizeViewTargetBlend();
    }
}

void FPlayerCameraManager::UpdateTransientScreenEffects(float UnscaledDeltaTime)
{
    // Screen effects use unscaled delta by design.
    TickAnimatedScalar(FadeAnimation, UnscaledDeltaTime);
    TickAnimatedScalar(LetterBoxAnimation, UnscaledDeltaTime);
    TickAnimatedScalar(VignetteIntensityAnimation, UnscaledDeltaTime);
    TickAnimatedScalar(VignetteRadiusAnimation, UnscaledDeltaTime);

    ScreenEffects.FadeAmount = MathUtil::Clamp(FadeAnimation.Current, 0.0f, 1.0f);
    ScreenEffects.LetterBoxAmount = MathUtil::Clamp(LetterBoxAnimation.Current, 0.0f, MaxLetterBoxAmount);
    ScreenEffects.VignetteIntensity = MathUtil::Clamp(VignetteIntensityAnimation.Current, 0.0f, 1.0f);
    ScreenEffects.VignetteRadius = MathUtil::Clamp(VignetteRadiusAnimation.Current, 0.0f, 1.5f);
    ScreenEffects.bVignetteEnabled = ScreenEffects.VignetteIntensity > 0.0f;
}

void FPlayerCameraManager::ApplyModifiers(float UnscaledDeltaTime)
{
    if (Modifiers.empty())
    {
        return;
    }

    std::stable_sort(
        Modifiers.begin(),
        Modifiers.end(),
        [](const std::unique_ptr<FCameraModifier>& A, const std::unique_ptr<FCameraModifier>& B)
        {
            return A->GetPriority() < B->GetPriority();
        });

    // Apply higher-priority modifiers last so they can override prior effects.
    for (int32 ModifierIndex = static_cast<int32>(Modifiers.size()) - 1; ModifierIndex >= 0; --ModifierIndex)
    {
        FCameraModifier* Modifier = Modifiers[ModifierIndex].get();
        if (Modifier == nullptr || Modifier->IsDisabled())
        {
            // Remove null/disabled entries eagerly to keep list compact.
            Modifiers.erase(Modifiers.begin() + ModifierIndex);
            continue;
        }

        if (!Modifier->ModifyCamera(UnscaledDeltaTime, FinalPOV))
        {
            // Modifiers self-expire by returning false.
            Modifiers.erase(Modifiers.begin() + ModifierIndex);
        }
    }
}

bool FPlayerCameraManager::RefreshViewTargetPOV(FViewTarget& InOutViewTarget) const
{
    // Missing camera component means this actor cannot be used as a view target.
    UCameraComponent* CameraComponent = FindCameraComponent(InOutViewTarget.Target);
    if (CameraComponent == nullptr)
    {
        return false;
    }

    const FTransform CameraTransform = CameraComponent->GetWorldTransform();
    const FCameraState& CameraState = CameraComponent->GetCameraState();

    // Snapshot actor camera state into plain view data.
    InOutViewTarget.POV.Location = CameraTransform.GetLocation();
    InOutViewTarget.POV.Rotation = CameraTransform.GetRotation();
    InOutViewTarget.POV.FOV = CameraState.FOV;
    InOutViewTarget.POV.NearClip = CameraState.NearZ;
    InOutViewTarget.POV.FarClip = CameraState.FarZ;
    InOutViewTarget.POV.bOrthographic = CameraState.bIsOrthogonal;

    const float AspectRatio = CameraState.AspectRatio > MathUtil::SmallNumber
        ? CameraState.AspectRatio
        : 1.0f;
    // Engine stores ortho width, while minimal view uses ortho height.
    InOutViewTarget.POV.OrthoHeight = CameraState.OrthoWidth / AspectRatio;
    return true;
}

AActor* FPlayerCameraManager::ResolveDefaultViewTarget() const
{
    // Default order:
    // 1) First pawn that has a camera component.
    // 2) First non-pawn actor that has a camera component.
    if (OwnerWorld == nullptr || OwnerWorld->GetPersistentLevel() == nullptr)
    {
        return nullptr;
    }

    AActor* FirstCameraActor = nullptr;
    for (AActor* Actor : OwnerWorld->GetPersistentLevel()->GetActors())
    {
        if (!IsValidViewActor(Actor))
        {
            continue;
        }

        if (APawn* Pawn = Cast<APawn>(Actor))
        {
            if (Pawn->GetCameraComponent() != nullptr)
            {
                return Pawn;
            }
        }

        if (FirstCameraActor == nullptr && FindCameraComponent(Actor) != nullptr)
        {
            FirstCameraActor = Actor;
        }
    }

    return FirstCameraActor;
}

AActor* FPlayerCameraManager::ResolveActiveViewTarget(AActor* RequestedTarget) const
{
    // If requested target is invalid, fail over to default camera source.
    if (FindCameraComponent(RequestedTarget) != nullptr)
    {
        return RequestedTarget;
    }

    return ResolveDefaultViewTarget();
}

FMinimalViewInfo FPlayerCameraManager::BlendPOV(const FMinimalViewInfo& From, const FMinimalViewInfo& To, float Alpha) const
{
    // Blend only view data; never write back into source actor components.
    FMinimalViewInfo BlendedPOV;
    BlendedPOV.Location = FVector::Lerp(From.Location, To.Location, Alpha);
    BlendedPOV.Rotation = FQuat::Slerp(From.Rotation, To.Rotation, Alpha);
    BlendedPOV.FOV = From.FOV + (To.FOV - From.FOV) * Alpha;
    BlendedPOV.NearClip = From.NearClip + (To.NearClip - From.NearClip) * Alpha;
    BlendedPOV.FarClip = From.FarClip + (To.FarClip - From.FarClip) * Alpha;
    BlendedPOV.bOrthographic = (Alpha >= 1.0f) ? To.bOrthographic : From.bOrthographic;
    BlendedPOV.OrthoHeight = From.OrthoHeight + (To.OrthoHeight - From.OrthoHeight) * Alpha;
    return BlendedPOV;
}
