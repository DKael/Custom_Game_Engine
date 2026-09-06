#pragma once

#include "Engine/Core/Containers/String.h"
#include "GameFramework/Camera/CameraModifier.h"
#include "GameFramework/Camera/CameraSequenceManager.h"

enum class ECameraShakePatternType
{
    WaveOscillator,
    CameraSequence,
};

struct FCameraShakeWaveOscillatorParams
{
    FVector LocationAmplitude = FVector::ZeroVector;
    FVector LocationFrequency = FVector::ZeroVector;
    FVector RotationAmplitude = FVector::ZeroVector;
    FVector RotationFrequency = FVector::ZeroVector;
    float FOVAmplitude = 0.0f;
    float FOVFrequency = 0.0f;
};

struct FCameraShakeParams
{
    ECameraShakePatternType PatternType = ECameraShakePatternType::WaveOscillator;
    float Duration = 0.0f;
    FCameraShakeWaveOscillatorParams WaveOscillator;
    FString Sequence;
    float Scale = 1.0f;
};

struct FCameraShakePatternUpdateResult
{
    FCameraShakePatternUpdateResult()
        : Location(FVector::ZeroVector), Rotation(FQuat::Identity), FOV(0.0f)
    {
    }

    FVector Location = FVector::ZeroVector;
    FQuat Rotation = FQuat::Identity;
    float FOV = 0.0f;
};

class ICameraShakePattern
{
public:
    virtual ~ICameraShakePattern() = default;
    virtual FCameraShakePatternUpdateResult UpdatePattern(float DeltaTime, float ElapsedTime, float TotalDuration) = 0;
};

class FCameraShakeModifier final : public FTimedCameraModifier
{
public:
    // TODO: Blend in/out time 추가 필요
    explicit FCameraShakeModifier(const FCameraShakeParams& Params);

    bool ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV) override;

private:
    std::unique_ptr<ICameraShakePattern> ShakePattern;
};

class FWaveOscillatorPattern : public ICameraShakePattern
{
public:
    explicit FWaveOscillatorPattern(const FCameraShakeWaveOscillatorParams& InParams)
        : Params(InParams)
    {
    }

    FCameraShakePatternUpdateResult UpdatePattern(float DeltaTime, float ElapsedTime, float TotalDuration) override;

private:
    FCameraShakeWaveOscillatorParams Params;
};

class FSequenceCameraShakePattern : public ICameraShakePattern
{
public:
    explicit FSequenceCameraShakePattern(const FCameraShakeParams& InParams);

    FCameraShakePatternUpdateResult UpdatePattern(
        float DeltaTime,
        float ElapsedTime,
        float TotalDuration = -1.0f) override;

private:
    std::shared_ptr<const FCameraSequenceAsset> SequenceAsset;
    float Scale = 1.0f;
};
