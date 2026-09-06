#include "Editor/Animation/AnimationSequenceEditorWidget.h"

#include "Animation/AnimNotify.h"
#include "Core/Paths.h"
#include "Editor/Animation/AnimationSequenceEditorDocument.h"
#include "Editor/Animation/AnimationSequencePreviewController.h"
#include "Editor/Animation/AnimationSequenceViewerUiHelpers.h"

#include "ImGui/imgui.h"

#include <algorithm>
#include <filesystem>

namespace
{
    constexpr float NotifyPanelSectionSpacing = 10.0f;
    constexpr float NotifyPanelToolbarSpacing = 6.0f;

    ImVec4 GetValidationSeverityColor(EAnimNotifyValidationSeverity Severity)
    {
        switch (Severity)
        {
        case EAnimNotifyValidationSeverity::Error:
            return ImVec4(0.95f, 0.35f, 0.35f, 1.0f);
        case EAnimNotifyValidationSeverity::Warning:
            return ImVec4(0.98f, 0.76f, 0.32f, 1.0f);
        case EAnimNotifyValidationSeverity::Info:
        default:
            return ImVec4(0.58f, 0.72f, 0.96f, 1.0f);
        }
    }

    const char* GetValidationSeverityLabel(EAnimNotifyValidationSeverity Severity)
    {
        switch (Severity)
        {
        case EAnimNotifyValidationSeverity::Error:
            return "Error";
        case EAnimNotifyValidationSeverity::Warning:
            return "Warning";
        case EAnimNotifyValidationSeverity::Info:
        default:
            return "Info";
        }
    }

    FString MakeRecentNotifyTrackLabel(const FAnimNotifyEvent& NotifyEvent)
    {
        if (NotifyEvent.SourceTrackName.IsValid())
        {
            return NotifyEvent.SourceTrackName.ToString();
        }

        if (NotifyEvent.SourceTrackIndex >= 0)
        {
            return FString("Track ") + std::to_string(NotifyEvent.SourceTrackIndex + 1);
        }

        return "(Unknown Track)";
    }

    FString MakeRecentNotifySourceLabel(const FAnimNotifyEvent& NotifyEvent)
    {
        if (!NotifyEvent.SourceSequencePath.empty())
        {
            const FString ProjectRelativePath = FPaths::ToProjectRelativePath(NotifyEvent.SourceSequencePath);
            const FString& PreferredPath = ProjectRelativePath.empty() ? NotifyEvent.SourceSequencePath : ProjectRelativePath;
            const std::filesystem::path SourcePath(FPaths::ToWide(PreferredPath));
            const FString FileName = FPaths::ToString(SourcePath.filename().wstring());
            return FileName.empty() ? PreferredPath : FileName;
        }

        if (!NotifyEvent.SourceSequenceName.empty())
        {
            return NotifyEvent.SourceSequenceName;
        }

        return "(Current Preview Sequence)";
    }

    bool HasPreviewController(const FAnimationSequencePreviewController* PreviewController)
    {
        return PreviewController != nullptr;
    }

    bool CanInspectRecentFiredNotifies(const FAnimationSequencePreviewController* PreviewController)
    {
        return HasPreviewController(PreviewController);
    }
}

void FAnimationSequenceEditorWidget::RenderNotifyDebugPanel()
{
    ImGui::TextDisabled("Notify Debug");
    ImGui::Dummy(ImVec2(0.0f, NotifyPanelSectionSpacing * 0.5f));
    RenderNotifyDebugTabs();
}

