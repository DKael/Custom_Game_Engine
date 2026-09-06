#include "AnimInstance.h"

#include "Asset/Skeleton.h"
#include "Animation/AnimNotify.h"
#include "Animation/AnimInstanceAsset.h"
#include "Animation/AnimationStateMachine.h"
#include "Animation/Compression/AclCompression.h"
#include "Component/SkeletalMeshComponent.h"
#include "Core/Logging/Log.h"
#include "Core/Paths.h"
#include "Core/ResourceManager.h"
#include "Core/Logging/Stats.h"
#include "Object/ObjectFactory.h"

#include <algorithm>
#include <cmath>

namespace
{
    bool IsLiveObject(const UObject* Object)
    {
        return Object != nullptr && UObjectManager::Get().ContainsObject(Object);
    }

    const UAnimDataModel* GetValidAnimDataModel(const UAnimSequence* Sequence)
    {
        if (!IsLiveObject(Sequence))
        {
            return nullptr;
        }

        const UAnimDataModel* DataModel = Sequence->DataModel;
        return IsLiveObject(DataModel) ? DataModel : nullptr;
    }

    bool IsNotifyTriggeredForward(float NotifyTime, float StartTime, float EndTime, bool bIncludeStartBoundary = false)
    {
        return (bIncludeStartBoundary ? NotifyTime >= StartTime : NotifyTime > StartTime) && NotifyTime <= EndTime;
    }

    bool IsNotifyTriggeredBackward(float NotifyTime, float StartTime, float EndTime, bool bIncludeStartBoundary = false)
    {
        return (bIncludeStartBoundary ? NotifyTime <= StartTime : NotifyTime < StartTime) && NotifyTime >= EndTime;
    }

    FString NormalizeSkeletonPathForComparison(const FString& Path)
    {
        return FPaths::Normalize(FPaths::ToProjectRelativePath(Path));
    }

    float GetNotifyStateTriggerTime(
        const FAnimNotifyEvent& NotifyEvent,
        float SequenceLength,
        bool bForwardPlayback,
        EAnimNotifyTriggerPhase TriggerPhase)
    {
        const float StartTime = std::clamp(NotifyEvent.Time, 0.0f, SequenceLength);
        const float EndTime = std::clamp(NotifyEvent.Time + NotifyEvent.Duration, 0.0f, SequenceLength);
        if (TriggerPhase == EAnimNotifyTriggerPhase::Begin)
        {
            return bForwardPlayback ? StartTime : EndTime;
        }
        if (TriggerPhase == EAnimNotifyTriggerPhase::End)
        {
            return bForwardPlayback ? EndTime : StartTime;
        }

        return StartTime;
    }
}

UAnimInstance::~UAnimInstance()
{
    ClearActiveAnimNotifyStates(false);
}

void UAnimInstance::Initialize()
{
	NativeInitializeAnimation();

    if (StateMachine && StateMachine->GetCurrentSequence() && !CurrentSequence)
    {
        SetSequence(StateMachine->GetCurrentSequence());
        SetLooping(StateMachine->GetCurrentStateLooping());
        PlayRate = StateMachine->GetCurrentStatePlayRate();
    }
}

void UAnimInstance::SetOwningComponent(USkinnedMeshComponent* InOwner)
{
    Owner = InOwner;
}

void UAnimInstance::SetSequence(UAnimSequence* InSequence)
{
    if (!PrepareSequenceForPlayback(InSequence))
    {
        return;
    }

	CurrentSequence = InSequence;
	CurrentTime = 0.0f;
	bPlaying = true;

	// Next Sequence 초기화
	NextSequence = nullptr;
	NextTime = 0.0f;
    bNextLoop = bLoop;
    NextPlayRate = PlayRate;
	BlendFactor = 0.0f;
    RecentNotifyEvents.clear();
    ClearActiveAnimNotifyStates(false);
}
void UAnimInstance::SetNextSequence(UAnimSequence* InNext, float InBlendSpeed)
{
    if (!InNext || InNext == CurrentSequence || !PrepareSequenceForPlayback(InNext))
    {
        return;
    }

    NextSequence = InNext;
    NextTime = 0.0f;
    bNextLoop = bLoop;
    NextPlayRate = PlayRate;
    BlendFactor = 0.0f;
    BlendSpeed = InBlendSpeed;
}

void UAnimInstance::SetCurrentTime(float InCurrentTime)
{
    const auto ClampTimeForSequence = [this](const UAnimSequence* Sequence, float InTime)
    {
        const float Length = GetSequenceLength(Sequence);
        return Length > 0.0f ? std::clamp(InTime, 0.0f, Length) : 0.0f;
    };

    // Explicit seeks should land on the requested endpoint even when looping is enabled.
    CurrentTime = ClampTimeForSequence(CurrentSequence, InCurrentTime);
    NextTime = ClampTimeForSequence(NextSequence, InCurrentTime);
    RecentNotifyEvents.clear();
    ClearActiveAnimNotifyStates(false);
}

void UAnimInstance::Play()
{
    if (HasValidSequence())
    {
        bPlaying = true;
    }
}

void UAnimInstance::Pause()
{
    bPlaying = false;
}

void UAnimInstance::Stop()
{
    CurrentTime = 0.0f;
    NextTime = 0.0f;
    NextSequence = nullptr;
    bNextLoop = bLoop;
    NextPlayRate = PlayRate;
    BlendFactor = 0.0f;
    bPlaying = false;
    RecentNotifyEvents.clear();
    ClearActiveAnimNotifyStates(false);
}

