#include "Editor/Animation/AnimationSequenceEditorWidget.h"

#include "Asset/SkeletalMesh.h"
#include "Editor/Animation/AnimationSequencePreviewController.h"
#include "Editor/Viewport/FSceneViewport.h"
#include "Editor/Viewport/SkeletalMeshViewportClient.h"

#include "ImGui/imgui.h"

#include <cstdint>

void FAnimationSequenceEditorWidget::RenderPreviewSkeletonTree(float Height)
{
    ImGui::BeginChild("##AnimSequencePreviewSkeletonTree", ImVec2(PreviewSkeletonTreeWidth, Height), false);
    ImGui::TextDisabled("Skeleton Tree");
    ImGui::Separator();

    const USkeletalMesh* PreviewMesh = PreviewController ? PreviewController->GetPreviewMesh() : nullptr;
    const FSkeletalMesh* MeshData = PreviewMesh ? PreviewMesh->GetMeshData() : nullptr;
    if (!MeshData)
    {
        PreviewSkeletonTreeCachedMesh = nullptr;
        PreviewSkeletonTreeChildren.clear();
        PreviewSkeletonTreeSelectedBoneIndex = -1;
        PendingPreviewBoneTreeOpenStateRoot = -1;
        ImGui::TextDisabled("Preview mesh is unavailable.");
        ImGui::EndChild();
        return;
    }

    if (PreviewSkeletonTreeCachedMesh != MeshData)
    {
        PreviewSkeletonTreeCachedMesh = MeshData;
        PreviewSkeletonTreeSelectedBoneIndex = -1;
        if (PreviewController)
        {
            PreviewController->ClearPreviewSelection();
        }
        RebuildPreviewSkeletonTreeCaches(MeshData);
    }

    ApplyPendingPreviewBoneTreeOpenState(MeshData);
    const TArray<FBoneInfo>& Bones = MeshData->GetBones();
    for (int32 BoneIndex = 0; BoneIndex < static_cast<int32>(Bones.size()); ++BoneIndex)
    {
        if (Bones[BoneIndex].ParentIndex == -1)
        {
            DrawPreviewSkeletonBoneNode(BoneIndex, Bones, PreviewSkeletonTreeChildren);
        }
    }

    if (PreviewSkeletonTreeSelectedBoneIndex >= 0 &&
        PreviewSkeletonTreeSelectedBoneIndex < static_cast<int32>(Bones.size()))
    {
        ImGui::Separator();
        ImGui::TextDisabled("Selected Bone");
        const FString SelectedBoneName = Bones[PreviewSkeletonTreeSelectedBoneIndex].Name.ToString();
        ImGui::Text("%s", SelectedBoneName.c_str());
    }

    ImGui::EndChild();
}

void FAnimationSequenceEditorWidget::DrawPreviewSkeletonBoneNode(
    int32 BoneIndex,
    const TArray<FBoneInfo>& Bones,
    const TArray<TArray<int32>>& Children)
{
    if (BoneIndex < 0 || BoneIndex >= static_cast<int32>(Bones.size()))
    {
        return;
    }

    const FBoneInfo& Bone = Bones[BoneIndex];
    const bool bHasChildren =
        BoneIndex >= 0 &&
        BoneIndex < static_cast<int32>(Children.size()) &&
        !Children[BoneIndex].empty();

    ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (!bHasChildren)
    {
        Flags |= ImGuiTreeNodeFlags_Leaf;
    }
    if (PreviewSkeletonTreeSelectedBoneIndex == BoneIndex)
    {
        Flags |= ImGuiTreeNodeFlags_Selected;
    }

    const FString BoneName = Bone.Name.ToString();
    const bool bOpen =
        ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<intptr_t>(BoneIndex)), Flags, "%s", BoneName.c_str());
    if (ImGui::IsItemClicked())
    {
        PreviewSkeletonTreeSelectedBoneIndex = BoneIndex;
        if (PreviewController)
        {
            PreviewController->SelectPreviewBone(BoneIndex);
        }
        if (FSceneViewport* SceneViewport = PreviewController ? PreviewController->GetSceneViewport() : nullptr)
        {
            if (FSkeletalMeshViewportClient* Client = static_cast<FSkeletalMeshViewportClient*>(SceneViewport->GetClient()))
            {
                Client->GetShowFlags().bShowBones = true;
            }
        }
    }

    if (ImGui::BeginPopupContextItem())
    {
        if (ImGui::MenuItem("Expand Children", nullptr, false, bHasChildren))
        {
            QueuePreviewBoneSubtreeOpenState(BoneIndex, true);
        }
        if (ImGui::MenuItem("Collapse Children", nullptr, false, bHasChildren))
        {
            QueuePreviewBoneSubtreeOpenState(BoneIndex, false);
        }
        ImGui::EndPopup();
    }

    if (!bOpen)
    {
        return;
    }

    for (int32 ChildIndex : Children[BoneIndex])
    {
        DrawPreviewSkeletonBoneNode(ChildIndex, Bones, Children);
    }
    ImGui::TreePop();
}