void FAnimationSequenceEditorWidget::RenderNotifyDebugTabs()
{
    if (!ImGui::BeginTabBar("##NotifyDebugTabs"))
    {
        return;
    }

    if (ImGui::BeginTabItem("Validation"))
    {
        RenderNotifyValidationTab();
        ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Recent Fired"))
    {
        RenderRecentFiredNotifyTab();
        ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Event Log"))
    {
        RenderNotifyEventLogTab();
        ImGui::EndTabItem();
    }

    ImGui::EndTabBar();
}

void FAnimationSequenceEditorWidget::RenderNotifyValidationTab()
{
    if (!Document)
    {
        ImGui::TextDisabled("Validation browser is unavailable.");
        return;
    }

    const FAnimNotifyValidationReport Report = Document->BuildDocumentNotifyValidationReport();
    RenderNotifyValidationSummaryBlock(Report);
    ImGui::Dummy(ImVec2(0.0f, NotifyPanelSectionSpacing * 0.4f));
    RenderNotifyValidationFilters();
    ImGui::Dummy(ImVec2(0.0f, NotifyPanelSectionSpacing * 0.5f));
    RenderNotifyValidationMessageList(Report);
}

void FAnimationSequenceEditorWidget::RenderNotifyValidationSummaryBlock(const FAnimNotifyValidationReport& Report) const
{
    AnimationSequenceViewerUi::DrawValidationSeverityCounts(
        Report.ErrorCount,
        Report.WarningCount,
        Report.InfoCount);
}

void FAnimationSequenceEditorWidget::RenderNotifyValidationFilters()
{
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(NotifyPanelToolbarSpacing, NotifyPanelToolbarSpacing));

    auto DrawSeverityFilterButton = [this](const char* Label, ENotifyValidationBrowserSeverityFilter Filter)
    {
        const bool bSelected = ActiveNotifyValidationSeverityFilter == Filter;
        if (bSelected)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.24f, 0.32f, 0.44f, 0.85f));
        }

        const bool bClicked = ImGui::Button(Label);
        if (bSelected)
        {
            ImGui::PopStyleColor();
        }

        if (bClicked)
        {
            ActiveNotifyValidationSeverityFilter = Filter;
        }
    };

    DrawSeverityFilterButton("All", ENotifyValidationBrowserSeverityFilter::All);
    ImGui::SameLine();
    DrawSeverityFilterButton("Errors", ENotifyValidationBrowserSeverityFilter::Errors);
    ImGui::SameLine();
    DrawSeverityFilterButton("Warnings", ENotifyValidationBrowserSeverityFilter::Warnings);
    ImGui::SameLine();
    DrawSeverityFilterButton("Info", ENotifyValidationBrowserSeverityFilter::Info);

    ImGui::PopStyleVar();
}

void FAnimationSequenceEditorWidget::RenderNotifyValidationMessageList(const FAnimNotifyValidationReport& Report)
{
    auto MatchesSeverityFilter = [this](EAnimNotifyValidationSeverity Severity)
    {
        switch (ActiveNotifyValidationSeverityFilter)
        {
        case ENotifyValidationBrowserSeverityFilter::Errors:
            return Severity == EAnimNotifyValidationSeverity::Error;
        case ENotifyValidationBrowserSeverityFilter::Warnings:
            return Severity == EAnimNotifyValidationSeverity::Warning;
        case ENotifyValidationBrowserSeverityFilter::Info:
            return Severity == EAnimNotifyValidationSeverity::Info;
        case ENotifyValidationBrowserSeverityFilter::All:
        default:
            return true;
        }
    };

    TArray<const FAnimNotifyValidationIssue*> FilteredIssues;
    FilteredIssues.reserve(Report.Issues.size());
    for (const FAnimNotifyValidationIssue& Issue : Report.Issues)
    {
        if (MatchesSeverityFilter(Issue.Severity))
        {
            FilteredIssues.push_back(&Issue);
        }
    }

    std::stable_sort(
        FilteredIssues.begin(),
        FilteredIssues.end(),
        [](const FAnimNotifyValidationIssue* Left, const FAnimNotifyValidationIssue* Right)
        {
            if (Left->Severity != Right->Severity)
            {
                return static_cast<int32>(Left->Severity) > static_cast<int32>(Right->Severity);
            }
            if (Left->TrackIndex != Right->TrackIndex)
            {
                return Left->TrackIndex < Right->TrackIndex;
            }
            return Left->EventIndex < Right->EventIndex;
        });

    if (FilteredIssues.empty())
    {
        AnimationSequenceViewerUi::DrawCompactEmptyState(
            Report.Issues.empty()
                ? "No notify validation issues."
                : "No issues match the current severity filter.");
        return;
    }

    if (ImGui::BeginChild("##NotifyValidationBrowser", ImVec2(0.0f, 0.0f), true))
    {
        for (int32 IssueIndex = 0; IssueIndex < static_cast<int32>(FilteredIssues.size()); ++IssueIndex)
        {
            if (IssueIndex > 0)
            {
                ImGui::Dummy(ImVec2(0.0f, NotifyPanelSectionSpacing * 0.4f));
            }

            const FAnimNotifyValidationIssue& Issue = *FilteredIssues[IssueIndex];
            FString TrackLabel = Issue.TrackIndex >= 0
                ? ("Track " + std::to_string(Issue.TrackIndex + 1))
                : FString("Document");
            if (Issue.TrackIndex >= 0)
            {
                if (const FAnimNotifyTrack* Track = Document->GetNotifyTrack(Issue.TrackIndex))
                {
                    const FString TrackName = Track->TrackName.IsValid() ? Track->TrackName.ToString() : FString();
                    if (!TrackName.empty())
                    {
                        TrackLabel += " (" + TrackName + ")";
                    }
                }
            }

            const FString RowLabel =
                "[" + FString(GetValidationSeverityLabel(Issue.Severity)) + "] " +
                TrackLabel + " - " + Issue.Message +
                "##NotifyValidationIssue" + std::to_string(IssueIndex);

            ImGui::PushStyleColor(ImGuiCol_Text, GetValidationSeverityColor(Issue.Severity));
            const bool bRowClicked = ImGui::Selectable(RowLabel.c_str(), false);
            ImGui::PopStyleColor();

            if (bRowClicked && Issue.TrackIndex >= 0 && Issue.StableId.IsValid())
            {
                Document->SelectNotifyByStableId(Issue.TrackIndex, Issue.StableId);
            }

            if (Issue.EventIndex >= 0)
            {
                ImGui::Indent();
                ImGui::TextDisabled("Event %d", Issue.EventIndex + 1);
                ImGui::Unindent();
            }

            if (!Issue.Hint.empty())
            {
                ImGui::Indent();
                ImGui::PushTextWrapPos(0.0f);
                ImGui::TextDisabled("%s", Issue.Hint.c_str());
                ImGui::PopTextWrapPos();
                ImGui::Unindent();
            }
        }
    }
    ImGui::EndChild();
}

