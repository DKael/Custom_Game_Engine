#include "SkeletalMeshComponent.h"

#include "Animation/AnimInstanceAsset.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/LuaAnimInstance.h"
#include "Asset/Skeleton.h"
#include "Core/ResourceManager.h"
#include "Object/ObjectFactory.h"
#include "Render/Proxy/SkeletalMeshRenderProxy.h"
#include "Render/Mesh/VertexFactory/SkeletalVertexFactoryData.h"
#include "Core/Paths.h"

#include <cstring>

USkeletalMeshComponent::~USkeletalMeshComponent()
{
    ReleaseAnimInstance();
}

UObject* USkeletalMeshComponent::Duplicate()
{
    UObject* DuplicatedObject = FObjectFactory::Get().Create(GetClass()->GetName());
    USkeletalMeshComponent* DuplicatedComponent = Cast<USkeletalMeshComponent>(DuplicatedObject);
    if (!DuplicatedComponent)
    {
        return DuplicatedObject;
    }

    DuplicatedComponent->bDeferAnimationInitialization = true;
    DuplicatedComponent->CopyPropertiesFrom(this);
    DuplicatedComponent->bDeferAnimationInitialization = false;
    DuplicatedComponent->AnimInstance = nullptr;
    DuplicatedComponent->PostDuplicate(this);
    return DuplicatedComponent;
}

void USkeletalMeshComponent::Serialize(FArchive& Ar)
{
    USkinnedMeshComponent::Serialize(Ar);

    if (Ar.IsLoading())
    {
        InitializeAnimation();
    }
}

void USkeletalMeshComponent::PostDuplicate(UObject* Original)
{
    USkinnedMeshComponent::PostDuplicate(Original);
    AnimInstance = nullptr;
    InitializeAnimation();
}

void USkeletalMeshComponent::PostEditProperty(const char* PropertyName)
{
    USkinnedMeshComponent::PostEditProperty(PropertyName);

    if (bDeferAnimationInitialization)
    {
        return;
    }

    if (PropertyName &&
        (std::strcmp(PropertyName, "SkeletalMeshPath") == 0 ||
         std::strcmp(PropertyName, "AnimInstanceAssetPath") == 0))
    {
        InitializeAnimation();
    }
}

void USkeletalMeshComponent::BeginPlay()
{
    USkinnedMeshComponent::BeginPlay();
    InitializeAnimation();
}

void USkeletalMeshComponent::TickComponent(float DeltaTime)
{
    USkinnedMeshComponent::TickComponent(DeltaTime);

    ApplyAnimationPose(DeltaTime);
    EnsureSkinningUpdated();
}

void USkeletalMeshComponent::ResetToBindPose()
{
    InitializePoseFromBindPose();
    MarkSkinningDirty();
}

void USkeletalMeshComponent::SetPreviewSequence(UAnimSequence* InSequence)
{
    if (!InSequence)
    {
        if (UAnimSingleNodeInstance* PreviewInstance = GetPreviewAnimInstance())
        {
            PreviewInstance->SetSequence(nullptr);
        }
        RecentFiredNotifyEvents.clear();
        ResetToBindPose();
        return;
    }

    UAnimSingleNodeInstance* PreviewInstance = EnsurePreviewAnimInstance();
    if (!PreviewInstance)
    {
        return;
    }

    PreviewInstance->SetSequence(InSequence);
    ApplyAnimationPoseFromInstance(PreviewInstance, 0.0f, false);
}

void USkeletalMeshComponent::SetPreviewLooping(bool bInLooping)
{
    if (UAnimSingleNodeInstance* PreviewInstance = GetPreviewAnimInstance())
    {
        PreviewInstance->SetLooping(bInLooping);
    }
}

void USkeletalMeshComponent::SetPreviewPlaying(bool bInPlaying)
{
    UAnimSingleNodeInstance* PreviewInstance = GetPreviewAnimInstance();
    if (!PreviewInstance)
    {
        return;
    }

    if (bInPlaying)
    {
        PreviewInstance->Play();
    }
    else
    {
        PreviewInstance->Pause();
    }
}

void USkeletalMeshComponent::SetPreviewPlayRate(float InPlayRate)
{
    if (UAnimSingleNodeInstance* PreviewInstance = GetPreviewAnimInstance())
    {
        PreviewInstance->SetPlayRate(InPlayRate);
    }
}

void USkeletalMeshComponent::SetPreviewTime(float InTime)
{
    UAnimSingleNodeInstance* PreviewInstance = GetPreviewAnimInstance();
    if (!PreviewInstance)
    {
        return;
    }

    PreviewInstance->SetCurrentTime(InTime);
    ApplyAnimationPoseFromInstance(PreviewInstance, 0.0f, false);
    EnsureSkinningUpdated();
}

