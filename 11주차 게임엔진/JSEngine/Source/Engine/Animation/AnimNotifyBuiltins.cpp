#include "Animation/AnimNotifyBuiltins.h"

#include "Animation/AnimData/AnimSequence.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimNotifyPayloadParser.h"
#include "Animation/AnimNotifyPayloadViews.h"
#include "Animation/AnimNotifySemanticFieldNames.h"
#include "Component/PrimitiveComponent.h"
#include "Component/SkeletalMeshComponent.h"
#include "Core/Logging/Log.h"
#include "Engine/Geometry/Ray.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Render/Resource/Material.h"
#include "Runtime/Engine.h"

namespace
{
    constexpr const char* AttackIdTagPrefix = "AttackId:";

    // Runtime-only payload assembly structs. These stay local to built-ins
    // because they exist to feed execution helpers, not to represent shared
    // semantic/editor data models.
    struct FGameplayEventPayload
    {
        FString EventName;
        FString PayloadValue;
    };

    struct FGameplayEventWindowPayload
    {
        FString EventName;
        FString EndEventName;
        FString PayloadValue;
    };

    struct FFootstepSurfacePayload
    {
        FString EventName;
        FName SocketName = FName::None;
        float TraceDistance = 25.0f;
        FString PayloadValue;
    };

    struct FSpawnDecalPayload
    {
        FString Decal;
        FName SocketName = FName::None;
        float TraceDistance = 50.0f;
        float Size = 1.0f;
        float Lifetime = 0.0f;
        bool bAlignToHitNormal = true;
    };

    struct FAttackWindowPayload
    {
        FString ComponentName;
        FString AttackId;
    };

    struct FAttackWindowAcquireResult
    {
        AActor* OwnerActor = nullptr;
        UPrimitiveComponent* PrimitiveComponent = nullptr;
        FString AttackTag;
        FString AttackTagStateKey;
        bool bWindowAcquired = false;
        bool bAttackTagAcquired = false;
    };

    struct FAttackWindowComponentState
    {
        int32 RefCount = 0;
        bool bOriginalGenerateOverlapEvents = false;
    };

    struct FAttackWindowTagState
    {
        int32 RefCount = 0;
        bool bOwnedByNotifySystem = false;
    };

    TMap<UPrimitiveComponent*, FAttackWindowComponentState> GAttackWindowComponentStates;
    TMap<FString, FAttackWindowTagState> GAttackWindowTagStates;

    bool IsLiveObject(const UObject* Object)
    {
        return Object != nullptr && UObjectManager::Get().ContainsObject(Object);
    }

    bool IsLiveActor(const AActor* Actor)
    {
        return Actor != nullptr && UObjectManager::Get().ContainsObject(Actor) && !Actor->IsPendingKill();
    }

    FString GetNotifyLabel(const FAnimNotifyEvent& NotifyEvent)
    {
        return NotifyEvent.Name.IsValid() ? NotifyEvent.Name.ToString() : FString("(UnnamedNotify)");
    }

    AActor* ResolveOwnerActor(USkeletalMeshComponent* MeshComponent)
    {
        AActor* OwnerActor = MeshComponent ? MeshComponent->GetOwner() : nullptr;
        return IsLiveActor(OwnerActor) ? OwnerActor : nullptr;
    }

    FVector ResolveAttachTargetLocation(USkeletalMeshComponent* MeshComponent, const FName& AttachTargetName)
    {
        // Runtime attach-target resolution follows the skinned-mesh component's
        // HasSocket/GetSocketTransform semantics, which already accept both
        // authored socket names and bone names. Missing targets still fall back
        // to the component world location exactly as before.
        if (MeshComponent && AttachTargetName != FName::None && MeshComponent->HasSocket(AttachTargetName))
        {
            return MeshComponent->GetSocketTransform(AttachTargetName).GetLocation();
        }

        return MeshComponent ? MeshComponent->GetWorldLocation() : FVector::ZeroVector;
    }

