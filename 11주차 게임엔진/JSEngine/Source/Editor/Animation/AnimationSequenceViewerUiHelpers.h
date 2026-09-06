#pragma once

namespace AnimationSequenceViewerUi
{
    void SetFullWidthItem();
    void SetClampedWidthItem(float WidthFraction, float MinWidth, float MaxWidth);
    void DrawSectionHeader(const char* Label, float TopSpacing, float BottomSpacing);
    void DrawCompactEmptyState(
        const char* PrimaryText,
        const char* SecondaryText = nullptr,
        float TopPadding = 0.0f);
    void DrawValidationSeverityCounts(int ErrorCount, int WarningCount, int InfoCount);
}
