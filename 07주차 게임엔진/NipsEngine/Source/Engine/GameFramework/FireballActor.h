#pragma once
#include "AActor.h"
class AFireballActor : public AActor
{
  public:
    DECLARE_CLASS(AFireballActor, AActor)

	virtual void InitDefaultComponents() override;
};