    bool TryTraceDownwardSurfaceHit(
        USkeletalMeshComponent* MeshComponent,
        const FVector& StartLocation,
        float TraceDistance,
        FHitResult& OutHit)
    {
        // Surface/decal notifies trace downward from an authored attach target,
        // but they must preserve the old fallback behavior when tracing is not
        // possible: the caller keeps using StartLocation instead of inventing a
        // different default origin here.
        OutHit.Reset();

        if (!MeshComponent || TraceDistance <= 0.0f)
        {
            return false;
        }

        AActor* OwnerActor = ResolveOwnerActor(MeshComponent);
        UWorld* World = OwnerActor ? OwnerActor->GetFocusedWorld() : nullptr;
        if (!World)
        {
            return false;
        }

        const FRay DownwardRay(StartLocation, FVector(0.0f, 0.0f, -1.0f));
        bool bFoundHit = false;
        float ClosestDistance = TraceDistance;

        for (AActor* Actor : World->GetActors())
        {
            if (!IsLiveActor(Actor) || Actor == OwnerActor || !Actor->IsVisible())
            {
                continue;
            }

            for (UPrimitiveComponent* PrimitiveComponent : Actor->GetPrimitiveComponents())
            {
                if (!IsLiveObject(PrimitiveComponent))
                {
                    continue;
                }

                FHitResult CandidateHit;
                if (!PrimitiveComponent->Raycast(DownwardRay, CandidateHit) || !CandidateHit.IsValid())
                {
                    continue;
                }

                if (CandidateHit.Distance < 0.0f || CandidateHit.Distance > ClosestDistance)
                {
                    continue;
                }

                ClosestDistance = CandidateHit.Distance;
                OutHit = CandidateHit;
                bFoundHit = true;
            }
        }

        return bFoundHit;
    }

    FString ResolveFootstepSurfaceName(const FHitResult& HitResult)
    {
        UPrimitiveComponent* PrimitiveComponent = HitResult.HitComponent;
        if (!IsLiveObject(PrimitiveComponent))
        {
            return "Default";
        }

        if (PrimitiveComponent->GetNumMaterials() > 0)
        {
            if (UMaterialInterface* Material = PrimitiveComponent->GetMaterial(0))
            {
                if (IsLiveObject(Material))
                {
                    const FString& MaterialName = Material->GetName();
                    if (!MaterialName.empty())
                    {
                        return MaterialName;
                    }
                }
            }
        }

        const FString ComponentName = PrimitiveComponent->GetName();
        if (!ComponentName.empty())
        {
            return ComponentName;
        }

        AActor* OwnerActor = PrimitiveComponent->GetOwner();
        if (IsLiveActor(OwnerActor))
        {
            const FString ActorName = OwnerActor->GetName();
            if (!ActorName.empty())
            {
                return ActorName;
            }
        }

        return "Default";
    }

    FVector ResolveDecalSurfaceNormal(const FHitResult& HitResult, bool bAlignToHitNormal)
    {
        if (bAlignToHitNormal && HitResult.IsValid())
        {
            FVector SurfaceNormal = HitResult.Normal;
            if (SurfaceNormal.NormalizeSafe())
            {
                return SurfaceNormal;
            }
        }

        return FVector::UpVector;
    }

    // Audio helpers
    void DispatchPlaySfxNotify(USkeletalMeshComponent* MeshComponent, const FPlaySfxPayloadView& PayloadView)
    {
        const float Volume = std::max(0.0f, PayloadView.VolumeMultiplier);
        if (PayloadView.bSpatialized)
        {
            GEngine->GetAudioSystem().PlaySoundCue(
                PayloadView.SoundCue,
                false,
                true,
                ResolveAttachTargetLocation(MeshComponent, PayloadView.SocketName),
                Volume);
            return;
        }

        GEngine->GetAudioSystem().PlaySFX(PayloadView.SoundCue, Volume);
    }

    FAudioHandle BeginLoopingSfxPlayback(USkeletalMeshComponent* MeshComponent, const FPlaySfxPayloadView& PayloadView)
    {
        const float Volume = std::max(0.0f, PayloadView.VolumeMultiplier);
        return GEngine->GetAudioSystem().PlaySoundCue(
            PayloadView.SoundCue,
            true,
            PayloadView.bSpatialized,
            ResolveAttachTargetLocation(MeshComponent, PayloadView.SocketName),
            Volume);
    }

    void UpdateLoopingSfxPlaybackPosition(
        FAudioHandle ActiveHandle,
        USkeletalMeshComponent* MeshComponent,
        const FPlaySfxPayloadView& PayloadView)
    {
        GEngine->GetAudioSystem().SetSoundPosition(
            ActiveHandle,
            ResolveAttachTargetLocation(MeshComponent, PayloadView.SocketName));
    }

