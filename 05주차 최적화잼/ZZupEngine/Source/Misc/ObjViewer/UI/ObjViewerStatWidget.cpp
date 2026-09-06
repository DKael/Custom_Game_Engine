#include "ObjViewerStatWidget.h"
#include "Misc/ObjViewer/ObjViewerEngine.h"
#include "Engine/Core/CoreTypes.h"
#include "ImGui/imgui.h"

void FObjViewerStatWidget::Render(float DeltaTime)
{
    constexpr ImGuiWindowFlags kFlags =
        ImGuiWindowFlags_NoDecoration      |
        ImGuiWindowFlags_AlwaysAutoResize  |
        ImGuiWindowFlags_NoSavedSettings   |
        ImGuiWindowFlags_NoFocusOnAppearing|
        ImGuiWindowFlags_NoNav             |
        ImGuiWindowFlags_NoMove            |
        ImGuiWindowFlags_NoInputs;

    // 배경 투명도 설정
    ImGui::SetNextWindowBgAlpha(0.3f);
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
    // Stat 창 생성
    if (ImGui::Begin("##ObjViewerStatFPS", nullptr, kFlags))
    {
        // FPS 및 ms 계산 후 출력 (초록색 텍스트)
        const float FPS = (DeltaTime > 0.f) ? (1.f / DeltaTime) : 0.f;
        ImGui::TextColored(ImVec4(0.f, 1.f, 0.f, 1.f), "FPS: %.1f (%.2f ms)", FPS, DeltaTime * 1000.f);
    }
    ImGui::End();

	ImGui::SetNextWindowBgAlpha(0.3f);
    ImGui::SetNextWindowPos(ImVec2(10, 50), ImGuiCond_Always);
    // Stat 창 생성
    if (ImGui::Begin("##Picking Stat", nullptr, kFlags))
    {
        // FPS 및 ms 계산 후 출력 (초록색 텍스트)
        uint64 PickCount = Engine->GetViewportClient().TotalPickCount;
        const double TotalPickTime = Engine->GetViewportClient().TotalPickTime;
        const double LastPickTime = Engine->GetViewportClient().LastPickTime;
        const double AvgPickTime = (PickCount > 0) ? (TotalPickTime / PickCount) : 0.0;
        ImGui::TextColored(ImVec4(0.f, 1.f, 0.f, 1.f), "Pick Count: %d picks", PickCount);
        ImGui::TextColored(ImVec4(0.f, 1.f, 0.f, 1.f), "Total Pick Time: %.3f ms", TotalPickTime);
        ImGui::TextColored(ImVec4(0.f, 1.f, 0.f, 1.f), "Last Pick Time: %.3f ms", LastPickTime);
        ImGui::TextColored(ImVec4(0.f, 1.f, 0.f, 1.f), "Average Pick Time: %.3f ms", AvgPickTime);
    }

    ImGui::End();
}