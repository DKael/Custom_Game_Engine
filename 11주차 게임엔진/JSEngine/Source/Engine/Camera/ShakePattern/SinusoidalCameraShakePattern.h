#pragma once
#include "Camera/CameraShakeBase.h"
#include "Generated/SinusoidalCameraShakePattern.generated.h"

UCLASS()
class USinusoidalCameraShakePattern : public UCameraShakePattern
{
public:
    GENERATED_BODY()

    // Location parameters
    UPROPERTY(EditAnywhere, DisplayName="LocationAmplitude")
    FVector LocationAmplitude{ 0, 0, 0 }; // units
    UPROPERTY(EditAnywhere, DisplayName="LocationFrequency")
    FVector LocationFrequency{ 0, 0, 0 }; // Hz per axis
    UPROPERTY(EditAnywhere, DisplayName="LocationPhase")
    FVector LocationPhase{ 0, 0, 0 };     // radians per axis

    // Rotation parameters (degrees)
    UPROPERTY(EditAnywhere, DisplayName="RotationAmplitudeDeg")
    FVector RotationAmplitudeDeg{ 0, 0, 0 }; // degrees
    UPROPERTY(EditAnywhere, DisplayName="RotationFrequency")
    FVector RotationFrequency{ 0, 0, 0 };    // Hz per axis
    UPROPERTY(EditAnywhere, DisplayName="RotationPhase")
    FVector RotationPhase{ 0, 0, 0 };        // radians per axis

    // FOV parameters (degrees)
    UPROPERTY(EditAnywhere, DisplayName="FOVAmplitude")
    float FOVAmplitude = 0.0f; // degrees
    UPROPERTY(EditAnywhere, DisplayName="FOVFrequency")
    float FOVFrequency = 0.0f; // Hz
    UPROPERTY(EditAnywhere, DisplayName="FOVPhase")
    float FOVPhase = 0.0f;     // radians

private:
    virtual void OnStartShakePattern(const FCameraShakeStartParams& Params) override; 
    virtual void OnUpdateShakePattern(
        const FCameraShakeUpdateParams& Params,
        FCameraShakeUpdateResult& OutResult) override;
};
