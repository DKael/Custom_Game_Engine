#include "AnimationStateMachine.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimData/AnimSequence.h"
#include "Asset/SkeletalMesh.h"
#include "Component/SkinnedMeshComponent.h"
#include "Core/ResourceManager.h"
#include "Object/ObjectFactory.h"

#include <algorithm>

namespace
{
    const FName AnyStateName("Any");
}

bool UAnimationStateMachine::AddState(const FName& StateName, UAnimSequence* Sequence, bool bLoop, float InPlayRate)
{
    if (!StateName.IsValid() || !Sequence)
    {
        return false;
    }
    if (AnimInstance && !AnimInstance->PrepareSequenceForPlayback(Sequence))
    {
        return false;
    }

    FAnimStateMachineState State;
    State.Name = StateName;
    State.Sequence = Sequence;
    State.bLoop = bLoop;
    State.PlayRate = InPlayRate;
    States[StateName] = State;
    return true;
}

bool UAnimationStateMachine::AddStateByPath(const FString& StateName, const FString& AnimSequencePath, bool bLoop, float InPlayRate)
{
    UAnimSequence* Sequence = FResourceManager::Get().LoadAnimSequence(AnimSequencePath);
    return AddState(FName(StateName), Sequence, bLoop, InPlayRate);
}

bool UAnimationStateMachine::AddStateFromOwnerMesh(const FString& StateName, int32 SequenceIndex, bool bLoop, float InPlayRate)
{
    return false;
}

bool UAnimationStateMachine::SetEntryState(const FName& StateName)
{
    if (!FindState(StateName))
    {
        return false;
    }

    CurrentStateName = StateName;
    return true;
}

void UAnimationStateMachine::RegisterParameterBool(const FName& Name, bool DefaultValue)
{
    FAnimStateMachineParameter Parameter;
    Parameter.Type = EAnimStateParameterType::Bool;
    Parameter.BoolValue = DefaultValue;
    Parameters[Name] = Parameter;
}

void UAnimationStateMachine::RegisterParameterFloat(const FName& Name, float DefaultValue)
{
    FAnimStateMachineParameter Parameter;
    Parameter.Type = EAnimStateParameterType::Float;
    Parameter.FloatValue = DefaultValue;
    Parameters[Name] = Parameter;
}

void UAnimationStateMachine::RegisterParameterInt(const FName& Name, int32 DefaultValue)
{
    FAnimStateMachineParameter Parameter;
    Parameter.Type = EAnimStateParameterType::Int;
    Parameter.IntValue = DefaultValue;
    Parameters[Name] = Parameter;
}

void UAnimationStateMachine::RegisterParameterTrigger(const FName& Name)
{
    FAnimStateMachineParameter Parameter;
    Parameter.Type = EAnimStateParameterType::Trigger;
    Parameters[Name] = Parameter;
}

void UAnimationStateMachine::SetParameterBool(const FName& Name, bool Value)
{
    auto It = Parameters.find(Name);
    if (It == Parameters.end() || It->second.Type != EAnimStateParameterType::Bool)
    {
        RegisterParameterBool(Name, Value);
        return;
    }

    It->second.BoolValue = Value;
}

void UAnimationStateMachine::SetParameterFloat(const FName& Name, float Value)
{
    auto It = Parameters.find(Name);
    if (It == Parameters.end() || It->second.Type != EAnimStateParameterType::Float)
    {
        RegisterParameterFloat(Name, Value);
        return;
    }

    It->second.FloatValue = Value;
}

void UAnimationStateMachine::SetParameterInt(const FName& Name, int32 Value)
{
    auto It = Parameters.find(Name);
    if (It == Parameters.end() || It->second.Type != EAnimStateParameterType::Int)
    {
        RegisterParameterInt(Name, Value);
        return;
    }

    It->second.IntValue = Value;
}

void UAnimationStateMachine::SetParameterTrigger(const FName& Name)
{
    auto It = Parameters.find(Name);
    if (It == Parameters.end() || It->second.Type != EAnimStateParameterType::Trigger)
    {
        RegisterParameterTrigger(Name);
        It = Parameters.find(Name);
    }

    if (It != Parameters.end())
    {
        It->second.bTriggerSet = true;
    }
}

bool UAnimationStateMachine::GetParameterBool(const FName& Name, bool DefaultValue) const
{
    auto It = Parameters.find(Name);
    return (It != Parameters.end() && It->second.Type == EAnimStateParameterType::Bool)
        ? It->second.BoolValue
        : DefaultValue;
}

float UAnimationStateMachine::GetParameterFloat(const FName& Name, float DefaultValue) const
{
    auto It = Parameters.find(Name);
    return (It != Parameters.end() && It->second.Type == EAnimStateParameterType::Float)
        ? It->second.FloatValue
        : DefaultValue;
}

int32 UAnimationStateMachine::GetParameterInt(const FName& Name, int32 DefaultValue) const
{
    auto It = Parameters.find(Name);
    return (It != Parameters.end() && It->second.Type == EAnimStateParameterType::Int)
        ? It->second.IntValue
        : DefaultValue;
}

