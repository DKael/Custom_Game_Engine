#include "UClearScene.h"
#include "URenderer.h"
#include "USceneManager.h"

int UClearScene::FinalScore = 0;

void UClearScene::Render(URenderer renderer)
{
}

void UClearScene::UIRender()
{
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImVec2 center = ImVec2(viewport->WorkPos.x + viewport->WorkSize.x * 0.5f,
        viewport->WorkPos.y + viewport->WorkSize.y * 0.5f);

    ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));

    ImGui::Begin("ClearScreen", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoBackground);

    ImGui::SetWindowFontScale(6.0f);
    float windowWidth = ImGui::GetWindowWidth();
    float titleWidth = ImGui::CalcTextSize("STAGE CLEAR!").x;

    ImGui::SetCursorPosX((windowWidth - titleWidth) * 0.5f);
    ImGui::TextColored(ImVec4(1, 1, 0, 1), "STAGE CLEAR!"); 

    ImGui::SetWindowFontScale(2.5f);
    ImGui::Text("YOUR FINAL SCORE: %d", FinalScore);

    ImGui::Separator(); 

    ImVec2 btnSize(250, 50);
    ImGui::SetCursorPosX((ImGui::GetWindowWidth() - btnSize.x) * 0.5f);
    if (ImGui::Button("RESTART", btnSize))
    {
        USceneManager::GetInstance().LoadScene(ESceneType::InGame);
    }

    ImGui::SetCursorPosX((ImGui::GetWindowWidth() - btnSize.x) * 0.5f);
    if (ImGui::Button("TITLE MENU", btnSize))
    {
        USceneManager::GetInstance().LoadScene(ESceneType::Title);
    }

    ImGui::End();
    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

void UClearScene::Init()
{
    SceneType = ESceneType::Clear;
}

void UClearScene::Release()
{
}