    void DispatchCameraShakeNotify(
        UAnimInstance* AnimInstance,
        UAnimSequence* Animation,
        const FAnimNotifyEvent& NotifyEvent,
        const FCameraShakePayloadView& PayloadView)
    {
        AnimInstance->DispatchCameraShakeAnimNotify(Animation, NotifyEvent, PayloadView.Shake, PayloadView.Scale);
    }

    // Gameplay event helpers
    FGameplayEventPayload BuildGameplayEventPayload(const FAnimNotifyPayloadParser& Payload)
    {
        FGameplayEventPayload Result;
        Result.EventName = Payload.GetStringAny(
            AnimNotifySemanticFieldNames::GetLookupKeys(EAnimNotifySemanticFieldId::EventName));
        Result.PayloadValue = Payload.GetStringAny(
            AnimNotifySemanticFieldNames::GetLookupKeys(EAnimNotifySemanticFieldId::PayloadValue));
        return Result;
    }

    FGameplayEventWindowPayload BuildGameplayEventWindowPayload(const FAnimNotifyPayloadParser& Payload)
    {
        FGameplayEventWindowPayload Result;
        Result.EventName = Payload.GetStringAny(
            AnimNotifySemanticFieldNames::GetLookupKeys(EAnimNotifySemanticFieldId::EventName));
        Result.EndEventName = Payload.GetStringAny(
            AnimNotifySemanticFieldNames::GetLookupKeys(EAnimNotifySemanticFieldId::EndEventName));
        Result.PayloadValue = Payload.GetStringAny(
            AnimNotifySemanticFieldNames::GetLookupKeys(EAnimNotifySemanticFieldId::PayloadValue));
        return Result;
    }

    FString ResolveGameplayEventWindowDispatchName(
        const FGameplayEventWindowPayload& Payload,
        bool bUseEndEventName)
    {
        if (bUseEndEventName && !Payload.EndEventName.empty())
        {
            return Payload.EndEventName;
        }

        return Payload.EventName;
    }

    void DispatchGameplayEventNotify(
        UAnimInstance* AnimInstance,
        UAnimSequence* Animation,
        const FAnimNotifyEvent& NotifyEvent,
        const FGameplayEventPayload& Payload)
    {
        AnimInstance->DispatchGameplayAnimNotifyEvent(
            Animation,
            NotifyEvent,
            Payload.EventName,
            Payload.PayloadValue);
    }

    bool DispatchGameplayEventWindowPhase(
        USkeletalMeshComponent* MeshComponent,
        UAnimSequence* Animation,
        const FAnimNotifyEvent& NotifyEvent,
        bool bUseEndEventName)
    {
        if (!MeshComponent)
        {
            return false;
        }

        UAnimInstance* AnimInstance = MeshComponent->GetAnimInstance();
        if (!AnimInstance)
        {
            return false;
        }

        const FAnimNotifyPayloadParser PayloadParser(NotifyEvent.Payload);
        const FGameplayEventWindowPayload Payload = BuildGameplayEventWindowPayload(PayloadParser);
        if (Payload.EventName.empty())
        {
            UE_LOG_WARNING(
                "[AnimNotifyBuiltins] GameplayEventWindow notify missing EventName payload | Notify=%s",
                GetNotifyLabel(NotifyEvent).c_str());
            return false;
        }

        AnimInstance->DispatchGameplayAnimNotifyEvent(
            Animation,
            NotifyEvent,
            ResolveGameplayEventWindowDispatchName(Payload, bUseEndEventName),
            Payload.PayloadValue);
        return true;
    }

    // Effect / trace helpers
    FFootstepSurfacePayload BuildFootstepSurfacePayload(const FAnimNotifyPayloadParser& Payload)
    {
        FFootstepSurfacePayload Result;
        Result.EventName = Payload.GetStringAny(
            AnimNotifySemanticFieldNames::GetLookupKeys(EAnimNotifySemanticFieldId::EventName));
        Result.SocketName = Payload.GetNameAny(
            AnimNotifySemanticFieldNames::GetLookupKeys(EAnimNotifySemanticFieldId::SocketName));
        Result.TraceDistance = Payload.GetFloatAny(
            AnimNotifySemanticFieldNames::GetLookupKeys(EAnimNotifySemanticFieldId::TraceDistance),
            25.0f);
        Result.PayloadValue = Payload.GetStringAny(
            AnimNotifySemanticFieldNames::GetLookupKeys(EAnimNotifySemanticFieldId::PayloadValue));
        return Result;
    }

