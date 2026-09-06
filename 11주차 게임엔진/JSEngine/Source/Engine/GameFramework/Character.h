#pragma once
#include "Pawn.h"
#include "Generated/Character.generated.h"

class UCapsuleComponent;
class USkeletalMeshComponent;

UCLASS()
class ACharacter : public APawn
{
public:
    GENERATED_BODY()

    void InitDefaultComponents() override;

protected:
	UCapsuleComponent* CapsuleComponent = nullptr;
    USkeletalMeshComponent* MeshComponent = nullptr;
};
