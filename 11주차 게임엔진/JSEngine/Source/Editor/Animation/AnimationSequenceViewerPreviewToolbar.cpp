#include "Editor/Animation/AnimationSequenceEditorWidget.h"

#include "Editor/Animation/AnimationSequencePreviewController.h"
#include "Editor/UI/EditorMainPanelViewportToolbarHelpers.h"
#include "Editor/Viewport/EditorViewportClient.h"
#include "Editor/Viewport/FSceneViewport.h"
#include "Editor/Viewport/SkeletalMeshViewportClient.h"
#include "Editor/Viewport/ViewportLayout.h"

#include "ImGui/imgui.h"

#include <algorithm>
#include <cstdio>

namespace
{
    constexpr float PreviewToolbarPaddingX = 8.0f;
}

void FAnimationSequenceEditorWidget::RenderPreviewViewportToolbar()
{
    constexpr ImGuiWindowFlags ToolbarFlags =
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse;
    constexpr float ToolbarHeight = static_cast<float>(FEditorViewportLayout::DefaultViewportToolbarHeight);

    ImGui::BeginChild("##AnimSequencePreviewViewportToolbar", ImVec2(0.0f, ToolbarHeight), false, ToolbarFlags);
    ImGui::SetCursorPosY(std::max(0.0f, (ToolbarHeight - ImGui::GetFrameHeight()) * 0.5f));
    ImGui::SetCursorPosX(PreviewToolbarPaddingX);

    FSceneViewport* SceneViewport = PreviewController ? PreviewController->GetSceneViewport() : nullptr;
    FEditorViewportClient* Client = SceneViewport ? SceneViewport->GetClient() : nullptr;
    FSkeletalMeshViewportClient* SkeletalViewportClient = Client ? static_cast<FSkeletalMeshViewportClient*>(Client) : nullptr;

    if (!Client)
    {
        ImGui::TextDisabled("Viewport controls are available after preview initialization.");
        ImGui::EndChild();
        return;
    }

    char ViewportTypeLabel[64] = {};
    snprintf(
        ViewportTypeLabel,
        sizeof(ViewportTypeLabel),
        "%s v",
        FEditorMainPanelViewportToolbarHelpers::GetViewportTypeName(Client->GetViewportType()));
    if (ImGui::Button(ViewportTypeLabel))
    {
        ImGui::OpenPopup("##AnimSequencePreviewViewportTypePopup");
    }
    if (ImGui::BeginPopup("##AnimSequencePreviewViewportTypePopup"))
    {
        static constexpr EEditorViewportType ViewportTypes[] = {
            EVT_Perspective,
            EVT_OrthoTop,
            EVT_OrthoBottom,
            EVT_OrthoFront,
            EVT_OrthoBack,
            EVT_OrthoLeft,
            EVT_OrthoRight,
        };
        for (EEditorViewportType Type : ViewportTypes)
        {
            if (ImGui::MenuItem(
                FEditorMainPanelViewportToolbarHelpers::GetViewportTypeName(Type),
                nullptr,
                Client->GetViewportType() == Type))
            {
                Client->SetViewportType(Type);
                Client->ApplyCameraMode();
            }
        }
        ImGui::EndPopup();
    }

    ImGui::SameLine();
    char CameraSpeedLabel[48] = {};
    snprintf(
        CameraSpeedLabel,
        sizeof(CameraSpeedLabel),
        "Cam %.1fx v",
        FEditorMainPanelViewportToolbarHelpers::GetCameraSpeedMultiplier(Client));
    if (ImGui::Button(CameraSpeedLabel))
    {
        ImGui::OpenPopup("##AnimSequencePreviewCameraSpeedPopup");
    }
    if (ImGui::BeginPopup("##AnimSequencePreviewCameraSpeedPopup"))
    {
        float SpeedMultiplier = FEditorMainPanelViewportToolbarHelpers::GetCameraSpeedMultiplier(Client);
        if (ImGui::SliderFloat(
            "Speed",
            &SpeedMultiplier,
            0.01f,
            FEditorMainPanelViewportToolbarHelpers::MaxCameraSpeedMultiplier,
            "%.2fx"))
        {
            FEditorMainPanelViewportToolbarHelpers::SetCameraSpeedMultiplier(Client, SpeedMultiplier);
        }
        ImGui::EndPopup();
    }

    ImGui::SameLine();
    if (ImGui::Button("Show v"))
    {
        ImGui::OpenPopup("##AnimSequencePreviewShowPopup");
    }
    if (ImGui::BeginPopup("##AnimSequencePreviewShowPopup"))
    {
        if (SkeletalViewportClient)
        {
            FSkeletalViewerShowFlags& ShowFlags = SkeletalViewportClient->GetShowFlags();
            ImGui::MenuItem("Bones", nullptr, &ShowFlags.bShowBones);
        }
        else
        {
            ImGui::TextDisabled("Show flags are unavailable.");
        }
        ImGui::EndPopup();
    }

    ImGui::EndChild();
}
