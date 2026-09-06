#include "GameFramework/Camera/CameraModifier_CameraShake.h"
#include "Math/Utils.h"
#include "Engine/Core/Logging/Log.h"

#include <cmath>
#include <memory>

namespace
{
    float ResolveCameraShakeDuration(const FCameraShakeParams& Params)
    {
        if (Params.PatternType == ECameraShakePatternType::CameraSequence)
        {
            std::shared_ptr<const FCameraSequenceAsset> SequenceAsset =
                FCameraSequenceManager::Get().LoadSequence(Params.Sequence);
            return SequenceAsset ? SequenceAsset->Duration : 0.0f;
        }

        return Params.Duration;
    }

    std::unique_ptr<ICameraShakePattern> MakeCameraShakePattern(const FCameraShakeParams& Params)
    {
        switch (Params.PatternType)
        {
        case ECameraShakePatternType::WaveOscillator:
            return std::make_unique<FWaveOscillatorPattern>(Params.WaveOscillator);

        case ECameraShakePatternType::CameraSequence:
            return std::make_unique<FSequenceCameraShakePattern>(Params);
        }

        return std::make_unique<FWaveOscillatorPattern>(Params.WaveOscillator);
    }
}

FCameraShakeModifier::FCameraShakeModifier(const FCameraShakeParams& Params)
    : FTimedCameraModifier(ResolveCameraShakeDuration(Params)), ShakePattern(MakeCameraShakePattern(Params))
{
    Priority = 16;
}

bool FCameraShakeModifier::ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV)
{
    float NormalizedTime = -1.0f; // OutParam of Advance()
    if (!Advance(DeltaTime, NormalizedTime))
    {
        return false;
    }

    const FCameraShakePatternUpdateResult Delta = ShakePattern->UpdatePattern(
        DeltaTime,
        GetElapsedSeconds(),
        GetTotalDuration());

    float FinalWeight = Alpha * (1.0f - NormalizedTime);
    InOutPOV.Location += Delta.Location * FinalWeight;
    InOutPOV.Rotation = (InOutPOV.Rotation * FQuat::Slerp(FQuat::Identity, Delta.Rotation, FinalWeight)).GetNormalized();
    InOutPOV.FOV += Delta.FOV * FinalWeight;

    return true;
}

FCameraShakePatternUpdateResult FWaveOscillatorPattern::UpdatePattern(float DeltaTime, float ElapsedTime, float TotalDuration)
{
    FCameraShakePatternUpdateResult Result;
    Result.Location.X = Params.LocationAmplitude.X * std::sin(Params.LocationFrequency.X * ElapsedTime * 2.0f * MathUtil::PI);
    Result.Location.Y = Params.LocationAmplitude.Y * std::sin(Params.LocationFrequency.Y * ElapsedTime * 2.0f * MathUtil::PI);
    Result.Location.Z = Params.LocationAmplitude.Z * std::sin(Params.LocationFrequency.Z * ElapsedTime * 2.0f * MathUtil::PI);

    const FVector RotationEuler(
        Params.RotationAmplitude.X * std::sin(Params.RotationFrequency.X * ElapsedTime * 2.0f * MathUtil::PI),
        Params.RotationAmplitude.Y * std::sin(Params.RotationFrequency.Y * ElapsedTime * 2.0f * MathUtil::PI),
        Params.RotationAmplitude.Z * std::sin(Params.RotationFrequency.Z * ElapsedTime * 2.0f * MathUtil::PI));
    Result.Rotation = FQuat::MakeFromEuler(RotationEuler);

    Result.FOV = Params.FOVAmplitude * std::sin(Params.FOVFrequency * ElapsedTime * 2.0f * MathUtil::PI);

    return Result;
}

FCameraShakePatternUpdateResult FSequenceCameraShakePattern::UpdatePattern(float DeltaTime, float ElapsedTime, float TotalDuration)
{
    FCameraShakePatternUpdateResult Result;
    if (!SequenceAsset)
    {
        return Result;
    }

    const FCameraSequenceSample Sample = SequenceAsset->Evaluate(ElapsedTime, Scale);
    Result.Location = Sample.Location;
    Result.Rotation = FQuat::MakeFromEuler(Sample.RotationEuler);
    Result.FOV = Sample.FOV;

    return Result;
}

FSequenceCameraShakePattern::FSequenceCameraShakePattern(const FCameraShakeParams& InParams)
    : SequenceAsset(FCameraSequenceManager::Get().LoadSequence(InParams.Sequence))
    , Scale(InParams.Scale)
{
    if (!SequenceAsset)
    {
        UE_LOG("[CameraSequence] Failed to resolve sequence '%s' for camera shake.\n", InParams.Sequence.c_str());
    }
}