    void DispatchFootstepSurfaceNotify(
        UAnimInstance* AnimInstance,
        USkeletalMeshComponent* MeshComponent,
        UAnimSequence* Animation,
        const FAnimNotifyEvent& NotifyEvent,
        const FFootstepSurfacePayload& Payload)
    {
        const FVector TraceStartLocation = ResolveAttachTargetLocation(MeshComponent, Payload.SocketName);
        FHitResult SurfaceHit;
        const bool bHasSurfaceHit = TryTraceDownwardSurfaceHit(
            MeshComponent,
            TraceStartLocation,
            Payload.TraceDistance,
            SurfaceHit);
        // Misses intentionally degrade to a generic "Default" surface and the
        // original trace start location so existing content still fires an event
        // even when preview/runtime geometry does not provide a hit.
        const FString SurfaceName = bHasSurfaceHit ? ResolveFootstepSurfaceName(SurfaceHit) : FString("Default");
        const FVector HitLocation = bHasSurfaceHit ? SurfaceHit.Location : TraceStartLocation;

        AnimInstance->DispatchFootstepSurfaceAnimNotify(
            Animation,
            NotifyEvent,
            Payload.EventName,
            SurfaceName,
            Payload.PayloadValue,
            HitLocation);
    }

    void DispatchPlayVfxNotify(
        UAnimInstance* AnimInstance,
        USkeletalMeshComponent* MeshComponent,
        UAnimSequence* Animation,
        const FAnimNotifyEvent& NotifyEvent,
        const FPlayVfxPayloadView& PayloadView)
    {
        AnimInstance->DispatchPlayVFXAnimNotify(
            Animation,
            NotifyEvent,
            PayloadView.Effect,
            ResolveAttachTargetLocation(MeshComponent, PayloadView.SocketName),
            PayloadView.SocketName,
            PayloadView.bAttached,
            PayloadView.Scale);
    }

    FSpawnDecalPayload BuildSpawnDecalPayload(const FAnimNotifyPayloadParser& Payload)
    {
        FSpawnDecalPayload Result;
        Result.Decal = Payload.GetStringAny(
            AnimNotifySemanticFieldNames::GetLookupKeys(EAnimNotifySemanticFieldId::Decal));
        Result.SocketName = Payload.GetNameAny(
            AnimNotifySemanticFieldNames::GetLookupKeys(EAnimNotifySemanticFieldId::SocketName));
        Result.TraceDistance = Payload.GetFloatAny(
            AnimNotifySemanticFieldNames::GetLookupKeys(EAnimNotifySemanticFieldId::TraceDistance),
            50.0f);
        Result.Size = Payload.GetFloatAny(
            AnimNotifySemanticFieldNames::GetLookupKeys(EAnimNotifySemanticFieldId::Size),
            1.0f);
        Result.Lifetime = Payload.GetFloatAny(
            AnimNotifySemanticFieldNames::GetLookupKeys(EAnimNotifySemanticFieldId::Lifetime),
            0.0f);
        Result.bAlignToHitNormal = Payload.GetBoolAny(
            AnimNotifySemanticFieldNames::GetLookupKeys(EAnimNotifySemanticFieldId::AlignToHitNormal),
            true);
        return Result;
    }

    void DispatchSpawnDecalNotify(
        UAnimInstance* AnimInstance,
        USkeletalMeshComponent* MeshComponent,
        UAnimSequence* Animation,
        const FAnimNotifyEvent& NotifyEvent,
        const FSpawnDecalPayload& Payload)
    {
        const FVector SpawnOrigin = ResolveAttachTargetLocation(MeshComponent, Payload.SocketName);
        FHitResult SurfaceHit;
        const bool bHasSurfaceHit = TryTraceDownwardSurfaceHit(
            MeshComponent,
            SpawnOrigin,
            Payload.TraceDistance,
            SurfaceHit);
        // Decal spawning follows the same fallback rule as footstep tracing:
        // keep the authored origin when no surface is found, and only use the
        // hit normal when the trace produced a valid surface.
        const FVector SpawnLocation = bHasSurfaceHit ? SurfaceHit.Location : SpawnOrigin;
        const FVector SpawnNormal = bHasSurfaceHit
            ? ResolveDecalSurfaceNormal(SurfaceHit, Payload.bAlignToHitNormal)
            : FVector::UpVector;

        AnimInstance->DispatchSpawnDecalAnimNotify(
            Animation,
            NotifyEvent,
            Payload.Decal,
            SpawnLocation,
            SpawnNormal,
            Payload.SocketName,
            Payload.Size,
            Payload.Lifetime,
            Payload.bAlignToHitNormal);
    }

