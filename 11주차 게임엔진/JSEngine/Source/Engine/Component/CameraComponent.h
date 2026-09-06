#pragma once
#include "Engine/Geometry/Ray.h"
#include "Object/ObjectFactory.h"
#include "Component/SceneComponent.h"
#include "Math/Matrix.h"
#include "Math/Utils.h"
#include "Math/Vector.h"
#include "Math/Color.h"
#include "Generated/CameraComponent.generated.h"

// 렌더 전용 구조체
USTRUCT()
struct FCameraState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, DisplayName="FOV", Min=0.1, Max=3.14, Speed=0.01, Animatable)
	float FOV = 3.14159265358979f / 3.0f;
	float AspectRatio = 16.0f / 9.0f;
	UPROPERTY(EditAnywhere, DisplayName="Near Z", SerializeName="NearClip", Min=0.01, Max=100.0, Speed=0.01)
	float NearZ = 0.1f;
	UPROPERTY(EditAnywhere, DisplayName="Far Z", SerializeName="FarClip", Min=1.0, Max=100000.0, Speed=10.0)
	float FarZ = 1000.0f;
	UPROPERTY(EditAnywhere, DisplayName="Ortho Width", Min=0.1, Max=1000.0, Speed=0.5, Animatable)
	float OrthoWidth = 10.0f;
	UPROPERTY(EditAnywhere, DisplayName="Orthographic")
	bool bIsOrthogonal = false;
};

USTRUCT()
struct FCameraPostProcessSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, DisplayName="Vignette Enabled", SerializeName="VignetteEnabled")
	bool bVignetteEnabled = false;
	UPROPERTY(EditAnywhere, DisplayName="Vignette Intensity", SerializeName="VignetteIntensity", Min=0.0, Max=1.0, Speed=0.01, Animatable)
	float VignetteIntensity = 0.0f;
	UPROPERTY(EditAnywhere, DisplayName="Vignette Radius", SerializeName="VignetteRadius", Min=0.0, Max=2.0, Speed=0.01, Animatable)
	float VignetteRadius = 0.75f;
	UPROPERTY(EditAnywhere, DisplayName="Vignette Smoothness", SerializeName="VignetteSmoothness", Min=0.001, Max=2.0, Speed=0.01, Animatable)
	float VignetteSmoothness = 0.35f;
	UPROPERTY(EditAnywhere, DisplayName="Vignette Color", SerializeName="VignetteColor", Speed=0.01, Animatable)
	FColor VignetteColor = FColor::Black();
};

// PlayerCameraManager 의 연산 결과를 담는 구조체
// 지금은 CameraState 와 대부분 겹치긴 하지만, 다른 레이어라고 생각하여 분리
struct FMinimalViewInfo
{
    float FOV = 3.14159265358979f / 3.0f;
    float AspectRatio = 16.0f / 9.0f;
    float NearZ = 0.1f;
    float FarZ = 1000.0f;
    float OrthoWidth = 10.0f;
    bool bIsOrthogonal = false;
    FCameraPostProcessSettings PostProcessSettings;

    FVector Location = FVector::ZeroVector;
    FQuat Rotation = FQuat::Identity;
};

UCLASS()
class UCameraComponent : public USceneComponent
{
public:
	GENERATED_BODY()

	UCameraComponent() = default;

	virtual void Serialize(FArchive& Ar) override;

	void LookAt(const FVector& Target);
	void SetCameraState(const FCameraState& NewState);
	const FCameraState& GetCameraState() const { return CameraState; }

	void SetFOV(float InFOV) { CameraState.FOV = InFOV; }
	void SetOrthoWidth(float InWidth) { CameraState.OrthoWidth = InWidth; }
	void SetOrthographic(bool bOrtho) { CameraState.bIsOrthogonal = bOrtho; }
	void SetVignette(float Intensity, float Radius = 0.75f, float Smoothness = 0.35f, const FColor& Color = FColor::Black());
	void ClearVignette();
	const FCameraPostProcessSettings& GetPostProcessSettings() const { return PostProcessSettings; }

	void OnResize(int32 Width, int32 Height);

	FMatrix GetViewMatrix() const;
	FMatrix GetProjectionMatrix() const;

	float GetFOV() const { return CameraState.FOV; }
	float GetNearPlane() const { return CameraState.NearZ; }
	float GetFarPlane() const { return CameraState.FarZ; }
	float GetOrthoWidth() const { return CameraState.OrthoWidth; }
	bool IsOrthogonal() const { return CameraState.bIsOrthogonal; }

	FRay DeprojectScreenToWorld(float MouseX, float MouseY, float ScreenWidth, float ScreenHeight);

public:
	//	Unreal-style editor camera helpers
	void AddYawInput(float DeltaYawDegrees);
	void AddPitchInput(float DeltaPitchDegrees);

	void MoveForward(float Distance);
	void MoveRight(float Distance);
	void MoveUp(float Distance);

	float GetPitchDegrees() const;
	float GetYawDegrees() const;

	FVector GetForwardVector() const;
	FVector GetRightVector() const;
	FVector GetUpVector() const;

	// DeltaTime 기반 업데이트 가능성 열어둠
    void GetCameraView(float DeltaTime, FMinimalViewInfo& OutView) const;

private:
	void SetViewRotationDegrees(float PitchDegrees, float YawDegrees);

private:
	UPROPERTY(EditAnywhere)
	FCameraState CameraState;
	UPROPERTY(EditAnywhere)
	FCameraPostProcessSettings PostProcessSettings;
};