float UAnimInstance::GetSequenceLength() const
{
    return GetSequenceLength(CurrentSequence);
}

bool UAnimInstance::HasValidSequence() const
{
    return GetValidAnimDataModel(CurrentSequence) != nullptr;
}

void UAnimInstance::DispatchGameplayAnimNotifyEvent(
    const UAnimSequence* Sequence,
    const FAnimNotifyEvent& NotifyEvent,
    const FString& EventName,
    const FString& Payload)
{
    if (EventName.empty())
    {
        return;
    }

    NativeGameplayAnimNotifyEvent(Sequence, NotifyEvent, EventName, Payload);
}

void UAnimInstance::DispatchCameraShakeAnimNotify(
    const UAnimSequence* Sequence,
    const FAnimNotifyEvent& NotifyEvent,
    const FString& Shake,
    float Scale)
{
    if (Shake.empty())
    {
        return;
    }

    NativeCameraShakeAnimNotify(Sequence, NotifyEvent, Shake, Scale);
}

void UAnimInstance::DispatchFootstepSurfaceAnimNotify(
    const UAnimSequence* Sequence,
    const FAnimNotifyEvent& NotifyEvent,
    const FString& EventName,
    const FString& SurfaceName,
    const FString& Payload,
    const FVector& HitLocation)
{
    if (EventName.empty())
    {
        return;
    }

    NativeFootstepSurfaceAnimNotify(Sequence, NotifyEvent, EventName, SurfaceName, Payload, HitLocation);
}

void UAnimInstance::DispatchPlayVFXAnimNotify(
    const UAnimSequence* Sequence,
    const FAnimNotifyEvent& NotifyEvent,
    const FString& Effect,
    const FVector& Location,
    const FName& SocketName,
    bool bAttached,
    float Scale)
{
    if (Effect.empty())
    {
        return;
    }

    NativePlayVFXAnimNotify(Sequence, NotifyEvent, Effect, Location, SocketName, bAttached, Scale);
}

void UAnimInstance::DispatchSpawnDecalAnimNotify(
    const UAnimSequence* Sequence,
    const FAnimNotifyEvent& NotifyEvent,
    const FString& Decal,
    const FVector& Location,
    const FVector& Normal,
    const FName& SocketName,
    float Size,
    float Lifetime,
    bool bAlignToHitNormal)
{
    if (Decal.empty())
    {
        return;
    }

    NativeSpawnDecalAnimNotify(Sequence, NotifyEvent, Decal, Location, Normal, SocketName, Size, Lifetime, bAlignToHitNormal);
}

void UAnimInstance::SetStateMachine(UAnimationStateMachine* InStateMachine)
{
    StateMachine = InStateMachine;
    if (StateMachine)
    {
        StateMachine->SetOwningAnimInstance(this);
    }
}

UAnimationStateMachine* UAnimInstance::CreateStateMachine()
{
    UAnimationStateMachine* NewStateMachine = UObjectManager::Get().CreateObject<UAnimationStateMachine>();
    SetStateMachine(NewStateMachine);
    return NewStateMachine;
}

bool UAnimInstance::PrepareSequenceForPlayback(UAnimSequence* Sequence)
{
    if (!Sequence)
    {
        return false;
    }

    if (!Owner)
    {
        return true;
    }

    const USkeletalMesh* Mesh = Owner->GetSkeletalMesh();
    if (!Mesh || !Mesh->HasValidMeshData())
    {
        return false;
    }

    USkeleton* MeshSkeleton = Mesh->GetSkeleton();
    if (!MeshSkeleton || !MeshSkeleton->HasValidSkeletonData())
    {
        return false;
    }

    const FString MeshSkeletonPath = FPaths::Normalize(Mesh->GetSkeletonAssetPath());
    const FString SequenceSkeletonPath = FPaths::Normalize(Sequence->GetSkeletonAssetPath());
    if (!MeshSkeletonPath.empty() &&
        !SequenceSkeletonPath.empty() &&
        MeshSkeletonPath != SequenceSkeletonPath)
    {
        TArray<int32> TrackToBoneIndex;
        if (!BuildCompatibleSkeletonRemap(Sequence, MeshSkeleton, TrackToBoneIndex))
        {
            SequenceBoneTrackRemaps.erase(Sequence);
            UE_LOG_WARNING("[AnimInstance] Sequence skeleton mismatch | Sequence=%s | MeshSkeleton=%s | SequenceSkeleton=%s",
                Sequence->GetName().c_str(),
                MeshSkeletonPath.c_str(),
                SequenceSkeletonPath.c_str());
            return false;
        }

        if (!IsSkeletonPathCompatible(MeshSkeleton, MeshSkeletonPath, SequenceSkeletonPath))
        {
            UE_LOG("[AnimInstance] Auto-compatible skeleton remap | Sequence=%s | MeshSkeleton=%s | SequenceSkeleton=%s",
                Sequence->GetName().c_str(),
                MeshSkeletonPath.c_str(),
                SequenceSkeletonPath.c_str());
        }

        SequenceBoneTrackRemaps[Sequence] = TrackToBoneIndex;
    }
    else
    {
        SequenceBoneTrackRemaps.erase(Sequence);
    }

    Sequence->SetSkeleton(MeshSkeleton);
    if (Sequence->GetSkeletonAssetPath().empty())
    {
        Sequence->SetSkeletonAssetPath(MeshSkeletonPath);
    }

    return true;
}

