#pragma once

#include "Core/CoreTypes.h"
#include "Core/Containers/Array.h"
#include "Core/Containers/String.h"
#include "Core/Delegate/MulticastDelegate.h"
#include "Math/Vector2.h"
#include "Engine/Render/Resource/Texture.h"

enum class EUIElementType : uint32
{
    Image,
    Text,
    ProgressBar,
};

// 좌표/크기를 픽셀 절대값으로 쓸지, 뷰포트 비율(0~1)로 쓸지
enum class EUIPositionMode : uint8
{
    Absolute,       // 픽셀 단위 — 리사이즈 시 위치 고정
    Relative,       // 0~1 비율 — 뷰포트 기준 (예: 0.5 = 화면 50%)
    ParentRelative, // 0~1 비율 — 부모 크기 기준 (Panel이 커지면 자식도 같이 커짐)
};

enum class EUIAnchor : uint8
{
    TopLeft,     // LocalPosition = 좌상단 (기본값)
    Center,      // LocalPosition = 중심
    TopRight,
    BottomLeft,
    BottomRight,

};
// CreateImage/CreateText/CreateProgressBar에 전달하는 생성 옵션
// 기본값 그대로 두면 기존 동작(Absolute, TopLeft) 유지
struct FUICreateParams
{
    EUIPositionMode PosMode  = EUIPositionMode::Absolute;
    EUIPositionMode SizeMode = EUIPositionMode::Absolute;
    EUIAnchor       Anchor   = EUIAnchor::TopLeft;

    // 자주 쓰는 조합
    static FUICreateParams FullRelative()  // 위치·크기 모두 비율, 앵커 중앙
    {
        return { EUIPositionMode::Relative, EUIPositionMode::Relative, EUIAnchor::Center };
    }
    static FUICreateParams RelativePos()   // 위치만 비율, 크기는 픽셀, 앵커 중앙
    {
        return { EUIPositionMode::Relative, EUIPositionMode::Absolute, EUIAnchor::Center };
    }
    static FUICreateParams Centered()      // 위치·크기 픽셀, 앵커만 중앙
    {
        return { EUIPositionMode::Absolute, EUIPositionMode::Absolute, EUIAnchor::Center };
    }
    static FUICreateParams ParentRelative() // 위치·크기 모두 부모 크기 기준, 앵커 중앙
    {                                        // Panel이 커지면 자식도 비례해서 커짐
        return { EUIPositionMode::ParentRelative, EUIPositionMode::ParentRelative, EUIAnchor::Center };
    }
};


// FUIElement — 순수 데이터, 렌더링 코드 없음
// LocalPosition은 부모 기준 상대 좌표, GetWorldPosition()으로 최종 스크린 좌표를 얻음
class FUIElement
{
public:
    explicit FUIElement(FVector2 InLocalPos, FVector2 InSize)
        : LocalPosition(InLocalPos), Size(InSize) {}

    virtual ~FUIElement() = default;

    virtual EUIElementType GetType() const = 0;

    // 부모 체인을 타고 올라가며 스크린 좌표 합산
    FVector2 GetWorldPosition() const
    {
        if (Parent)
            return Parent->GetWorldPosition() + LocalPosition;
        return LocalPosition;
    }

    // 부모가 숨겨지면 자식도 숨겨짐
    bool IsWorldVisible() const
    {
        if (!bVisible) return false;
        if (Parent)    return Parent->IsWorldVisible();
        return true;
    }

    // 부모 ZOrder에 로컬 ZOrder 합산 (값 클수록 위에 그려짐)
    float GetWorldZOrder() const
    {
        if (Parent)
            return Parent->GetWorldZOrder() + LocalZOrder;
        return LocalZOrder;
    }

    // Anchor를 반영한 렌더링용 좌상단 좌표 반환
    FVector2 GetRenderPosition() const
    {
        const FVector2 Pos = GetWorldPosition();
        switch (Anchor)
        {
        case EUIAnchor::Center:      return { Pos.X - Size.X * 0.5f, Pos.Y - Size.Y * 0.5f };
        case EUIAnchor::TopRight:    return { Pos.X - Size.X,         Pos.Y                 };
        case EUIAnchor::BottomLeft:  return { Pos.X,                  Pos.Y - Size.Y        };
        case EUIAnchor::BottomRight: return { Pos.X - Size.X,         Pos.Y - Size.Y        };
        default:                     return Pos;  // TopLeft
        }
    }

