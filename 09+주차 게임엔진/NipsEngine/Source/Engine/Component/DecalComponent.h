#pragma once

#include "Component/PrimitiveComponent.h"

class UMaterialInterface;

class UDecalComponent : public UPrimitiveComponent
{
public:
    DECLARE_CLASS(UDecalComponent, UPrimitiveComponent)

    UDecalComponent();

    void PostDuplicate(UObject* Original) override;

    virtual void Serialize(FArchive& Ar) override;

    void BeginPlay() override;

    virtual void SetMaterial(int32 SlotIndex, UMaterialInterface* InMaterial) override { if (SlotIndex == 0) Materials[0] = InMaterial; }
    virtual UMaterialInterface* GetMaterial(int32 SlotIndex) const override { return (SlotIndex == 0) ? Materials[0] : nullptr; }
    virtual int32 GetNumMaterials() const override { return 1; }

    // Legacy single-slot access
    void SetMaterial(UMaterialInterface* InMaterial) { SetMaterial(0, InMaterial); }
    UMaterialInterface* GetMaterial() const { return GetMaterial(0); }

    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
    void PostEditProperty(const char* PropertyName) override;

    void UpdateWorldAABB() const override;
    bool RaycastMesh(const FRay& Ray, FHitResult& OutHitResult) override;
    EPrimitiveType GetPrimitiveType() const override { return EPrimitiveType::EPT_Decal; }

    FMatrix GetDecalMatrix() const;
    FColor GetDecalColor() const { return DecalColor; }
    bool AffectsOnlyWaterReceivers() const { return bAffectOnlyWaterReceivers; }

    void SetSize(const FVector& InSize);
    void SetFadeOutSizeMultiplier(const FVector& InMultiplier);
    void SetAffectOnlyWaterReceivers(bool bInAffectOnlyWaterReceivers) { bAffectOnlyWaterReceivers = bInAffectOnlyWaterReceivers; }

    void SetFadeIn(float InStartDelay, float InDuration);
    void SetFadeOut(float InStartDelay, float InDuration, bool bInDestroyOwnerAfterFade = false);
    void ResetFadeState();

    bool SupportsOutline() const override { return true; }

protected:
    void TickComponent(float DeltaTime) override;

private:
    void ApplyCurrentSize(const FVector& InSize, bool bUpdateInitialSize);
    void TickFadeIn();
    void TickFadeOut();

private:
    TArray<UMaterialInterface*> Materials;
    FVector DecalSize = FVector(5.0f, 5.0f, 5.0f);
    FVector InitialDecalSize = FVector(5.0f, 5.0f, 5.0f);
    FColor DecalColor = FColor::White();
    FVector FadeOutSizeMultiplier = FVector::OneVector;

    float FadeStartDelay = 0.0f;
    float FadeDuration = 0.0f;
    float FadeInDuration = 0.0f;
    float FadeInStartDelay = 0.0f;
    bool bDestroyOwnerAfterFade = false;
    bool bAffectOnlyWaterReceivers = false;

    float LifeTime = 0.0f;
};