bool UAnimInstance::IsSkeletonPathCompatible(
    const USkeleton* TargetSkeleton,
    const FString& TargetSkeletonPath,
    const FString& SourceSkeletonPath) const
{
    if (!TargetSkeleton || SourceSkeletonPath.empty())
    {
        return false;
    }

    const FString NormalizedTargetPath = NormalizeSkeletonPathForComparison(TargetSkeletonPath);
    const FString NormalizedSourcePath = NormalizeSkeletonPathForComparison(SourceSkeletonPath);
    if (NormalizedTargetPath.empty() || NormalizedSourcePath.empty())
    {
        return false;
    }

    if (NormalizedTargetPath == NormalizedSourcePath)
    {
        return true;
    }

    for (const FString& CompatibleSkeletonPath : TargetSkeleton->GetCompatibleSkeletons())
    {
        if (NormalizeSkeletonPathForComparison(CompatibleSkeletonPath) == NormalizedSourcePath)
        {
            return true;
        }
    }

    return false;
}

bool UAnimInstance::BuildCompatibleSkeletonRemap(
    const UAnimSequence* Sequence,
    const USkeleton* TargetSkeleton,
    TArray<int32>& OutTrackToBoneIndex) const
{
    const UAnimDataModel* Model = GetValidAnimDataModel(Sequence);
    if (!Model || !TargetSkeleton || !TargetSkeleton->HasValidSkeletonData())
    {
        return false;
    }

    OutTrackToBoneIndex.clear();
    OutTrackToBoneIndex.reserve(Model->BoneAnimationTracks.size());

    bool bMappedAnyTrack = false;
    int32 MissingTrackCount = 0;
    FString MissingTrackSample;
    for (const FBoneAnimationTrack& Track : Model->BoneAnimationTracks)
    {
        if (!Track.Name.IsValid())
        {
            UE_LOG_WARNING("[AnimInstance] Compatible skeleton remap failed: invalid track bone name | Sequence=%s",
                Sequence ? Sequence->GetName().c_str() : "<null>");
            return false;
        }

        const int32 TargetBoneIndex = TargetSkeleton->FindBoneIndex(Track.Name);
        if (TargetBoneIndex < 0)
        {
            OutTrackToBoneIndex.push_back(-1);
            ++MissingTrackCount;
            if (MissingTrackCount <= 8)
            {
                if (!MissingTrackSample.empty())
                {
                    MissingTrackSample += ", ";
                }
                MissingTrackSample += Track.Name.ToString();
            }
            continue;
        }

        OutTrackToBoneIndex.push_back(TargetBoneIndex);
        bMappedAnyTrack = true;
    }

    if (MissingTrackCount > 0)
    {
        UE_LOG_WARNING("[AnimInstance] Compatible skeleton remap skipped missing target bones | Sequence=%s | Mapped=%d | Missing=%d | MissingSample=%s",
            Sequence ? Sequence->GetName().c_str() : "<null>",
            static_cast<int32>(Model->BoneAnimationTracks.size()) - MissingTrackCount,
            MissingTrackCount,
            MissingTrackSample.c_str());
    }

    return bMappedAnyTrack || Model->BoneAnimationTracks.empty();
}

bool UAnimInstance::BuildStateMachineFromAsset(UAnimInstanceAsset* Asset)
{
    if (!Asset)
    {
        return false;
    }

    UAnimationStateMachine* NewStateMachine = CreateStateMachine();
    if (!NewStateMachine)
    {
        return false;
    }

    for (const FAnimInstanceParameterAssetData& Param : Asset->Parameters)
    {
        switch (Param.Type)
        {
        case EAnimStateParameterType::Bool:
            NewStateMachine->RegisterParameterBool(Param.Name, Param.BoolDefault);
            break;
        case EAnimStateParameterType::Float:
            NewStateMachine->RegisterParameterFloat(Param.Name, Param.FloatDefault);
            break;
        case EAnimStateParameterType::Int:
            NewStateMachine->RegisterParameterInt(Param.Name, Param.IntDefault);
            break;
        case EAnimStateParameterType::Trigger:
            NewStateMachine->RegisterParameterTrigger(Param.Name);
            break;
        default:
            break;
        }
    }

    bool bAddedAnyState = false;
    for (const FAnimInstanceStateAssetData& State : Asset->States)
    {
        UAnimSequence* Sequence = FResourceManager::Get().LoadAnimSequence(State.AnimSequencePath);
        if (PrepareSequenceForPlayback(Sequence) &&
            NewStateMachine->AddState(State.Name, Sequence, State.bLoop, State.PlayRate))
        {
            bAddedAnyState = true;
        }
    }

    if (!bAddedAnyState)
    {
        return false;
    }

    if (!NewStateMachine->SetEntryState(Asset->EntryState) && !Asset->States.empty())
    {
        NewStateMachine->SetEntryState(Asset->States[0].Name);
    }

    for (const FAnimInstanceTransitionAssetData& Transition : Asset->Transitions)
    {
        switch (Transition.ConditionType)
        {
        case EAnimTransitionConditionType::BoolEquals:
            NewStateMachine->AddBoolEqualsTransition(
                Transition.FromState,
                Transition.ToState,
                Transition.ParameterName,
                Transition.ExpectedBool,
                Transition.BlendSpeed,
                Transition.Priority);
            break;
        case EAnimTransitionConditionType::FloatGreater:
            NewStateMachine->AddFloatGreaterTransition(
                Transition.FromState,
                Transition.ToState,
                Transition.ParameterName,
                Transition.CompareFloat,
                Transition.BlendSpeed,
                Transition.Priority);
            break;
        case EAnimTransitionConditionType::FloatLessEqual:
            NewStateMachine->AddFloatLessEqualTransition(
                Transition.FromState,
                Transition.ToState,
                Transition.ParameterName,
                Transition.CompareFloat,
                Transition.BlendSpeed,
                Transition.Priority);
            break;
        case EAnimTransitionConditionType::IntEquals:
            NewStateMachine->AddIntEqualsTransition(
                Transition.FromState,
                Transition.ToState,
                Transition.ParameterName,
                Transition.ExpectedInt,
                Transition.BlendSpeed,
                Transition.Priority);
            break;
        case EAnimTransitionConditionType::Trigger:
            NewStateMachine->AddTriggerTransition(
                Transition.FromState,
                Transition.ToState,
                Transition.ParameterName,
                Transition.BlendSpeed,
                Transition.Priority);
            break;
        case EAnimTransitionConditionType::StateFinished:
            NewStateMachine->AddStateFinishedTransition(
                Transition.FromState,
                Transition.ToState,
                Transition.BlendSpeed,
                Transition.Priority);
            break;
        case EAnimTransitionConditionType::Native:
        default:
            break;
        }
    }

    if (UAnimSequence* EntrySequence = NewStateMachine->GetCurrentSequence())
    {
        SetSequence(EntrySequence);
        SetLooping(NewStateMachine->GetCurrentStateLooping());
        PlayRate = NewStateMachine->GetCurrentStatePlayRate();
    }

    return true;
}

