#pragma once
#include "VertexFactoryData.h"
#include "Core/CoreMinimal.h"

class FSkeletalVertexFactoryData : public FVertexFactoryData
{
public:
    const TArray<FMatrix>* SkinningMatrices = nullptr;
};
