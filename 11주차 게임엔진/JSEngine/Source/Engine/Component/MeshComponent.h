#pragma once
#include "PrimitiveComponent.h"
#include "Generated/MeshComponent.generated.h"

class UMaterialInterface;

USTRUCT()
struct FScrollUV
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, DisplayName="Scroll U", Min=-1.0, Max=1.0, Speed=0.01)
	float U = 0.0f;
	UPROPERTY(EditAnywhere, DisplayName="Scroll V", Min=-1.0, Max=1.0, Speed=0.01)
	float V = 0.0f;
};

UCLASS(Abstract)
class UMeshComponent : public UPrimitiveComponent
{
public:
	GENERATED_BODY()

	virtual void Serialize(FArchive& Ar) override;

	virtual void SetMaterial(int32 SlotIndex, UMaterialInterface* InMaterial) override;
	virtual UMaterialInterface* GetMaterial(int32 SlotIndex) const override;

	const TArray<UMaterialInterface*>& GetOverrideMaterial() const;
	const std::pair<float, float> GetScroll() const { return { ScrollUV.U, ScrollUV.V }; };

	virtual int32 GetNumMaterials() const override;
	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void PostEditProperty(const char * PropertyName) override;
	
	virtual void TickComponent(float DeltaTime) override;

protected:
	TArray<UMaterialInterface*> Materials;
	UPROPERTY(EditAnywhere)
	FScrollUV ScrollUV;
};
