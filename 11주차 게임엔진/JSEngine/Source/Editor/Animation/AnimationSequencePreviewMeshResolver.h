#pragma once

#include "Core/CoreMinimal.h"

class UAnimSequence;

class FAnimationSequencePreviewMeshResolver
{
public:
    bool Resolve(const FString& SequencePath, const UAnimSequence* Sequence, FString& OutMeshPath) const;
};