void UAnimInstance::UpdateAnimation(float DeltaTime)
{
    RecentNotifyEvents.clear();
    SCOPE_STAT_ANIM("Animation Update");
    STAT_COUNTER_ANIM("Active Anim Instances", 1);

    if (!bPlaying || !HasValidSequence())
    {
	    // 서브 클래스에서 변수 업데이트
	    NativeUpdateAnimation(DeltaTime);
    }

	// State Machine Tick 및 Transition 처리
    if (StateMachine)
    {
        StateMachine->Tick(DeltaTime);

        FAnimStateTransitionResult TransitionResult;
        if (StateMachine->ConsumeTransition(TransitionResult))
        {
            if (!CurrentSequence || !bPlaying)
            {
                SetSequence(TransitionResult.TargetSequence);
                SetLooping(TransitionResult.bLoop);
                PlayRate = TransitionResult.PlayRate;
                bNextLoop = bLoop;
                NextPlayRate = PlayRate;
            }
            else
            {
                SetNextSequence(TransitionResult.TargetSequence, TransitionResult.BlendSpeed);
                if (NextSequence == TransitionResult.TargetSequence)
                {
                    bNextLoop = TransitionResult.bLoop;
                    NextPlayRate = TransitionResult.PlayRate;
                }
            }
        }
    }

    if (!bPlaying || CurrentSequence == nullptr || !CurrentSequence->DataModel)
    {
        return;
    }

	// Current Sequence Time
    const UAnimSequence* NotifySequence = CurrentSequence;
    const float PreviousTime = CurrentTime;
    const float Length = GetSequenceLength(CurrentSequence);

	CurrentTime += DeltaTime * PlayRate;

    if (bLoop && Length > 0.0f)
    {
        CurrentTime = std::fmod(CurrentTime, Length);

		// PlayRate < 0
        if (CurrentTime < 0.0f)
            CurrentTime += Length;
    }
    else
    {
        CurrentTime = std::clamp(CurrentTime, 0.0f, Length);

		if ((PlayRate >= 0.0f && CurrentTime >= Length) || (PlayRate < 0.0f && CurrentTime <= 0.0f))
            bPlaying = false;
    }

    UpdateRecentNotifyEvents(NotifySequence, PreviousTime, CurrentTime, DeltaTime);

	// Next Sequence Time (Blending)
    if (GetValidAnimDataModel(NextSequence))
    {
        const float NextLength = GetSequenceLength(NextSequence);

        NextTime += DeltaTime * NextPlayRate;

        if (bNextLoop && NextLength > 0.0f)
        {
            NextTime = std::fmod(NextTime, NextLength);

			// PlayRate < 0
            if (NextTime < 0.0f)
                NextTime += NextLength;
        }
        else
        {
            NextTime = std::clamp(NextTime, 0.0f, NextLength);
        }
		
        BlendFactor += DeltaTime * BlendSpeed;
        BlendFactor = std::clamp(BlendFactor, 0.0f, 1.0f);

		// Blend 완료 
        if (BlendFactor >= 1.0f)
        {
            CurrentSequence = NextSequence;
            CurrentTime = NextTime;
            bLoop = bNextLoop;
            PlayRate = NextPlayRate;
            NextSequence = nullptr;
            NextTime = 0.0f;
            bNextLoop = bLoop;
            NextPlayRate = PlayRate;
            BlendFactor = 0.0f;
            bPlaying = true;
            ClearActiveAnimNotifyStates(false);
        }
    }
}

void UAnimInstance::EvaluatePose(FSkeletonPose& OutPose)
{
    SCOPE_STAT_ANIM("Animation Evaluate");

    if (!CurrentSequence)
    {
        InitializeReferencePose(OutPose);
        return;
    }

    if (NextSequence && BlendFactor > 0.0f)
    {
        // 두 포즈를 각각 계산한 후 블렌딩
        FSkeletonPose PoseA, PoseB;

        EvaluatePoseAtTime(CurrentSequence, CurrentTime, PoseA.LocalTransforms);
        EvaluatePoseAtTime(NextSequence, NextTime, PoseB.LocalTransforms);

        BlendPoses(PoseA, PoseB, BlendFactor, OutPose);
    }
    else
    {
        EvaluatePoseAtTime(CurrentSequence, CurrentTime, OutPose.LocalTransforms);
    }
}