    // Attack window helpers
    FAttackWindowPayload BuildAttackWindowPayload(const FAnimNotifyPayloadParser& Payload)
    {
        FAttackWindowPayload Result;
        Result.ComponentName = Payload.GetStringAny(
            AnimNotifySemanticFieldNames::GetLookupKeys(EAnimNotifySemanticFieldId::ComponentName));
        Result.AttackId = Payload.GetStringAny(
            AnimNotifySemanticFieldNames::GetLookupKeys(EAnimNotifySemanticFieldId::AttackId));
        return Result;
    }

    UPrimitiveComponent* FindPrimitiveComponentByName(AActor* OwnerActor, const FString& ComponentName)
    {
        if (!IsLiveActor(OwnerActor) || ComponentName.empty())
        {
            return nullptr;
        }

        for (UActorComponent* Component : OwnerActor->GetComponents())
        {
            UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(Component);
            if (!PrimitiveComponent || !IsLiveObject(PrimitiveComponent))
            {
                continue;
            }

            if (PrimitiveComponent->GetName() == ComponentName)
            {
                return PrimitiveComponent;
            }
        }

        return nullptr;
    }

    FString BuildAttackIdTag(const FString& AttackId)
    {
        return AttackId.empty() ? FString() : FString(AttackIdTagPrefix) + AttackId;
    }

    FString BuildAttackTagStateKey(const AActor* OwnerActor, const FString& AttackTag)
    {
        return std::to_string(OwnerActor ? OwnerActor->GetUUID() : 0) + "|" + AttackTag;
    }

    bool AcquireAttackWindow(UPrimitiveComponent* PrimitiveComponent)
    {
        if (!IsLiveObject(PrimitiveComponent))
        {
            return false;
        }

        FAttackWindowComponentState& State = GAttackWindowComponentStates[PrimitiveComponent];
        if (State.RefCount == 0)
        {
            // AttackWindow temporarily forces overlap generation on while at
            // least one notify window is active, but the original component
            // setting must be restored exactly when the last reference leaves.
            State.bOriginalGenerateOverlapEvents = PrimitiveComponent->ShouldGenerateOverlapEvents();
            PrimitiveComponent->SetGenerateOverlapEvents(true);
        }

        ++State.RefCount;
        return true;
    }

    void ReleaseAttackWindow(UPrimitiveComponent* PrimitiveComponent)
    {
        const auto Iterator = GAttackWindowComponentStates.find(PrimitiveComponent);
        if (Iterator == GAttackWindowComponentStates.end())
        {
            return;
        }

        FAttackWindowComponentState& State = Iterator->second;
        if (State.RefCount > 0)
        {
            --State.RefCount;
        }

        if (State.RefCount <= 0)
        {
            if (IsLiveObject(PrimitiveComponent))
            {
                PrimitiveComponent->SetGenerateOverlapEvents(State.bOriginalGenerateOverlapEvents);
            }
            GAttackWindowComponentStates.erase(Iterator);
        }
    }

    FString AcquireAttackTagStateKey(AActor* OwnerActor, const FString& AttackTag)
    {
        if (!IsLiveActor(OwnerActor) || AttackTag.empty())
        {
            return FString();
        }

        const FString StateKey = BuildAttackTagStateKey(OwnerActor, AttackTag);
        FAttackWindowTagState& State = GAttackWindowTagStates[StateKey];
        if (State.RefCount == 0)
        {
            // Only remove the tag again if this notify system added it first.
            // Pre-authored actor tags must survive the notify lifetime intact.
            State.bOwnedByNotifySystem = !OwnerActor->HasTag(AttackTag);
            if (State.bOwnedByNotifySystem)
            {
                OwnerActor->AddTag(AttackTag);
            }
        }

        ++State.RefCount;
        return StateKey;
    }

    void ReleaseAttackTag(AActor* OwnerActor, const FString& StateKey, const FString& AttackTag)
    {
        if (StateKey.empty() || AttackTag.empty())
        {
            return;
        }

        const auto Iterator = GAttackWindowTagStates.find(StateKey);
        if (Iterator == GAttackWindowTagStates.end())
        {
            return;
        }

        FAttackWindowTagState& State = Iterator->second;
        if (State.RefCount > 0)
        {
            --State.RefCount;
        }

        if (State.RefCount <= 0)
        {
            if (State.bOwnedByNotifySystem && IsLiveActor(OwnerActor))
            {
                OwnerActor->RemoveTag(AttackTag);
            }
            GAttackWindowTagStates.erase(Iterator);
        }
    }

