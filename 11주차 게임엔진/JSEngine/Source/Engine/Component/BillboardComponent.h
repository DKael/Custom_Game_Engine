#pragma once
#include "PrimitiveComponent.h"
#include "Core/ResourceTypes.h"
#include "Object/FName.h"
#include "Generated/BillboardComponent.generated.h"


class FViewportCamera;
struct FTextureResource;

UCLASS()
class UBillboardComponent : public UPrimitiveComponent
{
protected:
	bool bIsBillboard = true;
	UPROPERTY(EditAnywhere, DisplayName="Inherit Owner Scale", SerializeName="bInheritOwnerScale")
	bool bInheritOwnerScale = false;
	bool TryGetActiveCamera(const FViewportCamera*& OutCamera) const;
	
	virtual void PostDuplicate(UObject* Original) override;

public:
	GENERATED_BODY()

	virtual void Serialize(FArchive& Ar) override;

	void TickComponent(float DeltaTime) override;

	void SetBillboardEnabled(bool bEnable) { bIsBillboard = bEnable; }
	void SetInheritOwnerScale(bool bInherit) { bInheritOwnerScale = bInherit; }
	bool ShouldInheritOwnerScale() const { return bInheritOwnerScale; }
	FVector GetBillboardWorldScale() const;
	static constexpr EPrimitiveType PrimitiveType = EPrimitiveType::EPT_Billboard;

	static FMatrix MakeBillboardWorldMatrix(
		const FVector& WorldLocation,
		const FVector& WorldScale,
		const FVector& CameraForward,
		const FVector& CameraRight,
		const FVector& CameraUp);

	EPrimitiveType GetPrimitiveType() const override { return PrimitiveType; }

	void SetTextureName(FString InName);
	FString GetTextureName();
	UTexture* GetTexture();

	//////////////////// override ////////////////////////////
	void UpdateWorldAABB() const override;
	bool RaycastMesh(const FRay& Ray, FHitResult& OutHitResult) override;
	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	float GetWidth()  const { return Width; }
	float GetHeight() const { return Height; }
	FColor GetColor() const { return Color; }
	void SetColor(const FColor& InColor) { Color = InColor; }
	// Billboard는 outline 미지원 (Batcher 계열)
	//void SetSpriteSize(float InWidth, float InHeight) { Width = InWidth; Height = InHeight; }

	///////////////////////////////////////////////////////////

protected:
    FPrimitiveRenderProxy* CreateRenderProxy() override;

private:
	UPROPERTY(EditAnywhere, DisplayName="Texture")
	FName TextureName;
	UTexture* Texture = nullptr; // ResourceManager 소유, 여기선 참조만
	FColor Color = FColor::White();

protected:
	uint32 FrameIndex = 0;
	UPROPERTY(EditAnywhere, DisplayName="Width", Min=0.1, Max=100.0, Speed=0.1)
	float  Width = 1.0f;
	UPROPERTY(EditAnywhere, DisplayName="Height", Min=0.1, Max=100.0, Speed=0.1)
	float  Height = 1.0f;
	UPROPERTY(EditAnywhere, DisplayName="Play Rate", SerializeName="PlayRate", Min=1.0, Max=120.0, Speed=1.0)
	float  PlayRate = 30.0f; // 초당 프레임 수
	float  TimeAccumulator = 0.0f;
	UPROPERTY(EditAnywhere, DisplayName="bLoop")
	bool   bLoop = true;
};

