#pragma once
#include "ShapeComponent.h"
#include "Generated/CapsuleComponent.generated.h"

UCLASS()
class UCapsuleComponent : public UShapeComponent
{
public:
    GENERATED_BODY()

    float GetCapsuleHalfHeight() const { return CapsuleHalfHeight; }
    float GetCapsuleRadius() const { return CapsuleRadius; }

	void UpdateWorldAABB() const override;

    float GetScaledCapsuleHalfHeight() const 
	{
        FVector Scale = GetWorldScale();
        return CapsuleHalfHeight * std::abs(Scale.Z);
	}
    
	float GetScaledCapsuleRadius() const
    {
        FVector Scale = GetWorldScale();
        return CapsuleRadius * std::abs(Scale.Z);
    }

    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
    void PostDuplicate(UObject* Original) override;
    void Serialize(FArchive& Ar) override;

private:
    UPROPERTY(EditAnywhere, DisplayName="CapsuleHalfHeight")
    float CapsuleHalfHeight = 0.5f;
    UPROPERTY(EditAnywhere, DisplayName="CapsuleRadius")
    float CapsuleRadius = 0.5f;

    bool RaycastMesh(const FRay& Ray, FHitResult& OutHitResult) override;
    EPrimitiveType GetPrimitiveType() const override;
};
