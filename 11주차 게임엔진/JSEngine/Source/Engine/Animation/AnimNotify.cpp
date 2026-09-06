#include "Animation/AnimNotify.h"

FString UAnimNotify::GetNotifyName(const FAnimNotifyEvent& NotifyEvent) const
{
    return NotifyEvent.Name.IsValid() ? NotifyEvent.Name.ToString() : GetName();
}

void UAnimNotify::Notify(USkeletalMeshComponent* MeshComponent, UAnimSequence* Animation, const FAnimNotifyEvent& NotifyEvent)
{
    (void)MeshComponent;
    (void)Animation;
    (void)NotifyEvent;
}

void UAnimNotifyState::Notify(USkeletalMeshComponent* MeshComponent, UAnimSequence* Animation, const FAnimNotifyEvent& NotifyEvent)
{
    NotifyBegin(MeshComponent, Animation, NotifyEvent);
}

void UAnimNotifyState::NotifyBegin(USkeletalMeshComponent* MeshComponent, UAnimSequence* Animation, const FAnimNotifyEvent& NotifyEvent)
{
    (void)MeshComponent;
    (void)Animation;
    (void)NotifyEvent;
}

void UAnimNotifyState::NotifyTick(USkeletalMeshComponent* MeshComponent, UAnimSequence* Animation, const FAnimNotifyEvent& NotifyEvent, float DeltaTime)
{
    (void)MeshComponent;
    (void)Animation;
    (void)NotifyEvent;
    (void)DeltaTime;
}

void UAnimNotifyState::NotifyEnd(USkeletalMeshComponent* MeshComponent, UAnimSequence* Animation, const FAnimNotifyEvent& NotifyEvent)
{
    (void)MeshComponent;
    (void)Animation;
    (void)NotifyEvent;
}
