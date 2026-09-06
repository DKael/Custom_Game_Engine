#include "Editor/Animation/AnimationSequenceViewerUiHelpers.h"

#include "ImGui/imgui.h"

#include <algorithm>

namespace AnimationSequenceViewerUi
{
    void SetFullWidthItem()
    {
        ImGui::SetNextItemWidth(-1.0f);
    }

    void SetClampedWidthItem(float WidthFraction, float MinWidth, float MaxWidth)
    {
        ImGui::SetNextItemWidth(
            std::clamp(
                ImGui::GetContentRegionAvail().x * WidthFraction,
                MinWidth,
                MaxWidth));
    }

    void DrawSectionHeader(const char* Label, float TopSpacing, float BottomSpacing)
    {
        if (TopSpacing > 0.0f)
        {
            ImGui::Dummy(ImVec2(0.0f, TopSpacing));
        }

        ImGui::TextDisabled("%s", Label);
        ImGui::Separator();

        if (BottomSpacing > 0.0f)
        {
            ImGui::Dummy(ImVec2(0.0f, BottomSpacing));
        }
    }

    void DrawCompactEmptyState(
        const char* PrimaryText,
        const char* SecondaryText,
        float TopPadding)
    {
        if (TopPadding > 0.0f)
        {
            ImGui::Dummy(ImVec2(0.0f, TopPadding));
        }

        ImGui::TextDisabled("%s", PrimaryText);
        if (SecondaryText && SecondaryText[0] != '\0')
        {
            ImGui::TextDisabled("%s", SecondaryText);
        }
    }

    void DrawValidationSeverityCounts(int ErrorCount, int WarningCount, int InfoCount)
    {
        ImGui::TextDisabled("%d errors, %d warnings, %d info", ErrorCount, WarningCount, InfoCount);
    }
}
