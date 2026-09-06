#include "UIManager.h"
#include "Engine/Input/InputSystem.h"
#include "Engine/Render/UIBatcher.h"
#include "Engine/Render/FontBatcher.h"
#include "Engine/Scripting/LuaBinder.h"
#include "Core/ResourceManager.h"

void FUIManager::Initialize(ID3D11Device* InDevice)
{
    UIBatcher = new FUIBatcher();
    UIBatcher->Create(InDevice);

    FontBatcher = new FFontBatcher();
    FontBatcher->Create(InDevice);
}

void FUIManager::Release()
{
    for (FUIElement* Element : AllElements)
        delete Element;

    AllElements.clear();
    RootElements.clear();

    if (UIBatcher)
    {
        UIBatcher->Release();
        delete UIBatcher;
        UIBatcher = nullptr;
    }

    if (FontBatcher)
    {
        FontBatcher->Release();
        delete FontBatcher;
        FontBatcher = nullptr;
    }
}

void FUIManager::OnResize(float NewViewportW, float NewViewportH)
{
    CachedViewportW = NewViewportW;
    CachedViewportH = NewViewportH;
}

FVector2 FUIManager::ResolvePosition(const FUIElement* Element) const
{
    FVector2 LocalPx = Element->LocalPosition;

    switch (Element->PositionMode)
    {
    case EUIPositionMode::Relative:
        // 뷰포트 기준 비율
        LocalPx.X *= CachedViewportW;
        LocalPx.Y *= CachedViewportH;
        break;

    case EUIPositionMode::ParentRelative:
        // 부모 크기 기준 비율 — 부모가 없으면 뷰포트로 폴백
        if (const FUIElement* Parent = Element->GetParent())
        {
            const FVector2 ParentPx = ResolveSize(Parent);
            LocalPx.X *= ParentPx.X;
            LocalPx.Y *= ParentPx.Y;
        }
        else
        {
            LocalPx.X *= CachedViewportW;
            LocalPx.Y *= CachedViewportH;
        }
        break;

    default: // Absolute
        break;
    }

    if (const FUIElement* Parent = Element->GetParent())
        return ResolvePosition(Parent) + LocalPx;

    return LocalPx;
}

FVector2 FUIManager::ResolveSize(const FUIElement* Element) const
{
    switch (Element->SizeMode)
    {
    case EUIPositionMode::Relative:
        // 뷰포트 기준 비율
        return { Element->Size.X * CachedViewportW, Element->Size.Y * CachedViewportH };

    case EUIPositionMode::ParentRelative:
        // 부모 크기 기준 비율 — 부모가 없으면 뷰포트로 폴백
        if (const FUIElement* Parent = Element->GetParent())
        {
            const FVector2 ParentPx = ResolveSize(Parent);
            return { Element->Size.X * ParentPx.X, Element->Size.Y * ParentPx.Y };
        }
        return { Element->Size.X * CachedViewportW, Element->Size.Y * CachedViewportH };

    default: // Absolute
        return Element->Size;
    }
}

void FUIManager::Update(float ViewportW, float ViewportH)
{
    if (!UIBatcher || !FontBatcher) return;

    CachedViewportW = ViewportW;
    CachedViewportH = ViewportH;

    UIBatcher->Clear();
    FontBatcher->Clear();
    FontBatcher->ClearUI();

    if (LuaBinder::IsUIMode())
    {
        TickHoverEvents();
    }
    else
    {
        for (FUIElement* Element : AllElements)
        {
            Element->bIsHovered = false;
        }
    }

    std::sort(RootElements.begin(), RootElements.end(),
        [](const FUIElement* A, const FUIElement* B)
        {
            return A->GetWorldZOrder() < B->GetWorldZOrder();
        });

    for (FUIElement* Root : RootElements)
        RenderRecursive(Root, ViewportW, ViewportH);
}

void FUIManager::Flush(ID3D11DeviceContext* Context, const FRenderBus* RenderBus)
{
    if (!UIBatcher || !FontBatcher) return;

    // 쿼드/이미지 먼저
    UIBatcher->Flush(Context, RenderBus);

    // 3D 월드 텍스트 (TextRenderComponent 등)
    const FFontResource* FontRes = FResourceManager::Get().FindFont(FName("Default"));
    if (FontRes)
        FontBatcher->Flush(Context, RenderBus, FontRes);

    // UI 2D 텍스트 — 행렬 없는 셰이더로 별도 렌더링
    if (FontRes)
        FontBatcher->FlushUI(Context, FontRes);
}

