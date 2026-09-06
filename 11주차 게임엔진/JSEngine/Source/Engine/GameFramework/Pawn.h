#pragma once

#include "GameFramework/AActor.h"
#include "Generated/Pawn.generated.h"

class APlayerController;
struct FInputActionState;

UCLASS()
class APawn : public AActor
{
public:
	GENERATED_BODY()

	APlayerController* GetController() const { return Controller; }
	void PossessedBy(APlayerController* NewController);
	void UnPossessed();

	virtual void OnInputAction(const FInputActionState& Action);

private:
	APlayerController* Controller = nullptr;
};
