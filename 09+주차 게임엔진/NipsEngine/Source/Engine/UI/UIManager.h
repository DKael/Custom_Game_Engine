#pragma once

#include "Core/CoreTypes.h"
#include "Core/Containers/Array.h"
#include "Core/Singleton.h"
#include "Math/Vector2.h"
#include "Engine/UI/UIElement.h"

class FUIBatcher;
class FFontBatcher;
struct ID3D11Device;
struct ID3D11DeviceContext;
class FRenderBus;

// FUIManager — 계층적 UIElement 트리를 소유하고 매 프레임 UIBatcher로 렌더링
class FUIManager : public TSingleton<FUIManager>
{
    friend class TSingleton<FUIManager>;

public:
    void Initialize(ID3D11Device* InDevice);
    void Release();

    // 게임 루프에서 호출 — 배처 초기화 후 Element 트리를 순회해 버텍스 데이터 누적
    void Update(float ViewportW, float ViewportH);

    // 렌더 패스에서 호출 — 누적된 버텍스를 GPU에 올리고 DrawCall
    void Flush(ID3D11DeviceContext* Context, const FRenderBus* RenderBus);

    // 창 크기 변경 시 호출 — 뷰포트 크기를 캐싱해 Relative 좌표 변환에 사용
    void OnResize(float NewViewportW, float NewViewportH);

    float GetViewportW() const { return CachedViewportW; }
    float GetViewportH() const { return CachedViewportH; }

    // --- Element 생성 (parent가 nullptr이면 루트로 등록) ---
    // Params 생략 시 기존 동작(Absolute, TopLeft) 유지 — 하위 호환
    FUIImage* CreateImage(FUIElement* Parent,
        FVector2 LocalPos, FVector2 Size,
        UTexture* Texture,
        FVector4 Color  = { 1, 1, 1, 1 },
        FUICreateParams Params = {});

    FUIText* CreateText(FUIElement* Parent,
        FVector2 LocalPos, FVector2 Size,
        const FString& Text,
        float FontSize  = 16.f,
        FVector4 Color  = { 1, 1, 1, 1 },
        FUICreateParams Params = {});

    FUIProgressBar* CreateProgressBar(FUIElement* Parent,
        FVector2 LocalPos, FVector2 Size,
        FVector4 FillColor  = { 0.8f, 0.1f, 0.1f, 1.f },
        FVector4 BgColor    = { 0.2f, 0.2f, 0.2f, 1.f },
        FUICreateParams Params = {});

    // Element와 그 자식 전체를 트리에서 제거하고 메모리 해제
    void DestroyElement(FUIElement* Element);

    // Lua 상태 소멸 전 모든 hover 델리게이트를 비워 dangling sol::function 방지
    void ClearAllHoverDelegates();

private:
    // Element를 UIBatcher에 제출하고 자식을 재귀 순회
    void RenderRecursive(FUIElement* Element, float ViewportW, float ViewportH);

    // Element가 루트인지 판별해 RootElements에 추가
    void RegisterElement(FUIElement* Element, FUIElement* Parent);

    // RootElements에서 제거 (자식 정리는 DestroyRecursive가 처리)
    void DestroyRecursive(FUIElement* Element);

    // 매 Update마다 마우스 위치로 interactable element의 hover 상태를 갱신
    void TickHoverEvents();

private:
    FUIBatcher*         UIBatcher      = nullptr;
    FFontBatcher*       FontBatcher    = nullptr;
    TArray<FUIElement*> RootElements;
    TArray<FUIElement*> AllElements;

    float CachedViewportW = 1280.f;
    float CachedViewportH = 720.f;

    // Relative 모드 좌표를 픽셀로 변환
    FVector2 ResolvePosition(const FUIElement* Element) const;
    FVector2 ResolveSize    (const FUIElement* Element) const;
};
