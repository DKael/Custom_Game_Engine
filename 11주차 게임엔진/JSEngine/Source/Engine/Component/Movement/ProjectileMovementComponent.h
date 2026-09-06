#pragma once

#include "MovementComponent.h"
#include "Generated/ProjectileMovementComponent.generated.h"

UCLASS()
class UProjectileMovementComponent : public UMovementComponent
{
public:
	GENERATED_BODY()

	virtual void TickComponent(float DeltaTime) override;
	virtual void BeginPlay() override;

    void SetInitialSpeed(const float InSpeed) { InitialSpeed = InSpeed; }
    float GetInitialSpeed() const { return InitialSpeed; }

    void SetMaxSpeed(const float InSpeed) { MaxSpeed = InSpeed; }
	virtual float GetMaxSpeed() const { return MaxSpeed; }

    void SetGravityScale(const float InScale) { GravityScale = InScale; }
    float GetGravityScale() const { return GravityScale; }

    void SetRotationFollowsVelocity(bool bFollow) { bRotationFollowsVelocity = bFollow; }
    bool GetRotationFollowsVelocity() const { return bRotationFollowsVelocity; }

	virtual void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;

private:
	UPROPERTY(EditAnywhere, DisplayName="Initial Speed", Speed=1.0)
	float InitialSpeed = 5.0f;
	UPROPERTY(EditAnywhere, DisplayName="Max Speed", Speed=1.0)
	float MaxSpeed = 100.0f;
	UPROPERTY(EditAnywhere, DisplayName="Gravity Scale", Min=0.0, Max=5.0, Speed=0.01)
	float GravityScale = 0.0f;

	UPROPERTY(EditAnywhere, DisplayName="Rotation Follows Velocity")
	bool bRotationFollowsVelocity = true; // 켤 시 화살 및 로켓이 날아가는 궤적을 바라본다.
};
