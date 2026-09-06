#pragma once

#include "Animation/AnimNotify.h"
#include "Generated/AnimNotifyLog.generated.h"

UCLASS()
class UAnimNotifyLog : public UAnimNotify
{
public:
    GENERATED_BODY()

    void Notify(USkeletalMeshComponent* MeshComponent, UAnimSequence* Animation, const FAnimNotifyEvent& NotifyEvent) override;
};

UCLASS()
class UAnimNotifyStateLog : public UAnimNotifyState
{
public:
    GENERATED_BODY()

    void NotifyBegin(USkeletalMeshComponent* MeshComponent, UAnimSequence* Animation, const FAnimNotifyEvent& NotifyEvent) override;
    void NotifyTick(USkeletalMeshComponent* MeshComponent, UAnimSequence* Animation, const FAnimNotifyEvent& NotifyEvent, float DeltaTime) override;
    void NotifyEnd(USkeletalMeshComponent* MeshComponent, UAnimSequence* Animation, const FAnimNotifyEvent& NotifyEvent) override;
};