    void        SetVisible(bool bInVisible) { bVisible = bInVisible; }
    bool        IsVisible()          const  { return bVisible; }
    FUIElement* GetParent()          const  { return Parent; }

    // 상호작용 이벤트 — bInteractable=true 이고 UIManager가 hit test를 통과한 경우에 발생
    TMulticastDelegate<void()> OnHoverEnter;
    TMulticastDelegate<void()> OnHoverExit;
    TMulticastDelegate<void()> OnClick;      // 좌클릭 Down 프레임에 발생

    bool bInteractable = false;  // true 일 때만 UIManager 가 hover/click 검사를 수행
    bool bIsHovered    = false;  // UIManager 내부 상태 추적용 (직접 수정 금지)

    const TArray<FUIElement*>& GetChildren() const { return Children; }

    void AddChild(FUIElement* Child)
    {
        Child->Parent = this;
        Children.push_back(Child);
    }

    void RemoveChild(FUIElement* Child)
    {
        auto It = std::find(Children.begin(), Children.end(), Child);
        if (It != Children.end())
        {
            (*It)->Parent = nullptr;
            Children.erase(It);
        }
    }

public:
    FVector2        LocalPosition;                              // 부모 기준 상대 좌표 (Anchor 기준점)
    FVector2        Size;
    float           RotationDegrees = 0.f;
    float           LocalZOrder    = 0.f;
    EUIAnchor       Anchor         = EUIAnchor::TopLeft;
    EUIPositionMode PositionMode   = EUIPositionMode::Absolute; // 픽셀 or 비율
    EUIPositionMode SizeMode       = EUIPositionMode::Absolute; // 크기도 픽셀 or 비율

private:
    bool                bVisible  = true;
    FUIElement*         Parent    = nullptr;
    TArray<FUIElement*> Children;
};


class FUIImage : public FUIElement
{
public:
    FUIImage(FVector2 InPos, FVector2 InSize, UTexture* InTexture, FVector4 InColor)
        : FUIElement(InPos, InSize), Texture(InTexture), TintColor(InColor) {}

    EUIElementType GetType() const override { return EUIElementType::Image; }

    // 스프라이트 시트에서 특정 셀만 잘라낼 때 사용 (기본값 = 전체 텍스처)
    void SetUV(FVector2 InMin, FVector2 InMax) { UVMin = InMin; UVMax = InMax; }

public:
    UTexture* Texture   = nullptr;
    FVector4  TintColor = { 1.f, 1.f, 1.f, 1.f };
    FVector2  UVMin     = { 0.f, 0.f };
    FVector2  UVMax     = { 1.f, 1.f };
};


class FUIText : public FUIElement
{
public:
    FUIText(FVector2 InPos, FVector2 InSize, const FString& InText, float InFontSize, FVector4 InColor)
        : FUIElement(InPos, InSize), Text(InText), FontSize(InFontSize), Color(InColor) {}

    EUIElementType GetType() const override { return EUIElementType::Text; }

public:
    FString  Text;
    float    FontSize = 16.f;
    FVector4 Color    = { 1.f, 1.f, 1.f, 1.f };
};


class FUIProgressBar : public FUIElement
{
public:
    FUIProgressBar(FVector2 InPos, FVector2 InSize, FVector4 InFillColor, FVector4 InBgColor)
        : FUIElement(InPos, InSize), FillColor(InFillColor), BgColor(InBgColor) {}

    EUIElementType GetType() const override { return EUIElementType::ProgressBar; }

    void  SetValue(float InValue) { Value = InValue < 0.f ? 0.f : (InValue > 1.f ? 1.f : InValue); }
    float GetValue()        const { return Value; }

public:
    FVector4 FillColor = { 0.8f, 0.1f, 0.1f, 1.f };
    FVector4 BgColor   = { 0.2f, 0.2f, 0.2f, 1.f };

private:
    float Value = 1.f;
};