float USkeletalMeshComponent::GetPreviewTime() const
{
    const UAnimSingleNodeInstance* PreviewInstance = GetPreviewAnimInstance();
    return PreviewInstance ? PreviewInstance->GetCurrentTime() : 0.0f;
}

float USkeletalMeshComponent::GetPreviewLength() const
{
    const UAnimSingleNodeInstance* PreviewInstance = GetPreviewAnimInstance();
    return PreviewInstance ? PreviewInstance->GetSequenceLength() : 0.0f;
}

UAnimSingleNodeInstance* USkeletalMeshComponent::GetPreviewAnimInstance() const
{
    return Cast<UAnimSingleNodeInstance>(AnimInstance);
}

void USkeletalMeshComponent::TickPreviewAnimation(float DeltaTime)
{
    ApplyAnimationPoseFromInstance(GetPreviewAnimInstance(), DeltaTime, true);
    EnsureSkinningUpdated();
}

void USkeletalMeshComponent::AdvancePreviewAnimation(
    float InTime,
    float DeltaTime,
    bool bWrapped,
    float RangeStart,
    float RangeEnd)
{
    UAnimSingleNodeInstance* PreviewInstance = GetPreviewAnimInstance();
    if (!PreviewInstance)
    {
        return;
    }

    PreviewInstance->AdvancePreviewPlayback(InTime, DeltaTime, bWrapped, RangeStart, RangeEnd);
    ApplyAnimationPoseFromInstance(PreviewInstance, 0.0f, false);
    EnsureSkinningUpdated();
}

void USkeletalMeshComponent::SetBoneLocalTransform(int32 BoneIndex, const FMatrix& NewLocalTransform)
{
    if (BoneIndex < 0 || BoneIndex >= static_cast<int32>(CurrentLocalPose.size()))
    {
        return;
    }

    CurrentLocalPose[BoneIndex] = NewLocalTransform;
    UpdateCurrentGlobalPose();
    MarkSkinningDirty();
}

const FMatrix& USkeletalMeshComponent::GetBoneLocalTransform(int32 BoneIndex) const
{
    static const FMatrix Identity = FMatrix::Identity;

    if (BoneIndex < 0 || BoneIndex >= static_cast<int32>(CurrentLocalPose.size()))
    {
        return Identity;
    }

    return CurrentLocalPose[BoneIndex];
}

FMatrix USkeletalMeshComponent::GetBoneGlobalTransform(int32 BoneIndex) const
{
    if (BoneIndex < 0 || BoneIndex >= static_cast<int32>(CurrentGlobalPose.size()))
    {
        return FMatrix::Identity;
    }

    return CurrentGlobalPose[BoneIndex] * GetWorldMatrix();
}

void USkeletalMeshComponent::SetBoneGlobalTransform(int32 BoneIndex, const FMatrix& NewGlobalTransform)
{
    if (BoneIndex < 0 || BoneIndex >= static_cast<int32>(CurrentLocalPose.size()))
    {
        return;
    }

    if (!SkeletalMesh)
    {
        return;
    }

    USkeleton* Skeleton = SkeletalMesh->GetSkeleton();
    if (!Skeleton || !Skeleton->HasValidSkeletonData())
    {
        return;
    }

    const TArray<FBoneInfo>& Bones = Skeleton->GetBones();
    if (BoneIndex >= static_cast<int32>(Bones.size()))
    {
        return;
    }

    const int32 ParentIndex = Bones[BoneIndex].ParentIndex;

    FMatrix ParentGlobalTransform;
    if (ParentIndex >= 0)
    {
        ParentGlobalTransform = CurrentGlobalPose[ParentIndex] * GetWorldMatrix();
    }
    else
    {
        ParentGlobalTransform = GetWorldMatrix();
    }

    const FMatrix NewLocalTransform = NewGlobalTransform * ParentGlobalTransform.GetInverse();
    SetBoneLocalTransform(BoneIndex, NewLocalTransform);
}

FPrimitiveRenderProxy* USkeletalMeshComponent::CreateRenderProxy()
{
    FSkeletalMeshRenderProxy* Proxy = new FSkeletalMeshRenderProxy;
	Proxy->SkeletalMeshComp = this;
    Proxy->SkelVFData = new FSkeletalVertexFactoryData;

    return Proxy;
}

