#pragma once
#include "Component/PrimitiveComponent.h"

struct FOBB;

struct FLineVertex
{
    FVector Position;
    FColor Color;

    FLineVertex(const FVector& InPos, const FColor& InColor) : Position(InPos), Color(InColor) {}
};

class UMaterialInterface;

// Currently unused. Planned for use after RenderProxy integration.
class ULineBatchComponent : public UPrimitiveComponent
{
public:
    ULineBatchComponent();
    virtual ~ULineBatchComponent() override = default;

    virtual EPrimitiveType GetPrimitiveType() const override { return EPrimitiveType::EPT_Line; }

    void AddLine(const FVector& Start, const FVector& End, const FColor& Color);
    void AddAABB(const FAABB& Box, const FColor& Color);
    void AddOBB(const FOBB& Box, const FColor& Color);

    UMaterialInterface* GetMaterial() const { return Material; }

private:
    UMaterialInterface* Material = nullptr;

    TArray<FLineVertex> LineVertices;
    TArray<uint32> Indices;
};