    FAttackWindowAcquireResult BeginAttackWindowNotify(
        USkeletalMeshComponent* MeshComponent,
        const FAnimNotifyEvent& NotifyEvent)
    {
        FAttackWindowAcquireResult Result;
        Result.OwnerActor = ResolveOwnerActor(MeshComponent);
        if (!Result.OwnerActor)
        {
            UE_LOG_WARNING(
                "[AnimNotifyBuiltins] AttackWindow notify missing owner actor | Notify=%s",
                GetNotifyLabel(NotifyEvent).c_str());
            return Result;
        }

        const FAnimNotifyPayloadParser PayloadParser(NotifyEvent.Payload);
        const FAttackWindowPayload Payload = BuildAttackWindowPayload(PayloadParser);
        if (Payload.ComponentName.empty())
        {
            UE_LOG_WARNING(
                "[AnimNotifyBuiltins] AttackWindow notify missing ComponentName payload | Notify=%s",
                GetNotifyLabel(NotifyEvent).c_str());
            Result.OwnerActor = nullptr;
            return Result;
        }

        Result.PrimitiveComponent = FindPrimitiveComponentByName(Result.OwnerActor, Payload.ComponentName);
        if (!Result.PrimitiveComponent)
        {
            UE_LOG_WARNING(
                "[AnimNotifyBuiltins] AttackWindow target component not found or not primitive | Notify=%s | Component=%s",
                GetNotifyLabel(NotifyEvent).c_str(),
                Payload.ComponentName.c_str());
            Result.OwnerActor = nullptr;
            return Result;
        }

        // Acquire component overlap state before optional attack-tag state so
        // NotifyEnd can release them in the exact reverse ownership order.
        Result.bWindowAcquired = AcquireAttackWindow(Result.PrimitiveComponent);
        Result.AttackTag = BuildAttackIdTag(Payload.AttackId);
        if (!Result.AttackTag.empty())
        {
            Result.AttackTagStateKey = AcquireAttackTagStateKey(Result.OwnerActor, Result.AttackTag);
            Result.bAttackTagAcquired = !Result.AttackTagStateKey.empty();
        }

        return Result;
    }
}

void UAnimNotify_PlaySFX::Notify(
    USkeletalMeshComponent* MeshComponent,
    UAnimSequence* Animation,
    const FAnimNotifyEvent& NotifyEvent)
{
    (void)Animation;

    if (!GEngine)
    {
        return;
    }

    const FAnimNotifyPayloadParser Payload(NotifyEvent.Payload);
    const FPlaySfxPayloadView PayloadView = BuildPlaySfxPayloadView(Payload);
    if (PayloadView.SoundCue.empty())
    {
        UE_LOG_WARNING("[AnimNotifyBuiltins] PlaySFX notify missing SoundCue payload | Notify=%s", GetNotifyLabel(NotifyEvent).c_str());
        return;
    }

    DispatchPlaySfxNotify(MeshComponent, PayloadView);
}

void UAnimNotify_GameplayEvent::Notify(
    USkeletalMeshComponent* MeshComponent,
    UAnimSequence* Animation,
    const FAnimNotifyEvent& NotifyEvent)
{
    if (!MeshComponent)
    {
        return;
    }

    UAnimInstance* AnimInstance = MeshComponent->GetAnimInstance();
    if (!AnimInstance)
    {
        return;
    }

    const FAnimNotifyPayloadParser PayloadParser(NotifyEvent.Payload);
    const FGameplayEventPayload Payload = BuildGameplayEventPayload(PayloadParser);
    if (Payload.EventName.empty())
    {
        UE_LOG_WARNING("[AnimNotifyBuiltins] GameplayEvent notify missing EventName payload | Notify=%s", GetNotifyLabel(NotifyEvent).c_str());
        return;
    }

    DispatchGameplayEventNotify(AnimInstance, Animation, NotifyEvent, Payload);
}

