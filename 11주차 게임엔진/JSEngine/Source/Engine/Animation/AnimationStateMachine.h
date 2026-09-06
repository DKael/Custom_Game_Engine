#pragma once

#include "Core/CoreMinimal.h"
#include "Object/Object.h"
#include "Object/FName.h"

#include <functional>
#include "Generated/AnimationStateMachine.generated.h"

class UAnimInstance;
class UAnimSequence;

enum class EAnimStateParameterType : uint8
{
    Bool,
    Float,
    Int,
    Trigger,
};

enum class EAnimTransitionConditionType : uint8
{
    Native,
    BoolEquals,
    FloatGreater,
    FloatLessEqual,
    IntEquals,
    Trigger,
    StateFinished,
};

struct FAnimStateMachineParameter
{
    EAnimStateParameterType Type = EAnimStateParameterType::Float;
    bool BoolValue = false;
    float FloatValue = 0.0f;
    int32 IntValue = 0;
    bool bTriggerSet = false;
};

struct FAnimStateMachineState
{
    FName Name;
    UAnimSequence* Sequence = nullptr;
    bool bLoop = true;
    float PlayRate = 1.0f;
};

struct FAnimStateTransitionResult
{
    FName FromState;
    FName ToState;
    UAnimSequence* TargetSequence = nullptr;
    bool bLoop = true;
    float PlayRate = 1.0f;
    float BlendSpeed = 5.0f;
};

class UAnimationStateMachine;
using FAnimTransitionPredicate = std::function<bool(UAnimationStateMachine&)>;

struct FAnimStateMachineTransition
{
    FName FromState;
    FName ToState;
    FName ParameterName;

    EAnimTransitionConditionType ConditionType = EAnimTransitionConditionType::Native;
    FAnimTransitionPredicate NativePredicate;

    bool ExpectedBool = false;
    float CompareFloat = 0.0f;
    int32 ExpectedInt = 0;

    float BlendSpeed = 5.0f;
    int32 Priority = 0;
};

UCLASS()
class UAnimationStateMachine : public UObject
{
public:
    GENERATED_BODY()

public:
    void SetOwningAnimInstance(UAnimInstance* InAnimInstance) { AnimInstance = InAnimInstance; }

    bool AddState(const FName& StateName, UAnimSequence* Sequence, bool bLoop = true, float InPlayRate = 1.0f);
    bool AddStateByPath(const FString& StateName, const FString& AnimSequencePath, bool bLoop = true, float InPlayRate = 1.0f);
    bool AddStateFromOwnerMesh(const FString& StateName, int32 SequenceIndex, bool bLoop = true, float InPlayRate = 1.0f);
    bool SetEntryState(const FName& StateName);

    void RegisterParameterBool(const FName& Name, bool DefaultValue = false);
    void RegisterParameterFloat(const FName& Name, float DefaultValue = 0.0f);
    void RegisterParameterInt(const FName& Name, int32 DefaultValue = 0);
    void RegisterParameterTrigger(const FName& Name);

    void SetParameterBool(const FName& Name, bool Value);
    void SetParameterFloat(const FName& Name, float Value);
    void SetParameterInt(const FName& Name, int32 Value);
    void SetParameterTrigger(const FName& Name);

    bool GetParameterBool(const FName& Name, bool DefaultValue = false) const;
    float GetParameterFloat(const FName& Name, float DefaultValue = 0.0f) const;
    int32 GetParameterInt(const FName& Name, int32 DefaultValue = 0) const;
    bool ConsumeParameterTrigger(const FName& Name);

    bool AddTransition(
        const FName& FromState,
        const FName& ToState,
        float BlendSpeed,
        FAnimTransitionPredicate Predicate,
        int32 Priority = 0);

    bool AddBoolEqualsTransition(
        const FName& FromState,
        const FName& ToState,
        const FName& ParameterName,
        bool ExpectedValue,
        float BlendSpeed,
        int32 Priority = 0);

    bool AddFloatGreaterTransition(
        const FName& FromState,
        const FName& ToState,
        const FName& ParameterName,
        float Threshold,
        float BlendSpeed,
        int32 Priority = 0);

    bool AddFloatLessEqualTransition(
        const FName& FromState,
        const FName& ToState,
        const FName& ParameterName,
        float Threshold,
        float BlendSpeed,
        int32 Priority = 0);

    bool AddIntEqualsTransition(
        const FName& FromState,
        const FName& ToState,
        const FName& ParameterName,
        int32 ExpectedValue,
        float BlendSpeed,
        int32 Priority = 0);

    bool AddTriggerTransition(
        const FName& FromState,
        const FName& ToState,
        const FName& ParameterName,
        float BlendSpeed,
        int32 Priority = 0);

    bool AddStateFinishedTransition(
        const FName& FromState,
        const FName& ToState,
        float BlendSpeed,
        int32 Priority = 0);

    void Tick(float DeltaTime);
    bool ConsumeTransition(FAnimStateTransitionResult& OutResult);

    const FName& GetCurrentStateName() const { return CurrentStateName; }
    UAnimSequence* GetCurrentSequence() const;
    bool GetCurrentStateLooping() const;
    float GetCurrentStatePlayRate() const;

private:
    const FAnimStateMachineState* FindState(const FName& StateName) const;
    FAnimStateMachineState* FindState(const FName& StateName);
    bool IsTransitionSourceAllowed(const FAnimStateMachineTransition& Transition) const;
    bool EvaluateTransition(FAnimStateMachineTransition& Transition);
    bool IsAnyStateName(const FName& StateName) const;

private:
    UAnimInstance* AnimInstance = nullptr;
    TMap<FName, FAnimStateMachineState, FName::Hash> States;
    TArray<FAnimStateMachineTransition> Transitions;
    TMap<FName, FAnimStateMachineParameter, FName::Hash> Parameters;

    FName CurrentStateName = FName::None;
    bool bHasPendingTransition = false;
    FAnimStateTransitionResult PendingTransition;
};
