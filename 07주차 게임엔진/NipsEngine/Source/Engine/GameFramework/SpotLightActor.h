#pragma once

#include "GameFramework/AActor.h"
#include "Component/Light/SpotLightComponent.h"
#include "Component/SceneComponent.h"
#include "Component/BillboardComponent.h"

class ASpotLightActor : public AActor
{
  public:
    DECLARE_CLASS(ASpotLightActor, AActor)

    ASpotLightActor() = default;
    ~ASpotLightActor() override = default;

    virtual void InitDefaultComponents() override;

    USpotLightComponent* GetLightComponent() const { return LightComponent; }

  private:
    USpotLightComponent* LightComponent = nullptr;
    UBillboardComponent*  LightBillboard = nullptr;
};
