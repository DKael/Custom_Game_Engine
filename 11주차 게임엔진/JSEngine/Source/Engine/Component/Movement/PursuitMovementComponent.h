#pragma once
#include "MovementComponent.h"
#include "Generated/PursuitMovementComponent.generated.h"

class FViewportCamera;

// Not intended for complex, non-ghost actors
UCLASS()
class UPursuitMovementComponent : public UMovementComponent {
public:
	GENERATED_BODY()

	// Overrides
	void				BeginPlay() override;
	void				TickComponent(float DeltaTime) override;
    void				GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
    void				PostDuplicate(UObject* Original) override;
	float				GetMaxSpeed() const override { return 0; };


	// Direction / Rotation
	bool				IsFacingTargetDir() const { return bFaceTargetDir; }
	void				ShouldFaceTargetDir(bool InBool) { bFaceTargetDir = InBool; }


	// Pursuit Logic
	void				SetPursuitTarget(FViewportCamera* InTarget);	// Change this to USceneComponent later on
	void				ClearTarget();
	bool				IsInPursuit() const;

	// TODO: Pursuit Forfeit/Resume logic

private:
	void UpdateTargetLoc();
	void UpdateLerp(float DeltaTime);
	void FaceTargetDir(float DeltaTime);

private:
	FViewportCamera* Target = nullptr;

	FVector CurrentPoint;
	FVector TargetPoint;

	float Elapsed					= 0.f;
	UPROPERTY(EditAnywhere, DisplayName="Pursuit Interval", Min=0.01, Max=5.0, Speed=0.01)
	float UpdateLerpInterval		= 2.0f;
	UPROPERTY(EditAnywhere, DisplayName="Detection Radius", Min=0.01, Max=4096.0, Speed=0.01)
	float DetectionRadius			= 20.f;
	UPROPERTY(EditAnywhere, DisplayName="Pursuit Speed", Min=0.01, Max=100.0, Speed=0.01)
	float PursuitSpeed				= 1.f;
    float TargetPitch				= 0.f;
    float TargetYaw					= 0.f;

	bool bIsActive					= true;
	UPROPERTY(EditAnywhere, DisplayName="Orient To Target")
	bool bFaceTargetDir				= true;
	bool bAutoTargetPerspCamera		= true;		// If no target is set, default to the primary perspective camera on BeginPlay
};