void UAnimInstance::InitializeReferencePose(FSkeletonPose& OutPose)
{
    if (!Owner)
        return;

	const USkeletalMesh* Mesh = Owner->GetSkeletalMesh();

	if (!Mesh || !Mesh->HasValidMeshData())
		return;

	USkeleton* Skeleton = Mesh->GetSkeleton();
	if (!Skeleton || !Skeleton->HasValidSkeletonData())
		return;

	const TArray<FBoneInfo>& Bones = Skeleton->GetBones();
    const int32 BoneCount = static_cast<int32>(Bones.size());

	OutPose.LocalTransforms.resize(BoneCount);

	for (int32 i = 0; i < BoneCount; i++)
	{
        OutPose.LocalTransforms[i] = FTransform(Bones[i].LocalBindTransform);
    }
}

void UAnimInstance::EvaluatePoseAtTime(const UAnimSequence* Sequence, float CurrentTime, TArray<FTransform>& OutLocalTransforms)
{
    const UAnimDataModel* Model = GetValidAnimDataModel(Sequence);
    if (!Model)
    {
        return;
    }

    SCOPE_STAT_ANIM("Animation Evaluate Pose At Time");

    if (Owner)
    {
        const USkeletalMesh* Mesh = Owner->GetSkeletalMesh();
        USkeleton* Skeleton = Mesh ? Mesh->GetSkeleton() : nullptr;
        if (Skeleton && Skeleton->HasValidSkeletonData())
        {
            const TArray<FBoneInfo>& Bones = Skeleton->GetBones();
            OutLocalTransforms.resize(Bones.size());
            for (int32 BoneIndex = 0; BoneIndex < static_cast<int32>(Bones.size()); ++BoneIndex)
            {
                OutLocalTransforms[BoneIndex] = FTransform(Bones[BoneIndex].LocalBindTransform);
            }
        }
    }

    const float FrameRate = static_cast<float>(Model->FrameRate.AsDecimal());
    if (FrameRate <= 0.0f)
    {
        return;
    }

    TArray<FTransform> AclTrackTransforms;
    if (FAclCompression::DecompressPoseToTrackTransforms(Sequence, CurrentTime, AclTrackTransforms))
    {
        for (int32 i = 0; i < static_cast<int32>(Model->BoneAnimationTracks.size()); ++i)
        {
            const FBoneAnimationTrack& Track = Model->BoneAnimationTracks[i];
            int32 BoneIndex = Track.BoneTreeIndex;
            auto RemapIt = SequenceBoneTrackRemaps.find(Sequence);
            if (RemapIt != SequenceBoneTrackRemaps.end() && i < static_cast<int32>(RemapIt->second.size()))
            {
                BoneIndex = RemapIt->second[i];
            }

            if (BoneIndex < 0 || BoneIndex >= static_cast<int32>(OutLocalTransforms.size()))
            {
                continue;
            }

            OutLocalTransforms[BoneIndex] = AclTrackTransforms[i];
        }
        return;
    }

	for (int32 i = 0; i < static_cast<int32>(Model->BoneAnimationTracks.size()); ++i)
    {
        const FBoneAnimationTrack& Track = Model->BoneAnimationTracks[i];
        const FRawAnimSequenceTrack& RawTrack = Track.InternalTrackData;
        int32 BoneIndex = Track.BoneTreeIndex;
        auto RemapIt = SequenceBoneTrackRemaps.find(Sequence);
        if (RemapIt != SequenceBoneTrackRemaps.end() && i < static_cast<int32>(RemapIt->second.size()))
        {
            BoneIndex = RemapIt->second[i];
        }

        if (BoneIndex < 0)
        {
            continue;
        }

        if (BoneIndex >= static_cast<int32>(OutLocalTransforms.size()))
        {
            continue;
        }

        FVector Position = InterpolateKeys(RawTrack.PosKeys, CurrentTime, FrameRate);
        FQuat Rotation = InterpolateKeys(RawTrack.RotKeys, CurrentTime, FrameRate);
        FVector Scale = InterpolateKeys(RawTrack.ScaleKeys, CurrentTime, FrameRate);

        Rotation.Normalize(); // 보간 후 정규화 필수

        OutLocalTransforms[BoneIndex] = FTransform(Rotation, Position, Scale);
    }
}

FVector UAnimInstance::InterpolateKeys(const TArray<FVector>& Keys, float Time, float FrameRate)
{
    if (Keys.empty())
        return FVector::ZeroVector;
    if (Keys.size() == 1)
        return Keys[0];

	float KeyIndex = Time * FrameRate;
    int32 Lo = (int32)KeyIndex;
    int32 Hi = Lo + 1;

    // 범위 클램프
    Lo = std::clamp(Lo, 0, (int32)Keys.size() - 1);
    Hi = std::clamp(Hi, 0, (int32)Keys.size() - 1);

    if (Lo == Hi)
        return Keys[Lo];

    float Alpha = KeyIndex - (float)Lo;
    return FVector::Lerp(Keys[Lo], Keys[Hi], Alpha);
}

