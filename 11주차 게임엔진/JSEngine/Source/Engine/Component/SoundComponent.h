#pragma once

#include "Audio/AudioSystem.h"
#include "Component/SceneComponent.h"
#include "Generated/SoundComponent.generated.h"

UCLASS()
class USoundComponent : public USceneComponent
{
public:
    GENERATED_BODY()

    void BeginPlay() override;
    void EndPlay() override;
    void Serialize(FArchive& Ar) override;
    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;

    void Play();
    void Stop();
    bool IsPlaying() const;

    void SetSound(const FString& InSoundKeyOrPath) { SoundKeyOrPath = InSoundKeyOrPath; }
    const FString& GetSound() const { return SoundKeyOrPath; }
    void SetPlayOnBeginPlay(bool bEnabled) { bPlayOnBeginPlay = bEnabled; }
    bool IsPlayOnBeginPlay() const { return bPlayOnBeginPlay; }
    void SetLoop(bool bEnabled) { bLoop = bEnabled; }
    bool IsLooping() const { return bLoop; }
    void SetSpatialized(bool bEnabled) { bSpatialized = bEnabled; }
    bool IsSpatialized() const { return bSpatialized; }
    void SetVolumeScale(float InVolumeScale) { VolumeScale = InVolumeScale; }
    float GetVolumeScale() const { return VolumeScale; }
    void Set3DMinMaxDistance(float InMinDistance, float InMaxDistance);
    float Get3DMinDistance() const { return MinDistance; }
    float Get3DMaxDistance() const { return MaxDistance; }
    void Set3DAttenuation(int InAttenuationModel, float InRolloffFactor);
    int Get3DAttenuationModel() const { return AttenuationModel; }
    float Get3DRolloffFactor() const { return RolloffFactor; }

protected:
    void TickComponent(float DeltaTime) override;

private:
    // SoundRegistry 키 또는 Asset/Audio 기준 파일 경로를 넣습니다.
    UPROPERTY(EditAnywhere, DisplayName="Sound")
    FString SoundKeyOrPath;
    UPROPERTY(EditAnywhere, DisplayName="Play On BeginPlay")
    bool bPlayOnBeginPlay = false;
    UPROPERTY(EditAnywhere, DisplayName="Loop")
    bool bLoop = false;
    UPROPERTY(EditAnywhere, DisplayName="Spatialized")
    bool bSpatialized = true;
    UPROPERTY(EditAnywhere, DisplayName="Volume Scale", Min=0.0, Max=2.0, Speed=0.01)
    float VolumeScale = 1.0f;
    UPROPERTY(EditAnywhere, DisplayName="Fade In", Min=0.0, Max=10.0, Speed=0.01)
    float FadeInSeconds = 0.0f;
    UPROPERTY(EditAnywhere, DisplayName="Fade Out", Min=0.0, Max=10.0, Speed=0.01)
    float FadeOutSeconds = 0.0f;
    UPROPERTY(EditAnywhere, DisplayName="3D Min Distance", Min=0.0, Max=100.0, Speed=0.1)
    float MinDistance = 2.5f;
    UPROPERTY(EditAnywhere, DisplayName="3D Max Distance", Min=0.1, Max=500.0, Speed=0.1)
    float MaxDistance = 22.0f;
    UPROPERTY(EditAnywhere, DisplayName="3D Attenuation Model", Min=0.0, Max=3.0, Speed=1.0)
    int AttenuationModel = 2;
    UPROPERTY(EditAnywhere, DisplayName="3D Rolloff Factor", Min=0.0, Max=8.0, Speed=0.1)
    float RolloffFactor = 1.0f;
    FAudioHandle ActiveHandle = 0;
};