void FAnimationSequenceEditorWidget::RebuildPreviewSkeletonTreeCaches(const FSkeletalMesh* MeshData)
{
    PreviewSkeletonTreeChildren.clear();
    if (!MeshData)
    {
        return;
    }

    const TArray<FBoneInfo>& Bones = MeshData->GetBones();
    PreviewSkeletonTreeChildren.resize(Bones.size());
    for (int32 BoneIndex = 0; BoneIndex < static_cast<int32>(Bones.size()); ++BoneIndex)
    {
        const int32 ParentIndex = Bones[BoneIndex].ParentIndex;
        if (ParentIndex >= 0 && ParentIndex < static_cast<int32>(PreviewSkeletonTreeChildren.size()))
        {
            PreviewSkeletonTreeChildren[ParentIndex].push_back(BoneIndex);
        }
    }
}

void FAnimationSequenceEditorWidget::QueuePreviewBoneSubtreeOpenState(int32 BoneIndex, bool bOpen)
{
    PendingPreviewBoneTreeOpenStateRoot = BoneIndex;
    bPendingPreviewBoneTreeOpenStateValue = bOpen;
}

void FAnimationSequenceEditorWidget::ApplyPendingPreviewBoneTreeOpenState(const FSkeletalMesh* MeshData)
{
    if (!MeshData || PendingPreviewBoneTreeOpenStateRoot < 0)
    {
        return;
    }

    // Expand/collapse requests are queued and replayed after the tree cache is
    // valid for the current preview mesh. That keeps subtree operations stable
    // even when selection changes trigger a mesh/cache refresh first.
    SetPreviewBoneSubtreeOpenState(
        PendingPreviewBoneTreeOpenStateRoot,
        PreviewSkeletonTreeChildren,
        bPendingPreviewBoneTreeOpenStateValue);
    PendingPreviewBoneTreeOpenStateRoot = -1;
}

void FAnimationSequenceEditorWidget::SetPreviewBoneSubtreeOpenState(
    int32 BoneIndex,
    const TArray<TArray<int32>>& Children,
    bool bOpen)
{
    if (BoneIndex < 0 || BoneIndex >= static_cast<int32>(Children.size()))
    {
        return;
    }

    ImGuiStorage* Storage = ImGui::GetStateStorage();
    if (!Storage)
    {
        return;
    }

    const void* NodePointer = reinterpret_cast<void*>(static_cast<intptr_t>(BoneIndex));
    const ImGuiID NodeId = ImGui::GetID(NodePointer);
    if (bOpen)
    {
        Storage->SetInt(NodeId, 1);
    }

    ImGui::PushID(NodePointer);
    for (int32 ChildIndex : Children[BoneIndex])
    {
        SetPreviewBoneSubtreeOpenState(ChildIndex, Children, bOpen);
    }
    ImGui::PopID();

    if (!bOpen)
    {
        Storage->SetInt(NodeId, 0);
    }
}
