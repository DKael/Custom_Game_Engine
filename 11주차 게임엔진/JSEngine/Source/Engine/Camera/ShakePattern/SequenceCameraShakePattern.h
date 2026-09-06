#pragma once
#include "../CameraShakeBase.h"
#include "Animation/TimelinePlayer.h"
#include "Generated/SequenceCameraShakePattern.generated.h"

class FTimelinePlayer;
class UCameraAnimationSequence;
class UCurveFloatAsset;

enum class ECameraShakeCurveChannel
{
    LocationX = 0,
    LocationY,
    LocationZ,
    Pitch,
    Yaw,
    Roll,
    FOV,
    Count
};

UCLASS()
class USequenceCameraShakePattern : public UCameraShakePattern
{
public:
    GENERATED_BODY()

	UCameraAnimationSequence* Sequence = nullptr;
	UCurveFloatAsset* Curve = nullptr;

    UPROPERTY(EditAnywhere, DisplayName="PlayRate")
	float PlayRate = 1.0f;
    UPROPERTY(EditAnywhere, DisplayName="Scale")
	float Scale = 1.0f;
    UPROPERTY(EditAnywhere, DisplayName="RandomSegmentDuration")
    float RandomSegmentDuration = 0.0f;
    UPROPERTY(EditAnywhere, DisplayName="bRandomSegment")
    bool bRandomSegment = false;
    UPROPERTY(EditAnywhere, DisplayName="bLoop")
    bool bLoop = false;
    UPROPERTY(EditAnywhere, DisplayName="CurveAssetPath")
    FString CurveAssetPath;

    UPROPERTY(EditAnywhere, DisplayName="LocationAmplitude")
    FVector LocationAmplitude = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, DisplayName="RotationAmplitudeDeg")
    FVector RotationAmplitudeDeg = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, DisplayName="FOVAmplitude")
    float FOVAmplitude = 0.0f;

    void GetCameraShakeInfo(FCameraShakeInfo& OutCameraInfo) const override;
private:	
    virtual void OnStartShakePattern(const FCameraShakeStartParams& Params) override;
    virtual void OnStopShakePattern(bool bImmediately) override;
    virtual void OnUpdateShakePattern(
        const FCameraShakeUpdateParams& Params,
        FCameraShakeUpdateResult& OutResult) override;

private:
    FTimelinePlayer CameraShakeTimeline;
    float CurrentCurveValues[(int)ECameraShakeCurveChannel::Count] = {};
};