FQuat UAnimInstance::InterpolateKeys(const TArray<FQuat>& Keys, float Time, float FrameRate)
{
    if (Keys.empty())
        return FQuat::Identity;
    if (Keys.size() == 1)
        return Keys[0];

    float KeyIndex = Time * FrameRate;
    int32 Lo = (int32)KeyIndex;
    int32 Hi = Lo + 1;

    Lo = std::clamp(Lo, 0, (int32)Keys.size() - 1);
    Hi = std::clamp(Hi, 0, (int32)Keys.size() - 1);

    if (Lo == Hi)
        return Keys[Lo];

    float Alpha = KeyIndex - (float)Lo;
    return FQuat::Slerp(Keys[Lo], Keys[Hi], Alpha);
}

void UAnimInstance::BlendPoses(const FSkeletonPose& PoseA, const FSkeletonPose& PoseB, float BlendFactor, FSkeletonPose& OutPose)
{
    SCOPE_STAT_ANIM("Pose Blend");
    const int32 BoneCount = static_cast<int32>(PoseA.LocalTransforms.size());
    OutPose.LocalTransforms.resize(BoneCount);

    for (int32 i = 0; i < BoneCount; ++i)
    {
        const FTransform& A = PoseA.LocalTransforms[i];
        const FTransform& B = PoseB.LocalTransforms[i];

        FVector BlendedPos = FVector::Lerp(A.GetTranslation(), B.GetTranslation(), BlendFactor);
        FQuat BlendedRot = FQuat::Slerp(A.GetRotation(), B.GetRotation(), BlendFactor);
        FVector BlendedScale = FVector::Lerp(A.GetScale3D(), B.GetScale3D(), BlendFactor);

        BlendedRot.Normalize();
        OutPose.LocalTransforms[i] = FTransform(BlendedRot, BlendedPos, BlendedScale);
    }
}

float UAnimInstance::GetSequenceLength(const UAnimSequence* Sequence) const
{
    const UAnimDataModel* Model = GetValidAnimDataModel(Sequence);
    if (!Model)
    {
        return 0.0f;
    }

    const float FrameRate = static_cast<float>(Model->FrameRate.AsDecimal());
    if (FrameRate <= 0.0f || Model->NumberOfKeys <= 1)
    {
        return 0.0f;
    }

    return static_cast<float>(Model->NumberOfKeys - 1) / FrameRate;
}

float UAnimInstance::NormalizeTimeForSequence(const UAnimSequence* Sequence, float InTime) const
{
    const float Length = GetSequenceLength(Sequence);
    if (Length <= 0.0f)
    {
        return 0.0f;
    }

    if (bLoop)
    {
        float WrappedTime = std::fmod(InTime, Length);
        if (WrappedTime < 0.0f)
        {
            WrappedTime += Length;
        }

        return WrappedTime;
    }

    return std::clamp(InTime, 0.0f, Length);
}

void UAnimInstance::UpdateRecentNotifyEvents(const UAnimSequence* Sequence, float PreviousTime, float NewTime, float DeltaTime)
{
    TArray<FAnimNotifyEvent> CrossedEvents;
    CollectNotifyEventsCrossed(Sequence, PreviousTime, NewTime, CrossedEvents);

    for (FAnimNotifyEvent NotifyEvent : CrossedEvents)
    {
        if (NotifyEvent.IsState() && NotifyEvent.Duration > 0.0f)
        {
            BeginAnimNotifyState(Sequence, NotifyEvent);
        }
        else
        {
            NotifyEvent.TriggerPhase = EAnimNotifyTriggerPhase::Notify;
            RecentNotifyEvents.push_back(NotifyEvent);
            DispatchAnimNotify(Sequence, NotifyEvent);
        }
    }

    TickActiveAnimNotifyStates(Sequence, PreviousTime, NewTime, DeltaTime);
}

void UAnimInstance::DispatchAnimNotify(const UAnimSequence* Sequence, const FAnimNotifyEvent& NotifyEvent)
{
    USkeletalMeshComponent* MeshComponent = Cast<USkeletalMeshComponent>(Owner);
    UAnimSequence* MutableSequence = const_cast<UAnimSequence*>(Sequence);

    if (UAnimNotify* NotifyObject = CreateNotifyObject(NotifyEvent))
    {
        NotifyObject->Notify(MeshComponent, MutableSequence, NotifyEvent);
        UObjectManager::Get().DestroyObject(NotifyObject);
    }

    NativeAnimNotify(Sequence, NotifyEvent);
}

void UAnimInstance::BeginAnimNotifyState(const UAnimSequence* Sequence, const FAnimNotifyEvent& NotifyEvent)
{
    if (!Sequence)
    {
        return;
    }

    FAnimNotifyEvent BeginEvent = NotifyEvent;
    BeginEvent.EventType = EAnimNotifyEventType::NotifyState;
    BeginEvent.TriggerPhase = EAnimNotifyTriggerPhase::Begin;
    BeginEvent.Time = GetNotifyStateTriggerTime(
        NotifyEvent,
        GetSequenceLength(Sequence),
        PlayRate >= 0.0f,
        EAnimNotifyTriggerPhase::Begin);

    FActiveAnimNotifyState ActiveState;
    ActiveState.Sequence = Sequence;
    ActiveState.Event = NotifyEvent;
    ActiveState.Event.EventType = EAnimNotifyEventType::NotifyState;
    ActiveState.NotifyObject = CreateNotifyStateObject(BeginEvent);

    USkeletalMeshComponent* MeshComponent = Cast<USkeletalMeshComponent>(Owner);
    UAnimSequence* MutableSequence = const_cast<UAnimSequence*>(Sequence);
    if (ActiveState.NotifyObject)
    {
        ActiveState.NotifyObject->NotifyBegin(MeshComponent, MutableSequence, BeginEvent);
    }
    NativeAnimNotifyBegin(Sequence, BeginEvent);

    RecentNotifyEvents.push_back(BeginEvent);
    ActiveNotifyStates.push_back(ActiveState);
}

