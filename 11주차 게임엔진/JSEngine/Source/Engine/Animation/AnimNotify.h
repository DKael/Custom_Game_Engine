#pragma once

#include "Animation/AnimData/AnimNotifyTypes.h"
#include "Object/Object.h"
#include "Generated/AnimNotify.generated.h"

class UAnimSequence;
class USkeletalMeshComponent;

UCLASS()
class UAnimNotify : public UObject
{
public:
    GENERATED_BODY()

    virtual FString GetNotifyName(const FAnimNotifyEvent& NotifyEvent) const;
    virtual void Notify(USkeletalMeshComponent* MeshComponent, UAnimSequence* Animation, const FAnimNotifyEvent& NotifyEvent);
};

UCLASS()
class UAnimNotifyState : public UAnimNotify
{
public:
    GENERATED_BODY()

    void Notify(USkeletalMeshComponent* MeshComponent, UAnimSequence* Animation, const FAnimNotifyEvent& NotifyEvent) override;
    virtual void NotifyBegin(USkeletalMeshComponent* MeshComponent, UAnimSequence* Animation, const FAnimNotifyEvent& NotifyEvent);
    virtual void NotifyTick(USkeletalMeshComponent* MeshComponent, UAnimSequence* Animation, const FAnimNotifyEvent& NotifyEvent, float DeltaTime);
    virtual void NotifyEnd(USkeletalMeshComponent* MeshComponent, UAnimSequence* Animation, const FAnimNotifyEvent& NotifyEvent);
};
