#pragma once

#include "GameFramework/AActor.h"
#include "Component/Light/PointLightComponent.h"
#include "Component/SceneComponent.h"
#include "Component/BillboardComponent.h"

class APointLightActor : public AActor
{
  public:
    DECLARE_CLASS(APointLightActor, AActor)

    APointLightActor() = default;
    ~APointLightActor() override = default;

    virtual void InitDefaultComponents() override;

    UPointLightComponent* GetLightComponent() const { return LightComponent; }

  private:
    UPointLightComponent* LightComponent = nullptr;
    UBillboardComponent*  LightBillboard = nullptr;
};