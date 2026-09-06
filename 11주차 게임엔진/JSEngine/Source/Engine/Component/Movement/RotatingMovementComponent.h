#pragma once

#include "MovementComponent.h"
#include "Generated/RotatingMovementComponent.generated.h"

UCLASS()
class URotatingMovementComponent : public UMovementComponent
{	
public:
	GENERATED_BODY()

	virtual void TickComponent(float DeltaTime) override;

	// RotationRate: X=X축 각속도, Y=Y축 각속도, Z=Z축 각속도 (deg/s)
	void SetRotationSpeed(const FVector& InRotationRate) { RotationRate = InRotationRate; }
	const FVector& GetRotationRate() const { return RotationRate; }

	void SetPivotTranslation(const FVector& InPivot) { PivotTranslation = InPivot; }
    const FVector& GetPivotTranslation() const { return PivotTranslation; }

	void SetRotationInLocalSpace(bool bInLocalSpace) { bRotationInLocalSpace = bInLocalSpace; }
    bool IsRotationInLocalSpace() const { return bRotationInLocalSpace; }

	virtual float GetMaxSpeed() const override { return 0.0f; } // 회전 컴포넌트이므로 0.0f 반환

	virtual void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;

private:
	// X=X축(Roll), Y=Y축(Pitch), Z=Z축(Yaw) 각속도 (deg/s)
	UPROPERTY(EditAnywhere, DisplayName="Rotation Rate", Min=-360.0, Max=360.0, Speed=1.0)
	FVector RotationRate = FVector(90.0f, 0.f, 0.f);
	// 오브젝트 로컬 공간 기준 피벗 오프셋 (오브젝트 → 피벗 방향)
	UPROPERTY(EditAnywhere, DisplayName="Pivot Translation", Speed=0.1)
	FVector PivotTranslation = FVector::ZeroVector;
	
	UPROPERTY(EditAnywhere, DisplayName="Local Space Rotation")
	bool bRotationInLocalSpace = true;
};