void FAnimationSequenceEditorWidget::RenderRecentFiredNotifyTab() const
{
    if (!CanInspectRecentFiredNotifies(PreviewController))
    {
        ImGui::TextDisabled("Preview controller is unavailable.");
        return;
    }

    const TArray<FAnimNotifyEvent>& RecentNotifies = PreviewController->GetRecentFiredNotifyEvents();
    if (RecentNotifies.empty())
    {
        if (ImGui::BeginChild("##RecentFiredNotifyTabEmpty", ImVec2(0.0f, 0.0f), true))
        {
            AnimationSequenceViewerUi::DrawCompactEmptyState(
                "No notify fired during the latest preview update.",
                "Preview playback activity will appear here.",
                NotifyPanelSectionSpacing);
        }
        ImGui::EndChild();
        return;
    }

    if (ImGui::BeginChild("##RecentFiredNotifyTab", ImVec2(0.0f, 0.0f), true))
    {
        constexpr int32 MaxVisibleEntries = 6;
        const int32 VisibleCount = std::min(static_cast<int32>(RecentNotifies.size()), MaxVisibleEntries);
        for (int32 Offset = 0; Offset < VisibleCount; ++Offset)
        {
            if (Offset > 0)
            {
                ImGui::Dummy(ImVec2(0.0f, NotifyPanelSectionSpacing * 0.5f));
            }

            const int32 RecentIndex = static_cast<int32>(RecentNotifies.size()) - 1 - Offset;
            const FAnimNotifyEvent& NotifyEvent = RecentNotifies[RecentIndex];
            const FString NotifyName = NotifyEvent.Name.IsValid() ? NotifyEvent.Name.ToString() : FString("(Unnamed)");
            const FString PhaseText = NotifyEvent.TriggerPhase == EAnimNotifyTriggerPhase::None
                ? AnimNotifyEventTypeToString(NotifyEvent.EventType)
                : AnimNotifyTriggerPhaseToString(NotifyEvent.TriggerPhase);
            const FString NotifyClassName = NotifyEvent.GetResolvedNotifyClassName();
            const FString TrackLabel = MakeRecentNotifyTrackLabel(NotifyEvent);
            const FString SourceLabel = MakeRecentNotifySourceLabel(NotifyEvent);

            ImGui::BulletText("%s [%s] @ %.3fs", NotifyName.c_str(), PhaseText.c_str(), NotifyEvent.Time);
            ImGui::Indent();
            ImGui::TextDisabled(
                "Class: %s | Track: %s | Source: %s",
                NotifyClassName.c_str(),
                TrackLabel.c_str(),
                SourceLabel.c_str());
            if (!NotifyEvent.Payload.empty())
            {
                ImGui::PushTextWrapPos(0.0f);
                ImGui::TextDisabled("Payload: %s", NotifyEvent.Payload.c_str());
                ImGui::PopTextWrapPos();
            }
            ImGui::Unindent();
        }
    }
    ImGui::EndChild();
}

void FAnimationSequenceEditorWidget::RenderNotifyEventLogTab() const
{
    if (ImGui::BeginChild("##NotifyEventLogPlaceholder", ImVec2(0.0f, 0.0f), true))
    {
        AnimationSequenceViewerUi::DrawCompactEmptyState(
            "Notify event log is reserved for future debug output.",
            "Use Validation and Recent Fired for the current notify debug flow.",
            NotifyPanelSectionSpacing);
    }
    ImGui::EndChild();
}