void UAnimInstance::TickActiveAnimNotifyStates(const UAnimSequence* Sequence, float PreviousTime, float NewTime, float DeltaTime)
{
    const float Length = GetSequenceLength(Sequence);
    USkeletalMeshComponent* MeshComponent = Cast<USkeletalMeshComponent>(Owner);
    UAnimSequence* MutableSequence = const_cast<UAnimSequence*>(Sequence);

    for (int32 Index = static_cast<int32>(ActiveNotifyStates.size()) - 1; Index >= 0; --Index)
    {
        FActiveAnimNotifyState& ActiveState = ActiveNotifyStates[Index];
        if (ActiveState.Sequence != Sequence)
        {
            EndActiveAnimNotifyState(Index, ActiveState.Sequence, EAnimNotifyTriggerPhase::End);
            continue;
        }

        if (IsNotifyStateEndCrossed(ActiveState.Event, PreviousTime, NewTime, Length))
        {
            EndActiveAnimNotifyState(Index, Sequence, EAnimNotifyTriggerPhase::End);
            continue;
        }

        FAnimNotifyEvent TickEvent = ActiveState.Event;
        TickEvent.TriggerPhase = EAnimNotifyTriggerPhase::Tick;
        if (ActiveState.NotifyObject)
        {
            ActiveState.NotifyObject->NotifyTick(MeshComponent, MutableSequence, TickEvent, DeltaTime);
        }
        NativeAnimNotifyTick(Sequence, TickEvent, DeltaTime);
    }
}

void UAnimInstance::EndActiveAnimNotifyState(int32 ActiveStateIndex, const UAnimSequence* Sequence, EAnimNotifyTriggerPhase TriggerPhase)
{
    if (ActiveStateIndex < 0 || ActiveStateIndex >= static_cast<int32>(ActiveNotifyStates.size()))
    {
        return;
    }

    FActiveAnimNotifyState ActiveState = ActiveNotifyStates[ActiveStateIndex];
    FAnimNotifyEvent EndEvent = ActiveState.Event;
    EndEvent.TriggerPhase = TriggerPhase;
    EndEvent.Time = GetNotifyStateTriggerTime(
        ActiveState.Event,
        GetSequenceLength(Sequence),
        PlayRate >= 0.0f,
        TriggerPhase);

    USkeletalMeshComponent* MeshComponent = Cast<USkeletalMeshComponent>(Owner);
    UAnimSequence* MutableSequence = const_cast<UAnimSequence*>(Sequence);
    if (ActiveState.NotifyObject)
    {
        ActiveState.NotifyObject->NotifyEnd(MeshComponent, MutableSequence, EndEvent);
        UObjectManager::Get().DestroyObject(ActiveState.NotifyObject);
    }
    NativeAnimNotifyEnd(Sequence, EndEvent);
    RecentNotifyEvents.push_back(EndEvent);

    ActiveNotifyStates.erase(ActiveNotifyStates.begin() + ActiveStateIndex);
}

void UAnimInstance::ClearActiveAnimNotifyStates(bool bDispatchEnd)
{
    for (int32 Index = static_cast<int32>(ActiveNotifyStates.size()) - 1; Index >= 0; --Index)
    {
        if (bDispatchEnd)
        {
            EndActiveAnimNotifyState(Index, ActiveNotifyStates[Index].Sequence, EAnimNotifyTriggerPhase::End);
        }
        else if (ActiveNotifyStates[Index].NotifyObject)
        {
            UObjectManager::Get().DestroyObject(ActiveNotifyStates[Index].NotifyObject);
        }
    }

    ActiveNotifyStates.clear();
}

UAnimNotify* UAnimInstance::CreateNotifyObject(const FAnimNotifyEvent& NotifyEvent) const
{
    UObject* Object = FObjectFactory::Get().Create(NotifyEvent.GetResolvedNotifyClassName());
    UAnimNotify* NotifyObject = Cast<UAnimNotify>(Object);
    if (!NotifyObject && Object)
    {
        UObjectManager::Get().DestroyObject(Object);
    }
    return NotifyObject;
}

UAnimNotifyState* UAnimInstance::CreateNotifyStateObject(const FAnimNotifyEvent& NotifyEvent) const
{
    FAnimNotifyEvent StateEvent = NotifyEvent;
    StateEvent.EventType = EAnimNotifyEventType::NotifyState;
    if (StateEvent.NotifyClassName.empty() || StateEvent.NotifyClassName == "UAnimNotify")
    {
        StateEvent.NotifyClassName = GetDefaultAnimNotifyClassName(EAnimNotifyEventType::NotifyState);
    }

    UObject* Object = FObjectFactory::Get().Create(StateEvent.GetResolvedNotifyClassName());
    UAnimNotifyState* NotifyStateObject = Cast<UAnimNotifyState>(Object);
    if (!NotifyStateObject && Object)
    {
        UObjectManager::Get().DestroyObject(Object);
    }
    return NotifyStateObject;
}

