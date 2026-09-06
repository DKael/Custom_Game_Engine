#pragma once

#include "Animation/AnimData/AnimNotifyTypes.h"
#include "Animation/LuaAnimInstance.h"
#include "Component/SkinnedMeshComponent.h"
#include "Generated/SkeletalMeshComponent.generated.h"

class UAnimSequence;
class UAnimSingleNodeInstance;

/**
 * @brief Unreal Engine 스타일에서는 skinned mesh가 skeleton을 이용하는 mesh를 표현하고,
 *        skeletal mesh는 실제로 actor에 붙어서 애니메이션을 붙일 수 있는 component로 사용되고 있으므로
 *        USkeletalMeshComponent 또한 해당 방식대로 우선은 얇게 유지.
 *        핵심 로직들은 대부분 USkinnedMeshComponent로 옮겼습니다.
 */
UCLASS()
class USkeletalMeshComponent : public USkinnedMeshComponent
{
public:
    GENERATED_BODY()

    USkeletalMeshComponent() = default;
    ~USkeletalMeshComponent() override;

    UObject* Duplicate() override;
    void Serialize(FArchive& Ar) override;
    void PostDuplicate(UObject* Original) override;
    void PostEditProperty(const char* PropertyName) override;

    void BeginPlay() override;
    void TickComponent(float DeltaTime) override;

    EPrimitiveType GetPrimitiveType() const override { return EPrimitiveType::EPT_SkeletalMesh; }

    void ResetToBindPose();
    void SetPreviewSequence(UAnimSequence* InSequence);
    void SetPreviewLooping(bool bInLooping);
    void SetPreviewPlaying(bool bInPlaying);
    void SetPreviewPlayRate(float InPlayRate);
    void SetPreviewTime(float InTime);
    float GetPreviewTime() const;
    float GetPreviewLength() const;
    UAnimSingleNodeInstance* GetPreviewAnimInstance() const;
    const TArray<FAnimNotifyEvent>& GetRecentFiredNotifyEvents() const { return RecentFiredNotifyEvents; }
    void TickPreviewAnimation(float DeltaTime);
    void AdvancePreviewAnimation(float InTime, float DeltaTime, bool bWrapped, float RangeStart, float RangeEnd);

    void SetBoneLocalTransform(int32 BoneIndex, const FMatrix& NewLocalTransform);
    const FMatrix& GetBoneLocalTransform(int32 BoneIndex) const;

    FMatrix GetBoneGlobalTransform(int32 BoneIndex) const;
    void SetBoneGlobalTransform(int32 BoneIndex, const FMatrix& NewGlobalTransform);

    void SetAnimInstance(UAnimInstance* InAnimInstance);
    UAnimInstance* GetAnimInstance() const { return AnimInstance; }
    void SetAnimInstanceAssetPath(const FString& InPath);
    const FString& GetAnimInstanceAssetPath() const { return AnimInstanceAssetPath; }
    ULuaAnimInstance* BindLuaAnimInstance(const FString& ScriptName);

protected:
    FPrimitiveRenderProxy* CreateRenderProxy() override;

private:
    void ReleaseAnimInstance();
    UAnimSingleNodeInstance* EnsurePreviewAnimInstance();
    void InitializeAnimation();
    void ApplyAnimationPose(float DeltaTime);
    void ApplyAnimationPoseFromInstance(UAnimInstance* InAnimInstance, float DeltaTime, bool bAdvanceTime);
    void ApplyEvaluatedPose(const FSkeletonPose& Pose);

private:
    UPROPERTY(EditAnywhere, DisplayName = "AnimInstance")
    FString AnimInstanceAssetPath;

    bool bDeferAnimationInitialization = false;

    UAnimInstance* AnimInstance = nullptr;
    TArray<FAnimNotifyEvent> RecentFiredNotifyEvents;
};
