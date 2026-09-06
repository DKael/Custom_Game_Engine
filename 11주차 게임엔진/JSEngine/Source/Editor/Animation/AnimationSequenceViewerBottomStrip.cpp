#include "Editor/Animation/AnimationSequenceEditorWidget.h"

#include "Editor/Animation/AnimationSequenceEditorState.h"
#include "Editor/Animation/AnimationSequencePreviewController.h"
#include "Editor/Animation/AnimationSequenceSequencerLayout.h"
#include "Editor/Animation/AnimationSequenceViewerUiHelpers.h"
#include "Core/ResourceManager.h"
#include "Render/Resource/Texture.h"

#include "ImGui/imgui.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace
{
    constexpr float PlaybackIconButtonSize = 14.0f;
    constexpr float PlaybackControlSpacing = 2.0f;

    struct FBottomStripAvailability
    {
        bool bCanTimelineControl = false;
        bool bCanPlaybackControl = false;
    };

    struct FTransportStripVerticalCenter
    {
        float ContentTopY = 0.0f;
        float ContentHeight = 0.0f;

        float ComputeCursorY(float ItemHeight) const
        {
            return ContentTopY + std::max(0.0f, (ContentHeight - ItemHeight) * 0.5f);
        }

        void AlignCursorY(float ItemHeight) const
        {
            ImGui::SetCursorPosY(ComputeCursorY(ItemHeight));
        }

        ImVec2 ComputeCenteredTextPos(const ImVec2& Min, const ImVec2& Max, const ImVec2& TextSize) const
        {
            return ImVec2(
                std::floor(Min.x + ((Max.x - Min.x) - TextSize.x) * 0.5f),
                std::floor(Min.y + ((Max.y - Min.y) - TextSize.y) * 0.5f));
        }
    };

    const FString& GetPlaybackIconPathJumpToStart()
    {
        static const FString Path = "Asset/UI/PlaybackControls/PlayControlsToFront.png";
        return Path;
    }

    const FString& GetPlaybackIconPathStepPrevious()
    {
        static const FString Path = "Asset/UI/PlaybackControls/PlayControlsToPrevious.png";
        return Path;
    }

    const FString& GetPlaybackIconPathPlayReverse()
    {
        static const FString Path = "Asset/UI/PlaybackControls/PlayControlsPlayReverse.png";
        return Path;
    }

    const FString& GetPlaybackIconPathRecord()
    {
        static const FString Path = "Asset/UI/PlaybackControls/PlayControlsRecord.png";
        return Path;
    }

    const FString& GetPlaybackIconPathPlayForward()
    {
        static const FString Path = "Asset/UI/PlaybackControls/PlayControlsPlayForward.png";
        return Path;
    }

    const FString& GetPlaybackIconPathPause()
    {
        static const FString Path = "Asset/UI/PlaybackControls/PlayControlsPause.png";
        return Path;
    }

    const FString& GetPlaybackIconPathStop()
    {
        static const FString Path = "Asset/UI/PlaybackControls/PlayControlsStop.png";
        return Path;
    }

    const FString& GetPlaybackIconPathStepNext()
    {
        static const FString Path = "Asset/UI/PlaybackControls/PlayControlsToNext.png";
        return Path;
    }

    const FString& GetPlaybackIconPathJumpToEnd()
    {
        static const FString Path = "Asset/UI/PlaybackControls/PlayControlsToEnd.png";
        return Path;
    }

    const FString& GetPlaybackIconPathLoopEnabled()
    {
        static const FString Path = "Asset/UI/PlaybackControls/PlayControlsLooping.png";
        return Path;
    }

    const FString& GetPlaybackIconPathLoopDisabled()
    {
        static const FString Path = "Asset/UI/PlaybackControls/PlayControlsNoLooping.png";
        return Path;
    }

    FString FormatPlaybackRateLabel(float PlaybackRate)
    {
        char Buffer[32];
        snprintf(Buffer, sizeof(Buffer), "x%.2f", std::fabs(PlaybackRate));

        FString Label = Buffer;
        while (Label.size() > 3 && !Label.empty() && Label.back() == '0')
        {
            Label.pop_back();
        }

        if (!Label.empty() && Label.back() == '.')
        {
            Label.push_back('0');
        }

        return Label;
    }

    FString FormatCompactTimelineRangeLabel(const FAnimationSequenceEditorState& State, float Time)
    {
        if (State.TotalFrames > 0)
        {
            return std::to_string(State.TimeToFrameIndex(Time));
        }

        char Buffer[32];
        snprintf(Buffer, sizeof(Buffer), "%.3f", Time);
        FString Label = Buffer;
        while (Label.size() > 2 && !Label.empty() && Label.back() == '0')
        {
            Label.pop_back();
        }
        if (!Label.empty() && Label.back() == '.')
        {
            Label.push_back('0');
        }
        return Label;
    }

    bool IsPlaybackRatePresetSelected(float CurrentRate, float PresetRate)
    {
        return std::fabs(std::fabs(CurrentRate) - PresetRate) <= 0.0001f;
    }

    struct FTransportPlaybackState
    {
        float PlaybackRateMagnitude = 1.0f;
        bool bIsPlaying = false;
        bool bIsReversePlaying = false;
        bool bIsForwardPlaying = false;
    };

    struct FRangeLabelSpec
    {
        FString Text;
        ImVec4 Color;
        const char* Tooltip = nullptr;
    };

    bool HasPreviewController(const FAnimationSequencePreviewController* PreviewController)
    {
        return PreviewController != nullptr;
    }

    FTransportPlaybackState BuildTransportPlaybackState(
        const FAnimationSequencePreviewController* PreviewController,
        const FAnimationSequenceEditorState* EditorState)
    {
        // Snapshot the playback-facing state once before rendering buttons so
        // reverse/play/pause decisions all use the same frame's state.
        const float PlaybackRateMagnitude = std::max(std::fabs(EditorState ? EditorState->PlayRate : 1.0f), 0.0001f);
        const bool bIsPlaying = PreviewController && PreviewController->IsPlaying();
        const bool bIsReversePlaying = bIsPlaying && EditorState && EditorState->PlayRate < 0.0f;
        const bool bIsForwardPlaying = bIsPlaying && EditorState && EditorState->PlayRate >= 0.0f;
        return
        {
            PlaybackRateMagnitude,
            bIsPlaying,
            bIsReversePlaying,
            bIsForwardPlaying
        };
    }

    bool CanControlTimeline(
        const FAnimationSequencePreviewController* PreviewController,
        const FAnimationSequenceEditorState* EditorState)
    {
        return HasPreviewController(PreviewController) && EditorState && EditorState->HasTimelineData();
    }

    bool CanControlPlayback(const FAnimationSequencePreviewController* PreviewController)
    {
        return HasPreviewController(PreviewController) && PreviewController->HasValidPreview();
    }

    FBottomStripAvailability BuildBottomStripAvailability(
        const FAnimationSequencePreviewController* PreviewController,
        const FAnimationSequenceEditorState* EditorState)
    {
        return
        {
            CanControlTimeline(PreviewController, EditorState),
            CanControlPlayback(PreviewController)
        };
    }

    template <size_t BufferSize>
    void CopyStringToBuffer(const FString& Source, std::array<char, BufferSize>& Buffer)
    {
        Buffer.fill('\0');
        strncpy_s(Buffer.data(), Buffer.size(), Source.c_str(), _TRUNCATE);
    }

    template <size_t BufferSize>
    void ClearTimelineRangePopupState(FString& ActivePopupId, std::array<char, BufferSize>& Buffer)
    {
        ActivePopupId.clear();
        Buffer.fill('\0');
    }

    template <size_t BufferSize>
    void SyncTimelineRangePopupBuffer(
        const FAnimationSequenceEditorState& EditorState,
        float CurrentValue,
        std::array<char, BufferSize>& Buffer)
    {
        if (EditorState.TotalFrames > 0)
        {
            CopyStringToBuffer(std::to_string(EditorState.TimeToFrameIndex(CurrentValue)), Buffer);
            return;
        }

        char SecondsBuffer[32];
        snprintf(SecondsBuffer, sizeof(SecondsBuffer), "%.3f", CurrentValue);
        CopyStringToBuffer(SecondsBuffer, Buffer);
    }

    template <typename TApplyEditedTime, size_t BufferSize>
    void RenderTimelineRangeEditPopup(
        const FAnimationSequenceEditorState& EditorState,
        FString& ActivePopupId,
        std::array<char, BufferSize>& Buffer,
        const char* PopupId,
        float CurrentValue,
        TApplyEditedTime&& ApplyEditedTime)
    {
        // All inline range editors share a single text buffer. Track the popup
        // that currently owns it so stale input is cleared deterministically
        // when focus moves to a different range label.
        if (ImGui::BeginPopup(PopupId))
        {
            const bool bFirstPopupFrame = ActivePopupId != PopupId;
            if (bFirstPopupFrame)
            {
                ActivePopupId = PopupId;
                SyncTimelineRangePopupBuffer(EditorState, CurrentValue, Buffer);
                ImGui::SetKeyboardFocusHere();
            }

            const bool bPressedEnter =
                ImGui::InputText(
                    EditorState.TotalFrames > 0 ? "Frame" : "Seconds",
                    Buffer.data(),
                    Buffer.size(),
                    ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);

            bool bApply = bPressedEnter;

            ImGui::Spacing();
            if (ImGui::Button("Apply"))
            {
                bApply = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel"))
            {
                ClearTimelineRangePopupState(ActivePopupId, Buffer);
                ImGui::CloseCurrentPopup();
            }

            if (bApply)
            {
                if (EditorState.TotalFrames > 0)
                {
                    ApplyEditedTime(EditorState.FrameToTime(std::atoi(Buffer.data())));
                }
                else
                {
                    ApplyEditedTime(static_cast<float>(std::atof(Buffer.data())));
                }

                ClearTimelineRangePopupState(ActivePopupId, Buffer);
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
        else if (ActivePopupId == PopupId)
        {
            ClearTimelineRangePopupState(ActivePopupId, Buffer);
        }
    }

    float MeasureRangeLabelsWidth(const TArray<FRangeLabelSpec>& Labels, float LabelSpacing)
    {
        float Width = 0.0f;
        for (int32 Index = 0; Index < static_cast<int32>(Labels.size()); ++Index)
        {
            Width += ImGui::CalcTextSize(Labels[Index].Text.c_str()).x;
            if (Index + 1 < static_cast<int32>(Labels.size()))
            {
                Width += LabelSpacing;
            }
        }
        return Width;
    }

    template <typename TApplyPlaybackRate>
    void RenderPlaybackSpeedPresets(float CurrentRate, TApplyPlaybackRate&& ApplyPlaybackRate)
    {
        constexpr float PresetRates[] = { 0.1f, 0.25f, 0.5f, 0.75f, 1.0f, 2.0f, 5.0f, 10.0f };
        for (float PresetRate : PresetRates)
        {
            const FString Label = FormatPlaybackRateLabel(PresetRate);
            if (ImGui::Selectable(Label.c_str(), IsPlaybackRatePresetSelected(CurrentRate, PresetRate)))
            {
                ApplyPlaybackRate(PresetRate);
            }
        }
    }

    template <typename TApplyPlaybackRate>
    void RenderPlaybackSpeedCustomInput(float& PlaybackSpeedCustomValue, TApplyPlaybackRate&& ApplyPlaybackRate)
    {
        ImGui::TextUnformatted("Custom Speed:");
        AnimationSequenceViewerUi::SetFullWidthItem();
        const bool bChangedByEnter = ImGui::InputFloat(
            "##AnimSequenceCustomPlaybackSpeed",
            &PlaybackSpeedCustomValue,
            0.0f,
            0.0f,
            "%.2f",
            ImGuiInputTextFlags_EnterReturnsTrue);
        if ((bChangedByEnter || ImGui::IsItemDeactivatedAfterEdit()) && std::isfinite(PlaybackSpeedCustomValue))
        {
            PlaybackSpeedCustomValue = std::max(std::fabs(PlaybackSpeedCustomValue), 0.0001f);
            ApplyPlaybackRate(PlaybackSpeedCustomValue);
        }
    }
}

void FAnimationSequenceEditorWidget::RenderBottomControlStrip(float Height, float LeftPaneWidth, float RightPaneWidth)
{
    // The strip is split into transport and timeline-range panes. Each pane
    // owns its detailed controls so this function stays focused on layout.
    const FBottomStripAvailability Availability = BuildBottomStripAvailability(PreviewController, EditorState);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::BeginChild(
        "##AnimationSequenceBottomControlStrip",
        ImVec2(0.0f, Height),
        true,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    ImGui::BeginChild(
        "##AnimationSequenceBottomTransportPane",
        ImVec2(LeftPaneWidth, Height),
        false,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    RenderTransportControls(
        Availability.bCanTimelineControl,
        Availability.bCanPlaybackControl,
        Height);
    ImGui::EndChild();

    ImGui::SameLine(0.0f, 0.0f);
    ImGui::BeginChild(
        "##AnimationSequenceBottomTransportSpacer",
        ImVec2(FAnimationSequenceSequencerLayout::OutlinerSplitterWidth, Height),
        false,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::EndChild();

    ImGui::SameLine(0.0f, 0.0f);
    ImGui::BeginChild(
        "##AnimationSequenceBottomTimelinePane",
        ImVec2(RightPaneWidth, Height),
        false,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    RenderTimelineRangeControls(Height);
    ImGui::EndChild();

    ImGui::EndChild();
    ImGui::PopStyleVar();
}

void FAnimationSequenceEditorWidget::RenderTransportControls(bool bCanTimelineControl, bool bCanPlaybackControl, float StripHeight)
{
    (void)StripHeight;
    EnsurePlaybackControlIconsLoaded();
    const ImVec2 ButtonSize(PlaybackIconButtonSize, PlaybackIconButtonSize);
    const FTransportStripVerticalCenter VerticalCenter
    {
        0.0f,
        std::max(ImGui::GetWindowHeight(), ImGui::GetContentRegionAvail().y)
    };
    const float IconButtonItemHeight = ButtonSize.y + ImGui::GetStyle().FramePadding.y * 2.0f;

    auto CenterButtonCursor = [&]()
    {
        VerticalCenter.AlignCursorY(IconButtonItemHeight);
    };
    const FTransportPlaybackState PlaybackState = BuildTransportPlaybackState(PreviewController, EditorState);

    auto DrawCenteredTransportButton = [&](const char* ButtonId, UTexture* IconTexture, const char* FallbackLabel, const char* Tooltip, bool bSelected = false)
    {
        CenterButtonCursor();
        return DrawPlaybackControlButton(ButtonId, IconTexture, FallbackLabel, Tooltip, ButtonSize, bSelected);
    };

    if (!bCanTimelineControl)
    {
        ImGui::BeginDisabled();
    }

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(PlaybackControlSpacing, PlaybackControlSpacing));
    ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.5f, 0.5f));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.30f, 0.40f, 0.54f, 0.48f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.24f, 0.32f, 0.44f, 0.60f));

    auto RenderTransportButtonRow = [&]()
    {
        if (DrawCenteredTransportButton(
                "##AnimSequenceJumpToStart",
                PlaybackControlIcons.JumpToStart,
                "|<",
                "Jump to Start") &&
            PreviewController)
        {
            PreviewController->JumpToStart();
            EditorState->SetCurrentTime(PreviewController->GetCurrentTime(), false);
        }
        ImGui::SameLine();
        if (DrawCenteredTransportButton(
                "##AnimSequenceStepPrevious",
                PlaybackControlIcons.StepPrevious,
                "<F",
                "Previous Frame") &&
            PreviewController)
        {
            PreviewController->StepToPreviousFrame();
            EditorState->SetCurrentTime(PreviewController->GetCurrentTime(), false);
        }
        ImGui::SameLine();

        if (!bCanPlaybackControl)
        {
            ImGui::BeginDisabled();
        }

        if (DrawCenteredTransportButton(
                "##AnimSequenceReverse",
                PlaybackState.bIsReversePlaying ? PlaybackControlIcons.Pause : PlaybackControlIcons.PlayReverse,
                PlaybackState.bIsReversePlaying ? "Pause" : "Rev",
                PlaybackState.bIsReversePlaying ? "Pause Reverse Playback" : "Play Reverse") &&
            PreviewController)
        {
            if (PlaybackState.bIsReversePlaying)
            {
                PreviewController->Pause();
                EditorState->SetCurrentTime(PreviewController->GetCurrentTime(), false);
            }
            else
            {
                PreviewController->SetPlayRate(-PlaybackState.PlaybackRateMagnitude);
                EditorState->PlayRate = PreviewController->GetPlayRate();
                PreviewController->Play();
            }
        }
        ImGui::SameLine();
        DrawCenteredTransportButton(
            "##AnimSequenceRecord",
            PlaybackControlIcons.Record,
            "Rec",
            "Record Placeholder");
        ImGui::SameLine();
        if (DrawCenteredTransportButton(
                "##AnimSequencePlay",
                PlaybackState.bIsForwardPlaying ? PlaybackControlIcons.Pause : PlaybackControlIcons.PlayForward,
                PlaybackState.bIsForwardPlaying ? "Pause" : "Play",
                PlaybackState.bIsForwardPlaying ? "Pause Playback" : "Play") &&
            PreviewController)
        {
            if (PlaybackState.bIsForwardPlaying)
            {
                PreviewController->Pause();
                EditorState->SetCurrentTime(PreviewController->GetCurrentTime(), false);
            }
            else
            {
                PreviewController->SetPlayRate(PlaybackState.PlaybackRateMagnitude);
                EditorState->PlayRate = PreviewController->GetPlayRate();
                PreviewController->Play();
            }
        }

        if (!bCanPlaybackControl)
        {
            ImGui::EndDisabled();
        }

        ImGui::SameLine();
        if (DrawCenteredTransportButton(
                "##AnimSequenceStepNext",
                PlaybackControlIcons.StepNext,
                "F>",
                "Next Frame") &&
            PreviewController)
        {
            PreviewController->StepToNextFrame();
            EditorState->SetCurrentTime(PreviewController->GetCurrentTime(), false);
        }
        ImGui::SameLine();
        if (DrawCenteredTransportButton(
                "##AnimSequenceJumpToEnd",
                PlaybackControlIcons.JumpToEnd,
                ">|",
                "Jump to End") &&
            PreviewController)
        {
            PreviewController->JumpToEnd();
            EditorState->SetCurrentTime(PreviewController->GetCurrentTime(), false);
        }
    };

    auto RenderLoopToggleControl = [&]()
    {
        bool bLooping = EditorState->bLoop;
        if (DrawCenteredTransportButton(
                "##AnimSequenceLoopToggle",
                bLooping ? PlaybackControlIcons.LoopEnabled : PlaybackControlIcons.LoopDisabled,
                "Loop",
                bLooping ? "Loop Enabled" : "Loop Disabled",
                bLooping) &&
            PreviewController)
        {
            const bool bNewLooping = !bLooping;
            PreviewController->SetLooping(bNewLooping);
            EditorState->bLoop = bNewLooping;
        }
    };

    auto RenderPlaybackSpeedControl = [&]()
    {
        CenterButtonCursor();
        const FString PlaybackRateButtonLabel = FormatPlaybackRateLabel(EditorState->PlayRate);
        const float PlaybackRateButtonWidth =
            std::max(
                50.0f,
                ImGui::CalcTextSize(PlaybackRateButtonLabel.c_str()).x +
                    ImGui::GetStyle().FramePadding.x * 2.0f +
                    8.0f);
        const ImVec2 PlaybackRateButtonSize(PlaybackRateButtonWidth, IconButtonItemHeight);
        const bool bPlaybackRatePressed =
            ImGui::InvisibleButton("##AnimSequencePlaybackRateButton", PlaybackRateButtonSize);
        const ImVec2 PlaybackRateButtonMin = ImGui::GetItemRectMin();
        const ImVec2 PlaybackRateButtonMax = ImGui::GetItemRectMax();
        const bool bPlaybackRateHovered = ImGui::IsItemHovered();
        const bool bPlaybackRateActive = ImGui::IsItemActive();
        ImDrawList* PlaybackRateDrawList = ImGui::GetWindowDrawList();
        if (bPlaybackRateHovered || bPlaybackRateActive)
        {
            PlaybackRateDrawList->AddRectFilled(
                PlaybackRateButtonMin,
                PlaybackRateButtonMax,
                ImGui::GetColorU32(bPlaybackRateActive ? ImGuiCol_ButtonActive : ImGuiCol_ButtonHovered),
                ImGui::GetStyle().FrameRounding);
        }

        const ImVec2 PlaybackRateTextSize = ImGui::CalcTextSize(PlaybackRateButtonLabel.c_str());
        const ImVec2 PlaybackRateTextPos =
            VerticalCenter.ComputeCenteredTextPos(PlaybackRateButtonMin, PlaybackRateButtonMax, PlaybackRateTextSize);
        PlaybackRateDrawList->AddText(
            PlaybackRateTextPos,
            ImGui::GetColorU32(ImGuiCol_Text),
            PlaybackRateButtonLabel.c_str());

        if (bPlaybackRatePressed)
        {
            PlaybackSpeedCustomValue = std::fabs(EditorState->PlayRate);
            ImGui::OpenPopup("##AnimSequencePlaybackSpeedPopup");
        }
        if (bPlaybackRateHovered)
        {
            ImGui::SetTooltip("%s", "Playback Speed Options");
        }
        RenderPlaybackSpeedPopup();
    };

    RenderTransportButtonRow();
    ImGui::SameLine(0.0f, PlaybackControlSpacing);
    RenderLoopToggleControl();
    ImGui::SameLine(0.0f, PlaybackControlSpacing);
    RenderPlaybackSpeedControl();

    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar(2);

    if (!bCanTimelineControl)
    {
        ImGui::EndDisabled();
    }
}

void FAnimationSequenceEditorWidget::RenderTimelineRangeControls(float StripHeight)
{
    (void)StripHeight;
    const FTransportStripVerticalCenter VerticalCenter
    {
        0.0f,
        std::max(ImGui::GetWindowHeight(), ImGui::GetContentRegionAvail().y)
    };
    const float SliderHeight = 12.0f;
    if (!EditorState || !EditorState->HasTimelineData())
    {
        VerticalCenter.AlignCursorY(ImGui::GetTextLineHeight());
        ImGui::TextDisabled("Timeline view controls are unavailable.");
        return;
    }

    const float MaximumTime = std::max(EditorState->GetMaxTime(), EditorState->GetMinimumVisibleRange());
    const float SequenceStart = EditorState->GetTimelineRangeStart();
    const float SequenceEnd = EditorState->GetTimelineRangeEnd();
    const float PlaybackStart = EditorState->GetPlaybackRangeStart();
    const float PlaybackEnd = EditorState->GetPlaybackRangeEnd();

    const FString ViewStartLabel = FormatCompactTimelineRangeLabel(*EditorState, EditorState->VisibleTimeStart);
    const FString PlaybackStartLabel = FormatCompactTimelineRangeLabel(*EditorState, PlaybackStart);
    const FString SequenceStartLabel = FormatCompactTimelineRangeLabel(*EditorState, SequenceStart);
    const FString SequenceEndLabel = FormatCompactTimelineRangeLabel(*EditorState, SequenceEnd);
    const FString PlaybackEndLabel = FormatCompactTimelineRangeLabel(*EditorState, PlaybackEnd);
    const FString ViewEndLabel = FormatCompactTimelineRangeLabel(*EditorState, EditorState->VisibleTimeEnd);

    const ImVec4 NeutralColor(0.62f, 0.66f, 0.74f, 1.0f);
    const ImVec4 PlaybackStartColor(0.37f, 0.78f, 0.42f, 1.0f);
    const ImVec4 PlaybackEndColor(0.90f, 0.30f, 0.28f, 1.0f);
    const TArray<FRangeLabelSpec> LeftLabels =
    {
        { ViewStartLabel, NeutralColor, "View start" },
        { PlaybackStartLabel, PlaybackStartColor, "Playback start" },
        { SequenceStartLabel, NeutralColor, "Sequence start" }
    };
    const TArray<FRangeLabelSpec> RightLabels =
    {
        { SequenceEndLabel, NeutralColor, "Sequence end" },
        { PlaybackEndLabel, PlaybackEndColor, "Playback end" },
        { ViewEndLabel, NeutralColor, "View end" }
    };

    constexpr float EdgePadding = 32.0f;
    constexpr float LabelSpacing = 12.0f;
    constexpr float GroupSpacing = 20.0f;
    const float LeftLabelsWidth = MeasureRangeLabelsWidth(LeftLabels, LabelSpacing);
    const float RightLabelsWidth = MeasureRangeLabelsWidth(RightLabels, LabelSpacing);
    const float AvailableWidth = std::max(ImGui::GetContentRegionAvail().x, 1.0f);
    const float ReservedWidth =
        LeftLabelsWidth +
        RightLabelsWidth +
        EdgePadding * 2.0f +
        GroupSpacing * 2.0f;
        const float SliderWidth = std::max(36.0f, AvailableWidth - ReservedWidth);

    auto DrawRangeLabel = [&](const char* PopupId, const FRangeLabelSpec& LabelSpec, bool bEditable, auto&& ApplyEditedTime, float CurrentValue)
    {
        VerticalCenter.AlignCursorY(ImGui::GetTextLineHeight());
        ImGui::TextColored(LabelSpec.Color, "%s", LabelSpec.Text.c_str());
        if (bEditable && ImGui::IsItemClicked(ImGuiMouseButton_Left))
        {
            ImGui::OpenPopup(PopupId);
        }
        if (ImGui::IsItemHovered() && LabelSpec.Tooltip)
        {
            ImGui::SetTooltip("%s%s", LabelSpec.Tooltip, bEditable ? " (Click to edit)" : "");
        }

        RenderTimelineRangeEditPopup(
            *EditorState,
            ActiveTimelineRangeEditPopupId,
            TimelineRangeEditBuffer,
            PopupId,
            CurrentValue,
            ApplyEditedTime);
    };

    auto RenderLeftRangeSummary = [&]()
    {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + EdgePadding);
        DrawRangeLabel(
            "##AnimSequenceViewStartEdit",
            LeftLabels[0],
            true,
            [&](float NewTime) { EditorState->SetVisibleRange(NewTime, EditorState->VisibleTimeEnd); },
            EditorState->VisibleTimeStart);
        ImGui::SameLine(0.0f, LabelSpacing);
        DrawRangeLabel(
            "##AnimSequencePlaybackStartDisplay",
            LeftLabels[1],
            true,
            [&](float NewTime)
            {
                if (PreviewController)
                {
                    PreviewController->SetPlaybackRange(NewTime, EditorState->GetPlaybackRangeEnd());
                    EditorState->SetPlaybackRange(
                        PreviewController->GetPlaybackRangeStart(),
                        PreviewController->GetPlaybackRangeEnd());
                }
            },
            PlaybackStart);
        ImGui::SameLine(0.0f, LabelSpacing);
        DrawRangeLabel(
            "##AnimSequenceSequenceStartEdit",
            LeftLabels[2],
            true,
            [&](float NewTime) { EditorState->SetTimelineRange(NewTime, EditorState->GetTimelineRangeEnd()); },
            SequenceStart);
    };

    auto RenderRightRangeSummary = [&](float SliderStartXValue)
    {
        ImGui::SetCursorPosX(SliderStartXValue + SliderWidth + GroupSpacing);
        DrawRangeLabel(
            "##AnimSequenceSequenceEndEdit",
            RightLabels[0],
            true,
            [&](float NewTime) { EditorState->SetTimelineRange(EditorState->GetTimelineRangeStart(), NewTime); },
            SequenceEnd);
        ImGui::SameLine(0.0f, LabelSpacing);
        DrawRangeLabel(
            "##AnimSequencePlaybackEndDisplay",
            RightLabels[1],
            true,
            [&](float NewTime)
            {
                if (PreviewController)
                {
                    PreviewController->SetPlaybackRange(EditorState->GetPlaybackRangeStart(), NewTime);
                    EditorState->SetPlaybackRange(
                        PreviewController->GetPlaybackRangeStart(),
                        PreviewController->GetPlaybackRangeEnd());
                }
            },
            PlaybackEnd);
        ImGui::SameLine(0.0f, LabelSpacing);
        DrawRangeLabel(
            "##AnimSequenceViewEndEdit",
            RightLabels[2],
            true,
            [&](float NewTime) { EditorState->SetVisibleRange(EditorState->VisibleTimeStart, NewTime); },
            EditorState->VisibleTimeEnd);
    };

    RenderLeftRangeSummary();

    ImGui::SameLine(0.0f, GroupSpacing);
    const float SliderStartX = ImGui::GetCursorPosX();
    VerticalCenter.AlignCursorY(SliderHeight);
    ImGui::InvisibleButton(
        "##AnimationSequenceTimelineViewRangeBar",
        ImVec2(SliderWidth, SliderHeight),
        ImGuiButtonFlags_MouseButtonLeft);

    enum class ERangeBarDragMode : uint8
    {
        None,
        StartHandle,
        EndHandle,
        Thumb
    };

    static ERangeBarDragMode ActiveDragMode = ERangeBarDragMode::None;
    static float DragMouseStartX = 0.0f;
    static float DragVisibleStart = 0.0f;
    static float DragVisibleEnd = 0.0f;

    const ImVec2 SliderMin = ImGui::GetItemRectMin();
    const ImVec2 SliderMax = ImGui::GetItemRectMax();
    const float SliderPixelWidth = std::max(SliderMax.x - SliderMin.x, 1.0f);
    const float HandleHalfWidth = 32.0f;

    auto TimeToSliderX = [&](float TimeValue)
    {
        const float Alpha = MaximumTime > 0.0f ? std::clamp(TimeValue / MaximumTime, 0.0f, 1.0f) : 0.0f;
        return SliderMin.x + SliderPixelWidth * Alpha;
    };

    float VisibleStart = EditorState->VisibleTimeStart;
    float VisibleEnd = EditorState->VisibleTimeEnd;
    const float ThumbMinX = TimeToSliderX(VisibleStart);
    const float ThumbMaxX = TimeToSliderX(VisibleEnd);

    if (ImGui::IsItemActivated())
    {
        DragMouseStartX = ImGui::GetIO().MousePos.x;
        DragVisibleStart = VisibleStart;
        DragVisibleEnd = VisibleEnd;

        const float MouseX = ImGui::GetIO().MousePos.x;
        const bool bNearStartHandle = std::fabs(MouseX - ThumbMinX) <= HandleHalfWidth + 2.0f;
        const bool bNearEndHandle = std::fabs(MouseX - ThumbMaxX) <= HandleHalfWidth + 2.0f;
        const bool bInsideThumb = MouseX >= ThumbMinX && MouseX <= ThumbMaxX;

        if (bNearStartHandle)
        {
            ActiveDragMode = ERangeBarDragMode::StartHandle;
        }
        else if (bNearEndHandle)
        {
            ActiveDragMode = ERangeBarDragMode::EndHandle;
        }
        else if (bInsideThumb)
        {
            ActiveDragMode = ERangeBarDragMode::Thumb;
        }
        else
        {
            ActiveDragMode = MouseX < ThumbMinX ? ERangeBarDragMode::StartHandle : ERangeBarDragMode::EndHandle;
        }
    }

    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
    {
        const float DeltaPixels = ImGui::GetIO().MousePos.x - DragMouseStartX;
        const float DeltaTime = MaximumTime > 0.0f ? (DeltaPixels / SliderPixelWidth) * MaximumTime : 0.0f;

        switch (ActiveDragMode)
        {
        case ERangeBarDragMode::StartHandle:
            EditorState->SetVisibleRange(DragVisibleStart + DeltaTime, DragVisibleEnd);
            break;
        case ERangeBarDragMode::EndHandle:
            EditorState->SetVisibleRange(DragVisibleStart, DragVisibleEnd + DeltaTime);
            break;
        case ERangeBarDragMode::Thumb:
            EditorState->SetVisibleRange(DragVisibleStart + DeltaTime, DragVisibleEnd + DeltaTime);
            break;
        case ERangeBarDragMode::None:
        default:
            break;
        }
    }

    if (ImGui::IsItemDeactivated())
    {
        ActiveDragMode = ERangeBarDragMode::None;
    }

    VisibleStart = EditorState->VisibleTimeStart;
    VisibleEnd = EditorState->VisibleTimeEnd;
    const float DrawThumbMinX = TimeToSliderX(VisibleStart);
    const float DrawThumbMaxX = TimeToSliderX(VisibleEnd);

    ImDrawList* DrawList = ImGui::GetWindowDrawList();
    const float TrackMidY = (SliderMin.y + SliderMax.y) * 0.5f;
    DrawList->AddRectFilled(
        ImVec2(SliderMin.x, TrackMidY - 2.0f),
        ImVec2(SliderMax.x, TrackMidY + 2.0f),
        IM_COL32(72, 76, 84, 255),
        2.0f);
    DrawList->AddRectFilled(
        ImVec2(DrawThumbMinX, SliderMin.y + 1.0f),
        ImVec2(DrawThumbMaxX, SliderMax.y - 1.0f),
        IM_COL32(154, 156, 160, 255),
        3.0f);
    DrawList->AddRect(
        ImVec2(DrawThumbMinX, SliderMin.y + 1.0f),
        ImVec2(DrawThumbMaxX, SliderMax.y - 1.0f),
        IM_COL32(206, 208, 214, 255),
        3.0f);
    DrawList->AddRectFilled(
        ImVec2(DrawThumbMinX - 8.5f, SliderMin.y),
        ImVec2(DrawThumbMinX + 8.5f, SliderMax.y),
        IM_COL32(226, 228, 232, 255),
        1.0f);
    DrawList->AddRectFilled(
        ImVec2(DrawThumbMaxX - 8.5f, SliderMin.y),
        ImVec2(DrawThumbMaxX + 8.5f, SliderMax.y),
        IM_COL32(226, 228, 232, 255),
        1.0f);

    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("%s", "Adjust the visible timeline range");
    }

    RenderRightRangeSummary(SliderStartX);
    ImGui::Dummy(ImVec2(EdgePadding, 0.0f));
}