bool UAnimationStateMachine::ConsumeParameterTrigger(const FName& Name)
{
    auto It = Parameters.find(Name);
    if (It == Parameters.end() || It->second.Type != EAnimStateParameterType::Trigger || !It->second.bTriggerSet)
    {
        return false;
    }

    It->second.bTriggerSet = false;
    return true;
}

bool UAnimationStateMachine::AddTransition(
    const FName& FromState,
    const FName& ToState,
    float InBlendSpeed,
    FAnimTransitionPredicate Predicate,
    int32 InPriority)
{
    if (!FindState(ToState) || (!IsAnyStateName(FromState) && !FindState(FromState)) || !Predicate)
    {
        return false;
    }

    FAnimStateMachineTransition Transition;
    Transition.FromState = FromState;
    Transition.ToState = ToState;
    Transition.BlendSpeed = InBlendSpeed;
    Transition.Priority = InPriority;
    Transition.ConditionType = EAnimTransitionConditionType::Native;
    Transition.NativePredicate = Predicate;
    Transitions.push_back(Transition);
    return true;
}

bool UAnimationStateMachine::AddBoolEqualsTransition(
    const FName& FromState,
    const FName& ToState,
    const FName& ParameterName,
    bool ExpectedValue,
    float InBlendSpeed,
    int32 InPriority)
{
    if (!FindState(ToState) || (!IsAnyStateName(FromState) && !FindState(FromState)))
    {
        return false;
    }

    FAnimStateMachineTransition Transition;
    Transition.FromState = FromState;
    Transition.ToState = ToState;
    Transition.ParameterName = ParameterName;
    Transition.ConditionType = EAnimTransitionConditionType::BoolEquals;
    Transition.ExpectedBool = ExpectedValue;
    Transition.BlendSpeed = InBlendSpeed;
    Transition.Priority = InPriority;
    Transitions.push_back(Transition);
    return true;
}

bool UAnimationStateMachine::AddFloatGreaterTransition(
    const FName& FromState,
    const FName& ToState,
    const FName& ParameterName,
    float Threshold,
    float InBlendSpeed,
    int32 InPriority)
{
    if (!FindState(ToState) || (!IsAnyStateName(FromState) && !FindState(FromState)))
    {
        return false;
    }

    FAnimStateMachineTransition Transition;
    Transition.FromState = FromState;
    Transition.ToState = ToState;
    Transition.ParameterName = ParameterName;
    Transition.ConditionType = EAnimTransitionConditionType::FloatGreater;
    Transition.CompareFloat = Threshold;
    Transition.BlendSpeed = InBlendSpeed;
    Transition.Priority = InPriority;
    Transitions.push_back(Transition);
    return true;
}

bool UAnimationStateMachine::AddFloatLessEqualTransition(
    const FName& FromState,
    const FName& ToState,
    const FName& ParameterName,
    float Threshold,
    float InBlendSpeed,
    int32 InPriority)
{
    if (!FindState(ToState) || (!IsAnyStateName(FromState) && !FindState(FromState)))
    {
        return false;
    }

    FAnimStateMachineTransition Transition;
    Transition.FromState = FromState;
    Transition.ToState = ToState;
    Transition.ParameterName = ParameterName;
    Transition.ConditionType = EAnimTransitionConditionType::FloatLessEqual;
    Transition.CompareFloat = Threshold;
    Transition.BlendSpeed = InBlendSpeed;
    Transition.Priority = InPriority;
    Transitions.push_back(Transition);
    return true;
}

bool UAnimationStateMachine::AddIntEqualsTransition(
    const FName& FromState,
    const FName& ToState,
    const FName& ParameterName,
    int32 ExpectedValue,
    float InBlendSpeed,
    int32 InPriority)
{
    if (!FindState(ToState) || (!IsAnyStateName(FromState) && !FindState(FromState)))
    {
        return false;
    }

    FAnimStateMachineTransition Transition;
    Transition.FromState = FromState;
    Transition.ToState = ToState;
    Transition.ParameterName = ParameterName;
    Transition.ConditionType = EAnimTransitionConditionType::IntEquals;
    Transition.ExpectedInt = ExpectedValue;
    Transition.BlendSpeed = InBlendSpeed;
    Transition.Priority = InPriority;
    Transitions.push_back(Transition);
    return true;
}

bool UAnimationStateMachine::AddTriggerTransition(
    const FName& FromState,
    const FName& ToState,
    const FName& ParameterName,
    float InBlendSpeed,
    int32 InPriority)
{
    if (!FindState(ToState) || (!IsAnyStateName(FromState) && !FindState(FromState)))
    {
        return false;
    }

    FAnimStateMachineTransition Transition;
    Transition.FromState = FromState;
    Transition.ToState = ToState;
    Transition.ParameterName = ParameterName;
    Transition.ConditionType = EAnimTransitionConditionType::Trigger;
    Transition.BlendSpeed = InBlendSpeed;
    Transition.Priority = InPriority;
    Transitions.push_back(Transition);
    return true;
}