void FUIManager::RenderRecursive(FUIElement* Element, float ViewportW, float ViewportH)
{
    if (!Element->IsWorldVisible()) return;

    // Relative 모드 변환 후 Anchor 적용
    const FVector2 ResolvedSize = ResolveSize(Element);
    const FVector2 RawPos       = ResolvePosition(Element);

    // Anchor 오프셋 — GetRenderPosition 대신 직접 계산 (ResolvedSize 반영)
    FVector2 WorldPos = RawPos;
    switch (Element->Anchor)
    {
    case EUIAnchor::Center:      WorldPos = { RawPos.X - ResolvedSize.X * 0.5f, RawPos.Y - ResolvedSize.Y * 0.5f }; break;
    case EUIAnchor::TopRight:    WorldPos = { RawPos.X - ResolvedSize.X,         RawPos.Y                         }; break;
    case EUIAnchor::BottomLeft:  WorldPos = { RawPos.X,                          RawPos.Y - ResolvedSize.Y        }; break;
    case EUIAnchor::BottomRight: WorldPos = { RawPos.X - ResolvedSize.X,         RawPos.Y - ResolvedSize.Y        }; break;
    default: break;
    }

    const FVector2 Size = ResolvedSize;

    switch (Element->GetType())
    {
    case EUIElementType::Image:
    {
        auto* Img = static_cast<FUIImage*>(Element);
        UIBatcher->AddQuad(WorldPos, Size, { ViewportW, ViewportH },
            Img->Texture, Img->TintColor, Img->UVMin, Img->UVMax, Img->RotationDegrees);
        break;
    }
    case EUIElementType::ProgressBar:
    {
        auto* Bar = static_cast<FUIProgressBar*>(Element);

        // 배경 쿼드
        UIBatcher->AddQuad(WorldPos, Size, { ViewportW, ViewportH },
            nullptr, Bar->BgColor);

        // 채움 쿼드 — Value(0~1)만큼 가로 크기 축소
        UIBatcher->AddQuad(WorldPos, { Size.X * Bar->GetValue(), Size.Y },
            { ViewportW, ViewportH }, nullptr, Bar->FillColor);
        break;
    }
    case EUIElementType::Text:
    {
        auto* Txt = static_cast<FUIText*>(Element);
        const float Scale = Txt->FontSize / 20.f;
        // 2D 전용 경로 — NDC 변환 후 행렬 없는 셰이더로 렌더링, Color 전달
        FontBatcher->AddUIText(Txt->Text, WorldPos, ViewportW, ViewportH, Scale, Txt->Color);
        break;
    }
    }

    // 자식 재귀 순회
    for (FUIElement* Child : Element->GetChildren())
        RenderRecursive(Child, ViewportW, ViewportH);
}

FUIImage* FUIManager::CreateImage(FUIElement* Parent,
    FVector2 LocalPos, FVector2 Size,
    UTexture* Texture, FVector4 Color, FUICreateParams Params)
{
    FUIImage* Element     = new FUIImage(LocalPos, Size, Texture, Color);
    Element->PositionMode = Params.PosMode;
    Element->SizeMode     = Params.SizeMode;
    Element->Anchor       = Params.Anchor;
    RegisterElement(Element, Parent);
    return Element;
}

FUIText* FUIManager::CreateText(FUIElement* Parent,
    FVector2 LocalPos, FVector2 Size,
    const FString& Text, float FontSize, FVector4 Color, FUICreateParams Params)
{
    FUIText* Element      = new FUIText(LocalPos, Size, Text, FontSize, Color);
    Element->PositionMode = Params.PosMode;
    Element->SizeMode     = Params.SizeMode;
    Element->Anchor       = Params.Anchor;
    RegisterElement(Element, Parent);
    return Element;
}

FUIProgressBar* FUIManager::CreateProgressBar(FUIElement* Parent,
    FVector2 LocalPos, FVector2 Size,
    FVector4 FillColor, FVector4 BgColor, FUICreateParams Params)
{
    FUIProgressBar* Element = new FUIProgressBar(LocalPos, Size, FillColor, BgColor);
    Element->PositionMode   = Params.PosMode;
    Element->SizeMode       = Params.SizeMode;
    Element->Anchor         = Params.Anchor;
    RegisterElement(Element, Parent);
    return Element;
}

void FUIManager::RegisterElement(FUIElement* Element, FUIElement* Parent)
{
    AllElements.push_back(Element);

    if (Parent)
        Parent->AddChild(Element);   // 부모의 Children에 추가
    else
        RootElements.push_back(Element); // 루트 등록
}

// --- 제거 ---

void FUIManager::DestroyElement(FUIElement* Element)
{
    if (!Element) return;

    // 부모의 Children에서 자신을 제거
    if (FUIElement* Parent = Element->GetParent())
        Parent->RemoveChild(Element);
    else
    {
        auto It = std::find(RootElements.begin(), RootElements.end(), Element);
        if (It != RootElements.end())
            RootElements.erase(It);
    }

    DestroyRecursive(Element);
}