void FAnimationSequenceEditorWidget::RenderPlaybackSpeedPopup()
{
    if (!EditorState)
    {
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(252.0f, 0.0f), ImGuiCond_Appearing);
    if (!ImGui::BeginPopup("##AnimSequencePlaybackSpeedPopup"))
    {
        return;
    }

    ImGui::Spacing();
    AnimationSequenceViewerUi::DrawSectionHeader("PLAYBACK SPEED", 0.0f, 0.0f);

    auto ApplyPlaybackRate = [&](float NewRate)
    {
        const float Direction = EditorState->PlayRate < 0.0f ? -1.0f : 1.0f;
        const float AppliedRate = std::max(std::fabs(NewRate), 0.0001f) * Direction;
        PlaybackSpeedCustomValue = std::fabs(NewRate);
        if (PreviewController)
        {
            PreviewController->SetPlayRate(AppliedRate);
            EditorState->PlayRate = PreviewController->GetPlayRate();
        }
        else
        {
            EditorState->PlayRate = AppliedRate;
        }
    };

    RenderPlaybackSpeedPresets(EditorState->PlayRate, ApplyPlaybackRate);

    ImGui::Separator();
    RenderPlaybackSpeedCustomInput(PlaybackSpeedCustomValue, ApplyPlaybackRate);

    ImGui::Separator();
    ImGui::Checkbox("Snap to Frames", &EditorState->bSnapToFrames);

    ImGui::EndPopup();
}