bool UAnimationStateMachine::AddStateFinishedTransition(
    const FName& FromState,
    const FName& ToState,
    float InBlendSpeed,
    int32 InPriority)
{
    if (!FindState(ToState) || !FindState(FromState))
    {
        return false;
    }

    FAnimStateMachineTransition Transition;
    Transition.FromState = FromState;
    Transition.ToState = ToState;
    Transition.ConditionType = EAnimTransitionConditionType::StateFinished;
    Transition.BlendSpeed = InBlendSpeed;
    Transition.Priority = InPriority;
    Transitions.push_back(Transition);
    return true;
}

void UAnimationStateMachine::Tick(float DeltaTime)
{
    if (!CurrentStateName.IsValid() || bHasPendingTransition)
    {
        return;
    }

    TArray<FAnimStateMachineTransition*> Candidates;
    for (FAnimStateMachineTransition& Transition : Transitions)
    {
        if (IsTransitionSourceAllowed(Transition))
        {
            Candidates.push_back(&Transition);
        }
    }

    std::sort(
        Candidates.begin(),
        Candidates.end(),
        [](const FAnimStateMachineTransition* A, const FAnimStateMachineTransition* B)
        {
            return A->Priority > B->Priority;
        });

    for (FAnimStateMachineTransition* Transition : Candidates)
    {
        if (!Transition || Transition->ToState == CurrentStateName)
        {
            continue;
        }

        if (!EvaluateTransition(*Transition))
        {
            continue;
        }

        const FAnimStateMachineState* TargetState = FindState(Transition->ToState);
        if (!TargetState || !TargetState->Sequence)
        {
            return;
        }

        PendingTransition.FromState = CurrentStateName;
        PendingTransition.ToState = Transition->ToState;
        PendingTransition.TargetSequence = TargetState->Sequence;
        PendingTransition.bLoop = TargetState->bLoop;
        PendingTransition.PlayRate = TargetState->PlayRate;
        PendingTransition.BlendSpeed = Transition->BlendSpeed;

        CurrentStateName = Transition->ToState;
        bHasPendingTransition = true;
        return;
    }
}

bool UAnimationStateMachine::ConsumeTransition(FAnimStateTransitionResult& OutResult)
{
    if (!bHasPendingTransition)
    {
        return false;
    }

    OutResult = PendingTransition;
    bHasPendingTransition = false;
    return true;
}

UAnimSequence* UAnimationStateMachine::GetCurrentSequence() const
{
    const FAnimStateMachineState* State = FindState(CurrentStateName);
    return State ? State->Sequence : nullptr;
}

bool UAnimationStateMachine::GetCurrentStateLooping() const
{
    const FAnimStateMachineState* State = FindState(CurrentStateName);
    return State ? State->bLoop : true;
}

float UAnimationStateMachine::GetCurrentStatePlayRate() const
{
    const FAnimStateMachineState* State = FindState(CurrentStateName);
    return State ? State->PlayRate : 1.0f;
}

const FAnimStateMachineState* UAnimationStateMachine::FindState(const FName& StateName) const
{
    auto It = States.find(StateName);
    return It != States.end() ? &It->second : nullptr;
}

FAnimStateMachineState* UAnimationStateMachine::FindState(const FName& StateName)
{
    auto It = States.find(StateName);
    return It != States.end() ? &It->second : nullptr;
}

bool UAnimationStateMachine::IsTransitionSourceAllowed(const FAnimStateMachineTransition& Transition) const
{
    return Transition.FromState == CurrentStateName || IsAnyStateName(Transition.FromState);
}

bool UAnimationStateMachine::EvaluateTransition(FAnimStateMachineTransition& Transition)
{
    switch (Transition.ConditionType)
    {
    case EAnimTransitionConditionType::Native:
        return Transition.NativePredicate ? Transition.NativePredicate(*this) : false;
    case EAnimTransitionConditionType::BoolEquals:
        return GetParameterBool(Transition.ParameterName) == Transition.ExpectedBool;
    case EAnimTransitionConditionType::FloatGreater:
        return GetParameterFloat(Transition.ParameterName) > Transition.CompareFloat;
    case EAnimTransitionConditionType::FloatLessEqual:
        return GetParameterFloat(Transition.ParameterName) <= Transition.CompareFloat;
    case EAnimTransitionConditionType::IntEquals:
        return GetParameterInt(Transition.ParameterName) == Transition.ExpectedInt;
    case EAnimTransitionConditionType::Trigger:
        return ConsumeParameterTrigger(Transition.ParameterName);
    case EAnimTransitionConditionType::StateFinished:
        return AnimInstance && AnimInstance->IsCurrentAnimationFinished();
    default:
        return false;
    }
}

bool UAnimationStateMachine::IsAnyStateName(const FName& StateName) const
{
    return StateName == AnyStateName;
}