bool UAnimInstance::IsNotifyStateEndCrossed(const FAnimNotifyEvent& NotifyEvent, float PreviousTime, float NewTime, float SequenceLength) const
{
    if (!NotifyEvent.IsState() || NotifyEvent.Duration <= 0.0f || SequenceLength <= 0.0f || PreviousTime == NewTime)
    {
        return false;
    }

    const bool bForwardPlayback = PlayRate >= 0.0f;
    const float EndTime = bForwardPlayback
        ? std::clamp(NotifyEvent.Time + NotifyEvent.Duration, 0.0f, SequenceLength)
        : std::clamp(NotifyEvent.Time, 0.0f, SequenceLength);
    const bool bWrapped = bLoop && ((bForwardPlayback && NewTime < PreviousTime) || (!bForwardPlayback && NewTime > PreviousTime));

    if (!bLoop || !bWrapped)
    {
        const bool bIncludeStartBoundary = bForwardPlayback ? PreviousTime <= 0.0f : PreviousTime >= SequenceLength;
        return bForwardPlayback
            ? IsNotifyTriggeredForward(EndTime, PreviousTime, NewTime, bIncludeStartBoundary)
            : IsNotifyTriggeredBackward(EndTime, PreviousTime, NewTime, bIncludeStartBoundary);
    }

    if (bForwardPlayback)
    {
        return IsNotifyTriggeredForward(EndTime, PreviousTime, SequenceLength) ||
            IsNotifyTriggeredForward(EndTime, 0.0f, NewTime, true);
    }

    return IsNotifyTriggeredBackward(EndTime, PreviousTime, 0.0f) ||
        IsNotifyTriggeredBackward(EndTime, SequenceLength, NewTime, true);
}

void UAnimInstance::CollectNotifyEventsCrossed(
    const UAnimSequence* Sequence,
    float PreviousTime,
    float NewTime,
    TArray<FAnimNotifyEvent>& OutEvents) const
{
    const UAnimDataModel* Model = GetValidAnimDataModel(Sequence);
    if (!Model || Model->NotifyTracks.empty())
    {
        return;
    }

    const float Length = GetSequenceLength(Sequence);
    if (Length <= 0.0f || PreviousTime == NewTime)
    {
        return;
    }

    const bool bForwardPlayback = PlayRate >= 0.0f;
    const bool bWrapped = bLoop && ((bForwardPlayback && NewTime < PreviousTime) || (!bForwardPlayback && NewTime > PreviousTime));
    const FString SourceSequenceName = Sequence ? Sequence->GetName() : FString();

    auto AppendTrackEvents =
        [&OutEvents, &SourceSequenceName, bForwardPlayback, Length](const FAnimNotifyTrack& Track, int32 TrackIndex, auto&& Predicate)
        {
            for (const FAnimNotifyEvent& NotifyEvent : Track.Events)
            {
                const float TriggerTime =
                    NotifyEvent.IsState() && NotifyEvent.Duration > 0.0f && !bForwardPlayback
                        ? std::clamp(NotifyEvent.Time + NotifyEvent.Duration, 0.0f, Length)
                        : NotifyEvent.Time;
                if (Predicate(TriggerTime))
                {
                    FAnimNotifyEvent RuntimeEvent = NotifyEvent;
                    RuntimeEvent.SourceTrackName = Track.TrackName;
                    RuntimeEvent.SourceTrackIndex = TrackIndex;
                    RuntimeEvent.SourceSequenceName = SourceSequenceName;
                    OutEvents.push_back(RuntimeEvent);
                }
            }
        };

    auto AppendForwardRange =
        [&Model, &AppendTrackEvents](float StartTime, float EndTime, bool bIncludeStartBoundary = false)
        {
            if (EndTime <= StartTime)
            {
                return;
            }

            for (int32 TrackIndex = 0; TrackIndex < static_cast<int32>(Model->NotifyTracks.size()); ++TrackIndex)
            {
                const FAnimNotifyTrack& Track = Model->NotifyTracks[TrackIndex];
                AppendTrackEvents(
                    Track,
                    TrackIndex,
                    [StartTime, EndTime, bIncludeStartBoundary](float NotifyTime)
                    {
                        return IsNotifyTriggeredForward(NotifyTime, StartTime, EndTime, bIncludeStartBoundary);
                    });
            }
        };

    auto AppendBackwardRange =
        [&Model, &AppendTrackEvents](float StartTime, float EndTime, bool bIncludeStartBoundary = false)
        {
            if (EndTime >= StartTime)
            {
                return;
            }

            for (int32 TrackIndex = 0; TrackIndex < static_cast<int32>(Model->NotifyTracks.size()); ++TrackIndex)
            {
                const FAnimNotifyTrack& Track = Model->NotifyTracks[TrackIndex];
                AppendTrackEvents(
                    Track,
                    TrackIndex,
                    [StartTime, EndTime, bIncludeStartBoundary](float NotifyTime)
                    {
                        return IsNotifyTriggeredBackward(NotifyTime, StartTime, EndTime, bIncludeStartBoundary);
                    });
            }
        };

    if (!bLoop || !bWrapped)
    {
        if (bForwardPlayback)
        {
            AppendForwardRange(PreviousTime, NewTime, PreviousTime <= 0.0f);
        }
        else
        {
            AppendBackwardRange(PreviousTime, NewTime, PreviousTime >= Length);
        }
        return;
    }

    if (bForwardPlayback)
    {
        AppendForwardRange(PreviousTime, Length);
        AppendForwardRange(0.0f, NewTime, true);
    }
    else
    {
        AppendBackwardRange(PreviousTime, 0.0f);
        AppendBackwardRange(Length, NewTime, true);
    }
}