void FAnimationSequenceEditorWidget::EnsurePlaybackControlIconsLoaded()
{
    if (PlaybackControlIcons.bAttemptedLoad)
    {
        return;
    }

    PlaybackControlIcons.bAttemptedLoad = true;
    FResourceManager& ResourceManager = FResourceManager::Get();
    PlaybackControlIcons.JumpToStart = ResourceManager.LoadTexture(GetPlaybackIconPathJumpToStart());
    PlaybackControlIcons.StepPrevious = ResourceManager.LoadTexture(GetPlaybackIconPathStepPrevious());
    PlaybackControlIcons.PlayReverse = ResourceManager.LoadTexture(GetPlaybackIconPathPlayReverse());
    PlaybackControlIcons.Record = ResourceManager.LoadTexture(GetPlaybackIconPathRecord());
    PlaybackControlIcons.PlayForward = ResourceManager.LoadTexture(GetPlaybackIconPathPlayForward());
    PlaybackControlIcons.Pause = ResourceManager.LoadTexture(GetPlaybackIconPathPause());
    PlaybackControlIcons.Stop = ResourceManager.LoadTexture(GetPlaybackIconPathStop());
    PlaybackControlIcons.StepNext = ResourceManager.LoadTexture(GetPlaybackIconPathStepNext());
    PlaybackControlIcons.JumpToEnd = ResourceManager.LoadTexture(GetPlaybackIconPathJumpToEnd());
    PlaybackControlIcons.LoopEnabled = ResourceManager.LoadTexture(GetPlaybackIconPathLoopEnabled());
    PlaybackControlIcons.LoopDisabled = ResourceManager.LoadTexture(GetPlaybackIconPathLoopDisabled());
}

bool FAnimationSequenceEditorWidget::DrawPlaybackControlButton(
    const char* ButtonId,
    UTexture* IconTexture,
    const char* FallbackLabel,
    const char* Tooltip,
    const ImVec2& Size,
    bool bSelected) const
{
    (void)bSelected;

    bool bPressed = false;
    if (IconTexture && IconTexture->GetSRV())
    {
        bPressed = ImGui::ImageButton(ButtonId, reinterpret_cast<void*>(IconTexture->GetSRV()), Size);
    }
    else
    {
        bPressed = ImGui::Button(FallbackLabel, Size);
    }

    if (ImGui::IsItemHovered() && Tooltip)
    {
        ImGui::SetTooltip("%s", Tooltip);
    }

    return bPressed;
}
