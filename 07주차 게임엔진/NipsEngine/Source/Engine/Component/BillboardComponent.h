#pragma once
#include "PrimitiveComponent.h"
#include "Core/ResourceTypes.h"
#include "Object/FName.h"


class FViewportCamera;
struct FTextureResource;

class UBillboardComponent : public UPrimitiveComponent
{
protected:
	bool bIsBillboard = true;
	bool TryGetActiveCamera(const FViewportCamera*& OutCamera) const;
	
	// UpdateWorldAABB 등의 함수를 오버라이드하지 않았기 때문에 UBillboradComponent도 추상 클래스가 됩니다.
	// 추후에 UBillboardComponent를 사용할 일이 있다면 Duplicate의 주석을 해제하고 수정하시면 됩니다.
	virtual UBillboardComponent* Duplicate() override;
	virtual UBillboardComponent* DuplicateSubObjects() override  { return this; }

public:
	DECLARE_CLASS(UBillboardComponent, UPrimitiveComponent)
	
	void TickComponent(float DeltaTime) override;

	void SetBillboardEnabled(bool bEnable) { bIsBillboard = bEnable; }
	static constexpr EPrimitiveType PrimitiveType = EPrimitiveType::EPT_Billboard;

	FMatrix MakeBillboardWorldMatrix(
		const FVector& CameraForward,
		const FVector& CameraRight,
		const FVector& CameraUp) const;

	EPrimitiveType GetPrimitiveType() const override { return PrimitiveType; }

	void SetTextureName(FString InName);
	FString GetTextureName();
	FMaterialResource* GetCachedSprite();

	void SetDistanceFade(bool bEnable) { bDistanceFade = bEnable; }
	bool IsDistanceFadeEnabled() const { return bDistanceFade; }
	void SetFadeStartDistance(float InDistance) { FadeStartDistance = InDistance; }
	float GetFadeStartDistance() const { return FadeStartDistance; }
	void SetFadeEndDistance(float InDistance) { FadeEndDistance = InDistance; }
	float GetFadeEndDistance() const { return FadeEndDistance; }
    void  SetCylindrical(bool bEnable) { bIsCylinderical = bEnable; }
    bool  IsCylindrical() const { return bIsCylinderical; }
    void  SetUseRotation(bool bEnable) { bUseRotation = bEnable; }
    bool  IsUseRotation() const { return bUseRotation; }
    void  SetTintColor(const FColor& InColor) { TintColor = InColor; }
    const FColor& GetTintColor() const { return TintColor; }

	//////////////////// override ////////////////////////////
	void UpdateWorldAABB() const override;
	bool RaycastMesh(const FRay& Ray, FHitResult& OutHitResult) override;
	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void PostEditProperty(const char* PropertyName) override;
	float GetWidth()  const { return Width; }
	float GetHeight() const { return Height; }
	// Billboard는 outline 미지원 (Batcher 계열)
	//void SetSpriteSize(float InWidth, float InHeight) { Width = InWidth; Height = InHeight; }

	///////////////////////////////////////////////////////////

private:
	FName TextureName;
	FMaterialResource* CachedSprite = { nullptr }; // ResourceManager 소유, 여기선 참조만
	uint32 FrameIndex = 0;
	float  Width = 1.0f;
	float  Height = 1.0f;
	float  PlayRate = 30.0f; // 초당 프레임 수
	float  TimeAccumulator = 0.0f;
	bool bLoop = true;

	bool bDistanceFade = false;
	float FadeStartDistance = 100.0f;
	float FadeEndDistance = 1000.0f;

	bool bIsCylinderical = false;
    bool bUseRotation = false;
    FColor TintColor = FColor(1.0f, 1.0f, 1.0f, 1.0f);
};

