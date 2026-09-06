#include "Runtime/Script/ScriptManager.h"

#include "Animation/ActorSequence.h"
#include "Animation/AnimData/AnimNotifyTypes.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimationStateMachine.h"
#include "Animation/LuaAnimInstance.h"
#include "Asset/CurveFloatAsset.h"
#include "Runtime/Script/ScriptComponent.h"
#include "Runtime/Script/ScriptUtils.h"

namespace
{
    FName LuaName(const FString& Name)
    {
        return FName(Name);
    }
}

void FScriptManager::BindAnimationTypes()
{
    LUA_BEGIN_TYPE_NO_CTOR_BASE(GLuaState, UCurveFloatAsset, "CurveFloatAsset", UObject)
    LUA_METHOD(Evaluate, Evaluate);
    LUA_METHOD(GetAssetPath, GetAssetPath);
    LUA_END_TYPE();

    LUA_BEGIN_TYPE_NO_CTOR_BASE(GLuaState, UActorSequence, "ActorSequence", UObject)
    LUA_FIELD(StartTime, StartTime);
    LUA_FIELD(Duration, Duration);
    LUA_FIELD(Loop, bLoop);
    LUA_END_TYPE();

    LUA_BEGIN_TYPE_NO_CTOR_BASE(GLuaState, UActorSequencePlayer, "ActorSequencePlayer", UObject)
    LUA_METHOD(Play, Play);
    LUA_METHOD(Pause, Pause);
    LUA_METHOD(Stop, Stop);
    LUA_METHOD(SetCurrentTime, SetCurrentTime);
    LUA_METHOD(GetCurrentTime, GetCurrentTime);
    LUA_METHOD(IsPlaying, IsPlaying);
    LUA_END_TYPE();

    LUA_BEGIN_TYPE_NO_CTOR(GLuaState, FAnimNotifyEvent, "AnimNotifyEvent")
    LUA_FIELD(Time, Time);
    LUA_FIELD(Duration, Duration);
    LUA_FIELD(NotifyClassName, NotifyClassName);
    LUA_FIELD(Payload, Payload);
    LUA_SET(Name, [](const FAnimNotifyEvent& Self)
            { return Self.Name.IsValid() ? Self.Name.ToString() : FString(); });
    LUA_SET(EventType, [](const FAnimNotifyEvent& Self)
            { return AnimNotifyEventTypeToString(Self.EventType); });
    LUA_SET(TriggerPhase, [](const FAnimNotifyEvent& Self)
            { return AnimNotifyTriggerPhaseToString(Self.TriggerPhase); });
    LUA_SET(GetName, [](const FAnimNotifyEvent& Self)
            { return Self.Name.IsValid() ? Self.Name.ToString() : FString(); });
    LUA_SET(GetEventType, [](const FAnimNotifyEvent& Self)
            { return AnimNotifyEventTypeToString(Self.EventType); });
    LUA_SET(GetTriggerPhase, [](const FAnimNotifyEvent& Self)
            { return AnimNotifyTriggerPhaseToString(Self.TriggerPhase); });
    LUA_SET(IsNotify, [](const FAnimNotifyEvent& Self)
            { return Self.TriggerPhase == EAnimNotifyTriggerPhase::Notify; });
    LUA_SET(IsBegin, [](const FAnimNotifyEvent& Self)
            { return Self.TriggerPhase == EAnimNotifyTriggerPhase::Begin; });
    LUA_SET(IsTick, [](const FAnimNotifyEvent& Self)
            { return Self.TriggerPhase == EAnimNotifyTriggerPhase::Tick; });
    LUA_SET(IsEnd, [](const FAnimNotifyEvent& Self)
            { return Self.TriggerPhase == EAnimNotifyTriggerPhase::End; });
    LUA_END_TYPE();

    LUA_BEGIN_TYPE_NO_CTOR(GLuaState, FLuaTimeline, "LuaTimeline")
    LUA_METHOD(Play, Play);
    LUA_METHOD(Pause, Pause);
    LUA_METHOD(Stop, Stop);
    LUA_METHOD(Tick, Tick);
    LUA_METHOD(SetPlayRate, SetPlayRate);
    LUA_METHOD(SetLoop, SetLoop);
    LUA_METHOD(SetCurrentTime, SetCurrentTime);
    LUA_METHOD(GetCurrentTime, GetCurrentTime);
    LUA_METHOD(AddFloatTrack, AddFloatTrack);
    LUA_METHOD(ClearTracks, ClearTracks);
    LUA_END_TYPE();

    LUA_BEGIN_TYPE_NO_CTOR_BASE(GLuaState, UAnimationStateMachine, "AnimationStateMachine", UObject)
    LUA_METHOD(AddStateByPath, AddStateByPath);
    LUA_METHOD(AddStateFromOwnerMesh, AddStateFromOwnerMesh);
    LUA_SET(SetEntryState, [](UAnimationStateMachine& Self, const FString& StateName)
            { return Self.SetEntryState(LuaName(StateName)); });
    LUA_SET(RegisterParameterBool, [](UAnimationStateMachine& Self, const FString& Name, bool DefaultValue)
            { Self.RegisterParameterBool(LuaName(Name), DefaultValue); });
    LUA_SET(RegisterParameterFloat, [](UAnimationStateMachine& Self, const FString& Name, float DefaultValue)
            { Self.RegisterParameterFloat(LuaName(Name), DefaultValue); });
    LUA_SET(RegisterParameterInt, [](UAnimationStateMachine& Self, const FString& Name, int32 DefaultValue)
            { Self.RegisterParameterInt(LuaName(Name), DefaultValue); });
    LUA_SET(RegisterParameterTrigger, [](UAnimationStateMachine& Self, const FString& Name)
            { Self.RegisterParameterTrigger(LuaName(Name)); });
    LUA_SET(SetParameterBool, [](UAnimationStateMachine& Self, const FString& Name, bool Value)
            { Self.SetParameterBool(LuaName(Name), Value); });
    LUA_SET(SetParameterFloat, [](UAnimationStateMachine& Self, const FString& Name, float Value)
            { Self.SetParameterFloat(LuaName(Name), Value); });
    LUA_SET(SetParameterInt, [](UAnimationStateMachine& Self, const FString& Name, int32 Value)
            { Self.SetParameterInt(LuaName(Name), Value); });
    LUA_SET(SetParameterTrigger, [](UAnimationStateMachine& Self, const FString& Name)
            { Self.SetParameterTrigger(LuaName(Name)); });
    LUA_SET(GetParameterBool, [](UAnimationStateMachine& Self, const FString& Name)
            { return Self.GetParameterBool(LuaName(Name)); });
    LUA_SET(GetParameterFloat, [](UAnimationStateMachine& Self, const FString& Name)
            { return Self.GetParameterFloat(LuaName(Name)); });
    LUA_SET(GetParameterInt, [](UAnimationStateMachine& Self, const FString& Name)
            { return Self.GetParameterInt(LuaName(Name)); });
    LUA_SET(AddBoolEqualsTransition, [](UAnimationStateMachine& Self, const FString& From, const FString& To, const FString& Parameter, bool Expected, float BlendSpeed, int32 Priority)
            { return Self.AddBoolEqualsTransition(LuaName(From), LuaName(To), LuaName(Parameter), Expected, BlendSpeed, Priority); });
    LUA_SET(AddFloatGreaterTransition, [](UAnimationStateMachine& Self, const FString& From, const FString& To, const FString& Parameter, float Threshold, float BlendSpeed, int32 Priority)
            { return Self.AddFloatGreaterTransition(LuaName(From), LuaName(To), LuaName(Parameter), Threshold, BlendSpeed, Priority); });
    LUA_SET(AddFloatLessEqualTransition, [](UAnimationStateMachine& Self, const FString& From, const FString& To, const FString& Parameter, float Threshold, float BlendSpeed, int32 Priority)
            { return Self.AddFloatLessEqualTransition(LuaName(From), LuaName(To), LuaName(Parameter), Threshold, BlendSpeed, Priority); });
    LUA_SET(AddIntEqualsTransition, [](UAnimationStateMachine& Self, const FString& From, const FString& To, const FString& Parameter, int32 Expected, float BlendSpeed, int32 Priority)
            { return Self.AddIntEqualsTransition(LuaName(From), LuaName(To), LuaName(Parameter), Expected, BlendSpeed, Priority); });
    LUA_SET(AddTriggerTransition, [](UAnimationStateMachine& Self, const FString& From, const FString& To, const FString& Parameter, float BlendSpeed, int32 Priority)
            { return Self.AddTriggerTransition(LuaName(From), LuaName(To), LuaName(Parameter), BlendSpeed, Priority); });
    LUA_SET(AddStateFinishedTransition, [](UAnimationStateMachine& Self, const FString& From, const FString& To, float BlendSpeed, int32 Priority)
            { return Self.AddStateFinishedTransition(LuaName(From), LuaName(To), BlendSpeed, Priority); });
    LUA_SET(GetCurrentStateName, [](UAnimationStateMachine& Self)
            { return Self.GetCurrentStateName().ToString(); });
    LUA_END_TYPE();

    LUA_BEGIN_TYPE_NO_CTOR_BASE(GLuaState, UAnimInstance, "AnimInstance", UObject)
    LUA_METHOD(CreateStateMachine, CreateStateMachine);
    LUA_METHOD(GetStateMachine, GetStateMachine);
    LUA_METHOD(GetOwningComponent, GetOwningComponent);
    LUA_END_TYPE();

    LUA_BEGIN_TYPE_NO_CTOR_BASE(GLuaState, ULuaAnimInstance, "LuaAnimInstance", UAnimInstance, UObject)
    LUA_METHOD(SetScriptName, SetScriptName);
    LUA_METHOD(GetScriptName, GetScriptName);
    LUA_END_TYPE();
}
