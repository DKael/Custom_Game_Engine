#include "Animation/AnimNotifyLog.h"

#include "Animation/AnimData/AnimSequence.h"
#include "Component/SkeletalMeshComponent.h"
#include "Core/Logging/Log.h"

namespace
{
    FString GetNotifyEventLabel(const FAnimNotifyEvent& NotifyEvent)
    {
        if (NotifyEvent.Name.IsValid())
        {
            return NotifyEvent.Name.ToString();
        }

        return "(UnnamedNotify)";
    }

    FString GetAnimationLabel(const UAnimSequence* Animation)
    {
        return Animation ? Animation->GetName() : "(NullAnimation)";
    }

    FString GetComponentLabel(const USkeletalMeshComponent* MeshComponent)
    {
        return MeshComponent ? MeshComponent->GetName() : "(NullMeshComponent)";
    }

    FString GetPayloadLabel(const FAnimNotifyEvent& NotifyEvent)
    {
        return NotifyEvent.Payload.empty() ? FString("(EmptyPayload)") : NotifyEvent.Payload;
    }
}

void UAnimNotifyLog::Notify(USkeletalMeshComponent* MeshComponent, UAnimSequence* Animation, const FAnimNotifyEvent& NotifyEvent)
{
    UE_LOG_WARNING(
        "[AnimNotifyLog] Notify=%s | Animation=%s | Component=%s | Time=%.3f | Payload=%s",
        GetNotifyEventLabel(NotifyEvent).c_str(),
        GetAnimationLabel(Animation).c_str(),
        GetComponentLabel(MeshComponent).c_str(),
        NotifyEvent.Time,
        GetPayloadLabel(NotifyEvent).c_str());
}

void UAnimNotifyStateLog::NotifyBegin(USkeletalMeshComponent* MeshComponent, UAnimSequence* Animation, const FAnimNotifyEvent& NotifyEvent)
{
    UE_LOG_WARNING(
        "[AnimNotifyStateLog] Begin=%s | Animation=%s | Component=%s | Time=%.3f | Duration=%.3f | Payload=%s",
        GetNotifyEventLabel(NotifyEvent).c_str(),
        GetAnimationLabel(Animation).c_str(),
        GetComponentLabel(MeshComponent).c_str(),
        NotifyEvent.Time,
        NotifyEvent.Duration,
        GetPayloadLabel(NotifyEvent).c_str());
}

void UAnimNotifyStateLog::NotifyTick(USkeletalMeshComponent* MeshComponent, UAnimSequence* Animation, const FAnimNotifyEvent& NotifyEvent, float DeltaTime)
{
    UE_LOG_WARNING(
        "[AnimNotifyStateLog] Tick=%s | Animation=%s | Component=%s | DeltaTime=%.3f | Payload=%s",
        GetNotifyEventLabel(NotifyEvent).c_str(),
        GetAnimationLabel(Animation).c_str(),
        GetComponentLabel(MeshComponent).c_str(),
        DeltaTime,
        GetPayloadLabel(NotifyEvent).c_str());
}

void UAnimNotifyStateLog::NotifyEnd(USkeletalMeshComponent* MeshComponent, UAnimSequence* Animation, const FAnimNotifyEvent& NotifyEvent)
{
    UE_LOG_WARNING(
        "[AnimNotifyStateLog] End=%s | Animation=%s | Component=%s | Time=%.3f | Payload=%s",
        GetNotifyEventLabel(NotifyEvent).c_str(),
        GetAnimationLabel(Animation).c_str(),
        GetComponentLabel(MeshComponent).c_str(),
        NotifyEvent.Time,
        GetPayloadLabel(NotifyEvent).c_str());
}