void UAnimNotify_CameraShake::Notify(
    USkeletalMeshComponent* MeshComponent,
    UAnimSequence* Animation,
    const FAnimNotifyEvent& NotifyEvent)
{
    if (!MeshComponent)
    {
        return;
    }

    UAnimInstance* AnimInstance = MeshComponent->GetAnimInstance();
    if (!AnimInstance)
    {
        return;
    }

    const FAnimNotifyPayloadParser PayloadParser(NotifyEvent.Payload);
    const FCameraShakePayloadView PayloadView = BuildCameraShakePayloadView(PayloadParser);
    if (PayloadView.Shake.empty())
    {
        UE_LOG_WARNING("[AnimNotifyBuiltins] CameraShake notify missing Shake payload | Notify=%s", GetNotifyLabel(NotifyEvent).c_str());
        return;
    }

    DispatchCameraShakeNotify(AnimInstance, Animation, NotifyEvent, PayloadView);
}

void UAnimNotify_FootstepSurfaceEvent::Notify(
    USkeletalMeshComponent* MeshComponent,
    UAnimSequence* Animation,
    const FAnimNotifyEvent& NotifyEvent)
{
    if (!MeshComponent)
    {
        return;
    }

    UAnimInstance* AnimInstance = MeshComponent->GetAnimInstance();
    if (!AnimInstance)
    {
        return;
    }

    const FAnimNotifyPayloadParser PayloadParser(NotifyEvent.Payload);
    const FFootstepSurfacePayload Payload = BuildFootstepSurfacePayload(PayloadParser);
    if (Payload.EventName.empty())
    {
        UE_LOG_WARNING("[AnimNotifyBuiltins] FootstepSurfaceEvent notify missing EventName payload | Notify=%s", GetNotifyLabel(NotifyEvent).c_str());
        return;
    }

    if (Payload.SocketName == FName::None)
    {
        UE_LOG_WARNING("[AnimNotifyBuiltins] FootstepSurfaceEvent notify missing SocketName payload | Notify=%s", GetNotifyLabel(NotifyEvent).c_str());
        return;
    }

    DispatchFootstepSurfaceNotify(AnimInstance, MeshComponent, Animation, NotifyEvent, Payload);
}

void UAnimNotify_PlayVFX::Notify(
    USkeletalMeshComponent* MeshComponent,
    UAnimSequence* Animation,
    const FAnimNotifyEvent& NotifyEvent)
{
    if (!MeshComponent)
    {
        return;
    }

    UAnimInstance* AnimInstance = MeshComponent->GetAnimInstance();
    if (!AnimInstance)
    {
        return;
    }

    const FAnimNotifyPayloadParser PayloadParser(NotifyEvent.Payload);
    const FPlayVfxPayloadView PayloadView = BuildPlayVfxPayloadView(PayloadParser);
    if (PayloadView.Effect.empty())
    {
        UE_LOG_WARNING("[AnimNotifyBuiltins] PlayVFX notify missing Effect payload | Notify=%s", GetNotifyLabel(NotifyEvent).c_str());
        return;
    }

    DispatchPlayVfxNotify(AnimInstance, MeshComponent, Animation, NotifyEvent, PayloadView);
}

void UAnimNotify_SpawnDecal::Notify(
    USkeletalMeshComponent* MeshComponent,
    UAnimSequence* Animation,
    const FAnimNotifyEvent& NotifyEvent)
{
    if (!MeshComponent)
    {
        return;
    }

    UAnimInstance* AnimInstance = MeshComponent->GetAnimInstance();
    if (!AnimInstance)
    {
        return;
    }

    const FAnimNotifyPayloadParser PayloadParser(NotifyEvent.Payload);
    const FSpawnDecalPayload Payload = BuildSpawnDecalPayload(PayloadParser);
    if (Payload.Decal.empty())
    {
        UE_LOG_WARNING("[AnimNotifyBuiltins] SpawnDecal notify missing Decal payload | Notify=%s", GetNotifyLabel(NotifyEvent).c_str());
        return;
    }

    DispatchSpawnDecalNotify(AnimInstance, MeshComponent, Animation, NotifyEvent, Payload);
}

void UAnimNotifyState_GameplayEventWindow::NotifyBegin(
    USkeletalMeshComponent* MeshComponent,
    UAnimSequence* Animation,
    const FAnimNotifyEvent& NotifyEvent)
{
    DispatchGameplayEventWindowPhase(MeshComponent, Animation, NotifyEvent, false);
}

void UAnimNotifyState_GameplayEventWindow::NotifyEnd(
    USkeletalMeshComponent* MeshComponent,
    UAnimSequence* Animation,
    const FAnimNotifyEvent& NotifyEvent)
{
    DispatchGameplayEventWindowPhase(MeshComponent, Animation, NotifyEvent, true);
}

