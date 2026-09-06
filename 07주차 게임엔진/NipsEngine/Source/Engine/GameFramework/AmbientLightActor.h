#pragma once

#include "GameFramework/AActor.h"
#include "Component/Light/AmbientLightComponent.h"
#include "Component/SceneComponent.h"

class AAmbientLightActor : public AActor
{
  public:
    DECLARE_CLASS(AAmbientLightActor, AActor)

    AAmbientLightActor() = default;
    ~AAmbientLightActor() override = default;

    virtual void InitDefaultComponents() override;

    UAmbientLightComponent* GetLightComponent() const { return LightComponent; }

  private:
    UAmbientLightComponent* LightComponent = nullptr;
};