void FUIManager::DestroyRecursive(FUIElement* Element)
{
    // 자식부터 재귀 삭제
    for (FUIElement* Child : Element->GetChildren())
        DestroyRecursive(Child);

    auto It = std::find(AllElements.begin(), AllElements.end(), Element);
    if (It != AllElements.end())
        AllElements.erase(It);

    delete Element;
}

void FUIManager::TickHoverEvents()
{
    const POINT MousePos = InputSystem::Get().GetClientMousePos();
    const float MouseX   = static_cast<float>(MousePos.x);
    const float MouseY   = static_cast<float>(MousePos.y);
    const bool  bClicked = InputSystem::Get().GetKeyDown(VK_LBUTTON);

    // 이벤트를 루프 후 일괄 발화해 콜백 내 DestroyElement 로 인한 iterator 무효화를 방지
    enum class EUIEvent : uint8 { HoverEnter, HoverExit, Click };
    struct FPendingEvent { FUIElement* Element; EUIEvent Type; };
    std::vector<FPendingEvent> Pending;

    for (FUIElement* Element : AllElements)
    {
        if (!Element->bInteractable || !Element->IsWorldVisible()) continue;

        const FVector2 ResolvedPos = ResolvePosition(Element);
        FVector2       HitSize     = ResolveSize(Element);

        // FUIText 는 렌더 높이가 FontSize 픽셀 (CharH = 20 * FontSize/20 = FontSize)
        // Size.Y 가 더 작으면 히트박스가 텍스트 상단만 커버하므로 맞춰 준다
        if (Element->GetType() == EUIElementType::Text)
        {
            const FUIText* Txt = static_cast<const FUIText*>(Element);
            const float    Ch  = Txt->FontSize;
            if (Ch > HitSize.Y) HitSize.Y = Ch;
            const float TextW = Ch * static_cast<float>(Txt->Text.size());
            if (TextW > HitSize.X) HitSize.X = TextW;
        }

        // RenderRecursive 와 동일한 Anchor 오프셋 적용
        FVector2 TopLeft = ResolvedPos;
        switch (Element->Anchor)
        {
        case EUIAnchor::Center:
            TopLeft = { ResolvedPos.X - HitSize.X * 0.5f, ResolvedPos.Y - HitSize.Y * 0.5f };
            break;
        case EUIAnchor::TopRight:
            TopLeft = { ResolvedPos.X - HitSize.X, ResolvedPos.Y };
            break;
        case EUIAnchor::BottomLeft:
            TopLeft = { ResolvedPos.X, ResolvedPos.Y - HitSize.Y };
            break;
        case EUIAnchor::BottomRight:
            TopLeft = { ResolvedPos.X - HitSize.X, ResolvedPos.Y - HitSize.Y };
            break;
        default:
            break;
        }

        const bool bHit = MouseX >= TopLeft.X && MouseX <= TopLeft.X + HitSize.X
                       && MouseY >= TopLeft.Y && MouseY <= TopLeft.Y + HitSize.Y;

        if (bHit && !Element->bIsHovered)
        {
            Element->bIsHovered = true;
            Pending.push_back({ Element, EUIEvent::HoverEnter });
        }
        else if (!bHit && Element->bIsHovered)
        {
            Element->bIsHovered = false;
            Pending.push_back({ Element, EUIEvent::HoverExit });
        }

        if (bHit && bClicked)
            Pending.push_back({ Element, EUIEvent::Click });
    }

    // 루프 종료 후 발화 — 콜백이 DestroyElement 를 호출해도 안전
    // 콜백 중 element 가 삭제됐을 경우 AllElements 에서 빠지므로 find 로 생존 확인
    for (const FPendingEvent& Ev : Pending)
    {
        const bool bAlive = std::find(AllElements.begin(), AllElements.end(), Ev.Element)
                            != AllElements.end();
        if (!bAlive) continue;

        switch (Ev.Type)
        {
        case EUIEvent::HoverEnter: Ev.Element->OnHoverEnter.Broadcast(); break;
        case EUIEvent::HoverExit:  Ev.Element->OnHoverExit.Broadcast();  break;
        case EUIEvent::Click:      Ev.Element->OnClick.Broadcast();      break;
        }
    }
}

void FUIManager::ClearAllHoverDelegates()
{
    for (FUIElement* Element : AllElements)
    {
        Element->OnHoverEnter.RemoveAll();
        Element->OnHoverExit.RemoveAll();
        Element->OnClick.RemoveAll();
        Element->bIsHovered = false;
    }
}