UAnimNotifyState_PlayLoopingSFX::~UAnimNotifyState_PlayLoopingSFX()
{
    Cleanup();
}

void UAnimNotifyState_PlayLoopingSFX::NotifyBegin(
    USkeletalMeshComponent* MeshComponent,
    UAnimSequence* Animation,
    const FAnimNotifyEvent& NotifyEvent)
{
    (void)Animation;
    Cleanup();

    if (!GEngine)
    {
        return;
    }

    const FAnimNotifyPayloadParser Payload(NotifyEvent.Payload);
    const FPlaySfxPayloadView PayloadView = BuildPlaySfxPayloadView(Payload);
    if (PayloadView.SoundCue.empty())
    {
        UE_LOG_WARNING("[AnimNotifyBuiltins] LoopingSFX notify missing SoundCue payload | Notify=%s", GetNotifyLabel(NotifyEvent).c_str());
        return;
    }

    bSpatialized = PayloadView.bSpatialized;
    ActiveHandle = BeginLoopingSfxPlayback(MeshComponent, PayloadView);
}

void UAnimNotifyState_PlayLoopingSFX::NotifyTick(
    USkeletalMeshComponent* MeshComponent,
    UAnimSequence* Animation,
    const FAnimNotifyEvent& NotifyEvent,
    float DeltaTime)
{
    (void)Animation;
    (void)NotifyEvent;
    (void)DeltaTime;

    if (!bSpatialized || !GEngine || ActiveHandle == 0)
    {
        return;
    }

    const FAnimNotifyPayloadParser Payload(NotifyEvent.Payload);
    const FPlaySfxPayloadView PayloadView = BuildPlaySfxPayloadView(Payload);
    UpdateLoopingSfxPlaybackPosition(ActiveHandle, MeshComponent, PayloadView);
}

void UAnimNotifyState_PlayLoopingSFX::NotifyEnd(
    USkeletalMeshComponent* MeshComponent,
    UAnimSequence* Animation,
    const FAnimNotifyEvent& NotifyEvent)
{
    (void)MeshComponent;
    (void)Animation;
    (void)NotifyEvent;
    Cleanup();
}

void UAnimNotifyState_PlayLoopingSFX::Cleanup()
{
    if (GEngine && ActiveHandle != 0)
    {
        GEngine->GetAudioSystem().StopSound(ActiveHandle);
    }

    ActiveHandle = 0;
    bSpatialized = false;
}

UAnimNotifyState_AttackWindow::~UAnimNotifyState_AttackWindow()
{
    Cleanup();
}

void UAnimNotifyState_AttackWindow::NotifyBegin(
    USkeletalMeshComponent* MeshComponent,
    UAnimSequence* Animation,
    const FAnimNotifyEvent& NotifyEvent)
{
    (void)Animation;
    Cleanup();

    const FAttackWindowAcquireResult AcquireResult = BeginAttackWindowNotify(MeshComponent, NotifyEvent);
    CachedOwnerActor = AcquireResult.OwnerActor;
    CachedPrimitiveComponent = AcquireResult.PrimitiveComponent;
    ActiveAttackTag = AcquireResult.AttackTag;
    ActiveAttackTagStateKey = AcquireResult.AttackTagStateKey;
    bWindowAcquired = AcquireResult.bWindowAcquired;
    bAttackTagAcquired = AcquireResult.bAttackTagAcquired;
}

void UAnimNotifyState_AttackWindow::NotifyEnd(
    USkeletalMeshComponent* MeshComponent,
    UAnimSequence* Animation,
    const FAnimNotifyEvent& NotifyEvent)
{
    (void)MeshComponent;
    (void)Animation;
    (void)NotifyEvent;
    Cleanup();
}

void UAnimNotifyState_AttackWindow::Cleanup()
{
    if (bAttackTagAcquired && !ActiveAttackTag.empty())
    {
        ReleaseAttackTag(CachedOwnerActor, ActiveAttackTagStateKey, ActiveAttackTag);
    }

    if (bWindowAcquired)
    {
        ReleaseAttackWindow(CachedPrimitiveComponent);
    }

    CachedOwnerActor = nullptr;
    CachedPrimitiveComponent = nullptr;
    ActiveAttackTag.clear();
    ActiveAttackTagStateKey.clear();
    bWindowAcquired = false;
    bAttackTagAcquired = false;
}
