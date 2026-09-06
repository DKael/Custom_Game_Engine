#include "Editor/Animation/AnimationSequenceEditorWidget.h"

#include "Animation/AnimData/AnimDataModel.h"
#include "Animation/AnimData/AnimSequence.h"
#include "Asset/SkeletalMesh.h"
#include "Editor/Animation/AnimationSequencePreviewController.h"
#include "Editor/Animation/AnimationSequenceViewerUtils.h"
#include "Editor/Viewport/ViewportLayout.h"

#include "ImGui/imgui.h"

#include <algorithm>
#include <string>

namespace
{
    constexpr float PreviewSkeletonSplitterWidth = 4.0f;
}

TArray<FString> FAnimationSequenceEditorWidget::BuildPreviewOverlayLines() const
{
    TArray<FString> PreviewOverlayLines;
    PreviewOverlayLines.push_back(
        AnimationSequenceViewer::IsLiveObject(Sequence) ? Sequence->GetName() : FString("Animation Sequence"));

    const UAnimDataModel* DataModel = AnimationSequenceViewer::GetValidAnimDataModel(Sequence);
    if (DataModel)
    {
        PreviewOverlayLines.push_back(
            std::to_string(DataModel->NumberOfFrames) +
            " frames  |  " +
            std::to_string(DataModel->NumberOfKeys) +
            " keys  |  " +
            std::to_string(static_cast<int32>(DataModel->BoneAnimationTracks.size())) +
            " bone tracks");
        PreviewOverlayLines.push_back(
            std::to_string(static_cast<int32>(DataModel->CurveData.FloatCurves.size())) +
            " curves  |  " +
            std::to_string(AnimationSequenceViewer::GetNotifyEventCount(Sequence)) +
            " notifies");
        PreviewOverlayLines.push_back(
            std::to_string(DataModel->FrameRate.Numerator) +
            " / " +
            std::to_string(DataModel->FrameRate.Denominator) +
            " fps");
    }
    else
    {
        PreviewOverlayLines.push_back("Sequence data model is unavailable.");
    }

    return PreviewOverlayLines;
}

void FAnimationSequenceEditorWidget::RenderPreviewPane(
    float PreviewHeight,
    const TArray<FString>& PreviewOverlayLines)
{
    bool bBlockViewportInputThisFrame = bSuppressEmbeddedPreviewViewportInput;
    ImVec2 PreviewSize(ImGui::GetContentRegionAvail().x, PreviewHeight);
    PreviewSize.x = std::max(PreviewSize.x, 1.0f);
    PreviewSize.y = std::max(PreviewSize.y, 1.0f);

    ImGui::BeginChild("AnimSequencePreviewViewport", PreviewSize, true, ImGuiWindowFlags_NoScrollbar);

    const USkeletalMesh* PreviewMesh = PreviewController ? PreviewController->GetPreviewMesh() : nullptr;
    const FSkeletalMesh* PreviewMeshData = PreviewMesh ? PreviewMesh->GetMeshData() : nullptr;
    if (PreviewMeshData)
    {
        PreviewSkeletonTreeWidth = std::clamp(
            PreviewSkeletonTreeWidth,
            180.0f,
            std::max(180.0f, PreviewSize.x * 0.4f));

        const float ViewportWidth = std::max(
            PreviewSize.x - PreviewSkeletonTreeWidth - PreviewSkeletonSplitterWidth,
            160.0f);
        const float ViewportToolbarHeight = static_cast<float>(FEditorViewportLayout::DefaultViewportToolbarHeight);
        const float ViewportContentHeight = std::max(PreviewSize.y - ViewportToolbarHeight, 80.0f);

        RenderPreviewSkeletonTree(PreviewSize.y);

        ImGui::SameLine(0.0f, 0.0f);
        ImGui::InvisibleButton(
            "##AnimSequencePreviewSkeletonSplitter",
            ImVec2(PreviewSkeletonSplitterWidth, PreviewSize.y));
        if (ImGui::IsItemHovered() || ImGui::IsItemActive())
        {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        }
        ImGui::GetWindowDrawList()->AddRectFilled(
            ImGui::GetItemRectMin(),
            ImGui::GetItemRectMax(),
            ImGui::GetColorU32(
                ImGui::IsItemActive()
                    ? ImGuiCol_SeparatorActive
                    : (ImGui::IsItemHovered() ? ImGuiCol_SeparatorHovered : ImGuiCol_Separator)),
            2.0f);
        if (ImGui::IsItemActive())
        {
            bBlockViewportInputThisFrame = true;
            PreviewSkeletonTreeWidth = std::clamp(
                PreviewSkeletonTreeWidth + ImGui::GetIO().MouseDelta.x,
                180.0f,
                std::max(180.0f, PreviewSize.x * 0.4f));
        }

        ImGui::SameLine(0.0f, 0.0f);
        ImGui::BeginChild("##AnimSequencePreviewViewportPane", ImVec2(ViewportWidth, PreviewSize.y), false, ImGuiWindowFlags_NoScrollbar);
        RenderPreviewViewportToolbar();

        ImGui::BeginChild(
            "##AnimSequencePreviewViewportContent",
            ImVec2(0.0f, ViewportContentHeight),
            false,
            ImGuiWindowFlags_NoScrollbar);
        if (PreviewController)
        {
            PreviewController->SetViewportSize(static_cast<int32>(ViewportWidth), static_cast<int32>(ViewportContentHeight));
        }

        if (PreviewController && PreviewController->HasValidPreview() && PreviewController->GetPreviewSRV())
        {
            RenderPreviewImage(
                ImVec2(ViewportWidth, ViewportContentHeight),
                PreviewOverlayLines,
                bBlockViewportInputThisFrame);
        }
        else
        {
            RenderPreviewFallback(ImVec2(ViewportWidth, ViewportContentHeight), PreviewOverlayLines);
        }
        ImGui::EndChild();
        ImGui::EndChild();
    }
    else
    {
        PreviewSkeletonTreeCachedMesh = nullptr;
        PreviewSkeletonTreeChildren.clear();
        PreviewSkeletonTreeSelectedBoneIndex = -1;
        PendingPreviewBoneTreeOpenStateRoot = -1;
        if (PreviewController)
        {
            PreviewController->ClearPreviewSelection();
        }
        if (PreviewController)
        {
            PreviewController->SetViewportSize(static_cast<int32>(PreviewSize.x), static_cast<int32>(PreviewSize.y));
        }

        if (PreviewController && PreviewController->HasValidPreview() && PreviewController->GetPreviewSRV())
        {
            RenderPreviewImage(PreviewSize, PreviewOverlayLines, bBlockViewportInputThisFrame);
        }
        else
        {
            RenderPreviewFallback(PreviewSize, PreviewOverlayLines);
        }
    }

    ImGui::EndChild();
}
