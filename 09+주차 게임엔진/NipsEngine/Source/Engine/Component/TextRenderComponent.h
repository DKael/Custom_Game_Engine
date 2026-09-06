#pragma once

#include "PrimitiveComponent.h"
#include "Core/ResourceTypes.h"
#include "Object/FName.h"

// Text rendering space mode.
enum class ETextRenderSpace : int32
{
    World,  // Billboarded in 3D world space
    Screen, // Fixed in 2D screen space
};

// Horizontal alignment.
enum class ETextHAlign : int32
{
    Left,
    Center,
    Right,
};

// Vertical alignment.
enum class ETextVAlign : int32
{
    Top,
    Center,
    Bottom,
};

// Component that renders text in world or screen space.
// It is discovered through UPrimitiveComponent and rendered by the font batcher.
class UTextRenderComponent : public UPrimitiveComponent
{
public:
    DECLARE_CLASS(UTextRenderComponent, UPrimitiveComponent)

    UTextRenderComponent();
    ~UTextRenderComponent() override = default;

    virtual void PostDuplicate(UObject* Original) override;
    virtual void Serialize(FArchive& Ar) override;

    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
    void PostEditProperty(const char* PropertyName) override;

    // Text.
    void SetText(const FString& InText) { Text = InText; }
    const FString& GetText() const { return Text; }

    FString GetOwnerUUIDToString() const;
    FString GetOwnerNameToString() const;

    // Font.
    void SetFont(const FName& InFontName);
    const FFontResource* GetFont() const { return CachedFont; }
    const FName& GetFontName() const { return FontName; }

    // Appearance.
    void SetColor(const FVector4& InColor) { Color = InColor; }
    const FVector4& GetColor() const { return Color; }

    void SetFontSize(float InSize) { FontSize = InSize; }
    float GetFontSize() const { return FontSize; }

    // Space mode.
    void SetRenderSpace(ETextRenderSpace InSpace) { RenderSpace = InSpace; }
    ETextRenderSpace GetRenderSpace() const { return RenderSpace; }

    // Screen-space position (pixels).
    void SetScreenPosition(float X, float Y) { ScreenX = X; ScreenY = Y; }
    float GetScreenX() const { return ScreenX; }
    float GetScreenY() const { return ScreenY; }

    // Alignment.
    void SetHorizontalAlignment(ETextHAlign InAlign) { HAlign = InAlign; }
    ETextHAlign GetHorizontalAlignment() const { return HAlign; }

    void SetVerticalAlignment(ETextVAlign InAlign) { VAlign = InAlign; }
    ETextVAlign GetVerticalAlignment() const { return VAlign; }

    // Primitive component interface.
    EPrimitiveType GetPrimitiveType() const override { return PrimitiveType; }
    bool SupportsOutline() const override { return true; }
    static constexpr EPrimitiveType PrimitiveType = EPrimitiveType::EPT_Text;

    void UpdateWorldAABB() const override;
    bool RaycastMesh(const FRay& Ray, FHitResult& OutHitResult);

    FMatrix GetTextMatrix() const;
    int32 GetUTF8Length(const FString& str) const;

private:
    FString Text;
    FName FontName = FName("Default");
    FFontResource* CachedFont = nullptr; // Owned by ResourceManager; referenced only.

    FVector4 Color = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
    float FontSize = 1.0f;
    float Spacing = 0.1f;
    float CharWidth = 0.5f;
    float CharHeight = 0.5f;

    ETextRenderSpace RenderSpace = ETextRenderSpace::World;
    ETextHAlign HAlign = ETextHAlign::Center;
    ETextVAlign VAlign = ETextVAlign::Center;

    float ScreenX = 0.0f;
    float ScreenY = 0.0f;
};
