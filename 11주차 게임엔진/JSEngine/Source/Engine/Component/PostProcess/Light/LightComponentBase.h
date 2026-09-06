#pragma once
#include "Component/SceneComponent.h"
#include "Generated/LightComponentBase.generated.h"

UCLASS()
class ULightComponentBase : public USceneComponent {
public:
	GENERATED_BODY()

	ULightComponentBase() = default;
    virtual void PostDuplicate(UObject* Original) override;
	virtual void Serialize(FArchive& Ar) override;

protected:
	~ULightComponentBase() = default;

public:
    UPROPERTY(EditAnywhere, DisplayName="Color", Speed=0.1, Animatable)
    FColor LightColor = FColor::White();
	UPROPERTY(EditAnywhere, DisplayName="Intensity", Speed=0.1, Animatable)
	float Intensity = 1.0f;

	UPROPERTY(EditAnywhere, DisplayName="Cast Shadows", SerializeName="CastShadows")
	bool bCastShadows = true;
};
