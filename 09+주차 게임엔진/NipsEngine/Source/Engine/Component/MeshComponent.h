#pragma once
#include "PrimitiveComponent.h"

class UMaterialInterface;

class UMeshComponent : public UPrimitiveComponent
{
public:
    DECLARE_CLASS(UMeshComponent, UPrimitiveComponent)
    ~UMeshComponent() override;

    virtual void Serialize(FArchive& Ar) override;

    virtual void SetMaterial(int32 SlotIndex, UMaterialInterface* InMaterial) override;
    virtual UMaterialInterface* GetMaterial(int32 SlotIndex) const override;

    const TArray<UMaterialInterface*>& GetOverrideMaterial() const;
    const TPair<float, float> GetScroll() const { return ScrollUV; };

    virtual int32 GetNumMaterials() const override;
    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
    void PostEditProperty(const char * PropertyName) override;
    
    virtual void TickComponent(float DeltaTime) override;

    // --- Drift Salvage 회수 강조 ---
    // SetHighlight(true) 호출 시 모든 머티리얼 슬롯을 GHighlightMaterial로 교체하고
    // 원본을 캐시한다. SetHighlight(false)에서 원본으로 복원.
    // 매 프레임 같은 값으로 호출되어도 안전 (idempotent).
    void SetHighlight(bool bOn);
    bool IsHighlighted() const { return bHighlighted; }

    // 회수 가능 강조 시 사용할 전역 머티리얼.
    // 게임 시작 시 한 번 세팅 (예: 노란색 unlit 머티리얼).
    // 미세팅 상태에서 SetHighlight(true)는 no-op이다.
    static void SetGlobalHighlightMaterial(UMaterialInterface* InMat) { GHighlightMaterial = InMat; }
    static UMaterialInterface* GetGlobalHighlightMaterial() { return GHighlightMaterial; }

protected:
    void ReleaseOwnedMaterialInstances();
    void ReleaseOwnedMaterialSlot(UMaterialInterface*& InOutMaterial);

protected:
    TArray<UMaterialInterface*> Materials;
    TPair<float, float> ScrollUV = { };

    // 강조 상태에서 원본 머티리얼 보관 (복원용)
    bool bHighlighted = false;
    TArray<UMaterialInterface*> OriginalMaterialsCache;

    static UMaterialInterface* GHighlightMaterial;
};