void USkeletalMeshComponent::SetAnimInstance(UAnimInstance* InAnimInstance)
{
    if (AnimInstance == InAnimInstance)
    {
        return;
    }

    ReleaseAnimInstance();

    AnimInstance = InAnimInstance;
    RecentFiredNotifyEvents.clear();
    if (AnimInstance)
    {
        AnimInstance->SetOwningComponent(this);
        AnimInstance->Initialize();
    }
}

void USkeletalMeshComponent::SetAnimInstanceAssetPath(const FString& InPath)
{
    AnimInstanceAssetPath = InPath;
    InitializeAnimation();
}

ULuaAnimInstance* USkeletalMeshComponent::BindLuaAnimInstance(const FString& ScriptName)
{
    if (ScriptName.empty())
    {
        return nullptr;
    }

    ULuaAnimInstance* LuaAnimInstance = UObjectManager::Get().CreateObject<ULuaAnimInstance>();
    if (!LuaAnimInstance)
    {
        return nullptr;
    }

    LuaAnimInstance->SetScriptName(ScriptName);
    SetAnimInstance(LuaAnimInstance);
    return LuaAnimInstance;
}

void USkeletalMeshComponent::ReleaseAnimInstance()
{
    if (!AnimInstance)
    {
        RecentFiredNotifyEvents.clear();
        return;
    }

    UAnimInstance* InstanceToRelease = AnimInstance;
    AnimInstance = nullptr;
    RecentFiredNotifyEvents.clear();
    InstanceToRelease->SetOwningComponent(nullptr);

    if (UObjectManager::Get().ContainsObject(InstanceToRelease))
    {
        UObjectManager::Get().DestroyObject(InstanceToRelease);
    }
}

UAnimSingleNodeInstance* USkeletalMeshComponent::EnsurePreviewAnimInstance()
{
    if (UAnimSingleNodeInstance* PreviewInstance = GetPreviewAnimInstance())
    {
        return PreviewInstance;
    }

    UAnimSingleNodeInstance* PreviewInstance = UObjectManager::Get().CreateObject<UAnimSingleNodeInstance>();
    SetAnimInstance(PreviewInstance);
    return PreviewInstance;
}

void USkeletalMeshComponent::InitializeAnimation()
{
    if (!SkeletalMesh || !SkeletalMesh->HasValidMeshData())
    {
        RecentFiredNotifyEvents.clear();
        ReleaseAnimInstance();
        return;
    }

    if (Cast<ULuaAnimInstance>(AnimInstance) || Cast<UAnimSingleNodeInstance>(AnimInstance))
    {
        return;
    }

    if (!AnimInstanceAssetPath.empty())
    {
        if (UAnimInstanceAsset* AnimAsset = FResourceManager::Get().LoadAnimInstanceAsset(AnimInstanceAssetPath))
        {
            UAnimInstance* AssetAnimInstance = UObjectManager::Get().CreateObject<UAnimInstance>();
            if (AssetAnimInstance)
            {
                AssetAnimInstance->SetOwningComponent(this);
            }
            if (AssetAnimInstance && AssetAnimInstance->BuildStateMachineFromAsset(AnimAsset))
            {
                SetAnimInstance(AssetAnimInstance);
                return;
            }
            if (AssetAnimInstance)
            {
                UObjectManager::Get().DestroyObject(AssetAnimInstance);
            }
        }
    }

    ReleaseAnimInstance();
}

void USkeletalMeshComponent::ApplyAnimationPose(float DeltaTime)
{
    ApplyAnimationPoseFromInstance(AnimInstance, DeltaTime, true);
}

void USkeletalMeshComponent::ApplyAnimationPoseFromInstance(UAnimInstance* InAnimInstance, float DeltaTime, bool bAdvanceTime)
{
    if (!InAnimInstance)
    {
        RecentFiredNotifyEvents.clear();
        return;
    }

    if (bAdvanceTime)
    {
        InAnimInstance->UpdateAnimation(DeltaTime);
    }

    RecentFiredNotifyEvents = InAnimInstance->GetRecentNotifyEvents();

    FSkeletonPose Pose;
    InAnimInstance->EvaluatePose(Pose);
    ApplyEvaluatedPose(Pose);
}

void USkeletalMeshComponent::ApplyEvaluatedPose(const FSkeletonPose& Pose)
{
    if (Pose.LocalTransforms.empty())
    {
        return;
    }

    CurrentLocalPose.resize(Pose.LocalTransforms.size());
    for (int32 BoneIndex = 0; BoneIndex < static_cast<int32>(Pose.LocalTransforms.size()); ++BoneIndex)
    {
        CurrentLocalPose[BoneIndex] = Pose.LocalTransforms[BoneIndex].ToMatrixWithScale();
    }

    MarkSkinningDirty();
}
