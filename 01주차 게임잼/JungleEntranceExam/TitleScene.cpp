#include "TitleScene.h"
#include "USceneManager.h"
#include "UsoundManager.h"
#include "URenderer.h"
#include "UGameObject.h"

void UTitleScene::Init()
{
    UGameObjectList.clear();
    SceneType = ESceneType::Title;
}

void UTitleScene::Update(float delta)
{
    
}

void UTitleScene::Render(URenderer render)
{
}

void UTitleScene::Release()
{
    //생성된 모든 UGameObject을 제거
    for (UGameObject* Object : UGameObjectList)
    {
        if (Object != nullptr) {
            delete Object;
        }
    }

    UGameObjectList.clear();
}

void UTitleScene::GameStart()
{
    USoundManager::GetInstance().Play("Start");
    USceneManager::GetInstance().LoadScene(ESceneType::InGame);
}

void UTitleScene::GameEnd()
{
    PostQuitMessage(0);
}

void UTitleScene::UIRender()
{
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    const ImGuiViewport* viewport = ImGui::GetMainViewport();

    // 1. 창을 화면 정중앙에 배치
    ImVec2 centerPos = ImVec2(
        viewport->WorkPos.x + viewport->WorkSize.x * 0.5f,
        viewport->WorkPos.y + viewport->WorkSize.y * 0.5f
    );
    ImGui::SetNextWindowPos(centerPos, ImGuiCond_Always, ImVec2(0.5f, 0.5f));

    ImGuiWindowFlags hudFlags =
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_AlwaysAutoResize;

    ImGui::Begin("Title", nullptr, hudFlags);

    float windowWidth = ImGui::GetWindowWidth();

    const char* titleText = "Brick Breaker";
    ImGui::SetWindowFontScale(8.0f);

    // 글자가 차지할 가로 길이를 계산하고 커서를 이동시킵니다.
    float titleWidth = ImGui::CalcTextSize(titleText).x;
    ImGui::SetCursorPosX((windowWidth - titleWidth) * 0.5f);
    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), titleText);

    ImGui::Text("\n"); // 줄바꿈

    // ==========================================
    // 2. 버튼 중앙 정렬 
    // ==========================================
    ImGui::SetWindowFontScale(4.0f);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.1f, 0.1f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 1.0f, 1.0f, 0.1f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.6f, 1.0f, 1.0f, 0.1f));

    // 통일감을 위해 버튼 크기를 고정합니다.
    float btnWidth = 400.0f;
    float btnHeight = 80.0f;
    ImVec2 btnSize(btnWidth, btnHeight);

    // 게임 시작 버튼
    ImGui::SetCursorPosX((windowWidth - btnWidth) * 0.5f);
    if (ImGui::Button("Game Start", btnSize))
    {
        USceneManager::GetInstance().LoadScene(ESceneType::InGame);
    }

    ImGui::Text("\n");

    // 게임 종료 버튼
    ImGui::SetCursorPosX((windowWidth - btnWidth) * 0.5f);
    if (ImGui::Button("Quit", btnSize))
    {
        GameEnd();
    }

    ImGui::Text("\n");
    ImGui::PopStyleColor(3); // 버튼 스타일 원상복구

    ImGui::SetWindowFontScale(1.5f);
    const char* teamText = "Team 8: Kim Hyung-do,  Kim Ho-jun,  Jo Sang-hyeon,  Kim Sun-myeong";

    float teamWidth = ImGui::CalcTextSize(teamText).x;
    ImGui::SetCursorPosX((windowWidth - teamWidth) * 0.5f);
    ImGui::Text(teamText);

    // ==========================================

    ImGui::End();
    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}
