#pragma once

#include "Animation/AnimInstance.h"
#include "ThirdParty/sol/sol.hpp"
#include "Generated/LuaAnimInstance.generated.h"

UCLASS()
class ULuaAnimInstance : public UAnimInstance
{
public:
    GENERATED_BODY()

public:
    void SetScriptName(const FString& InScriptName) { ScriptName = InScriptName; }
    const FString& GetScriptName() const { return ScriptName; }

protected:
    void NativeInitializeAnimation() override;
    void NativeUpdateAnimation(float DeltaTime) override;
    void NativeAnimNotify(const UAnimSequence* Sequence, const FAnimNotifyEvent& NotifyEvent) override;
    void NativeAnimNotifyBegin(const UAnimSequence* Sequence, const FAnimNotifyEvent& NotifyEvent) override;
    void NativeAnimNotifyTick(const UAnimSequence* Sequence, const FAnimNotifyEvent& NotifyEvent, float DeltaTime) override;
    void NativeAnimNotifyEnd(const UAnimSequence* Sequence, const FAnimNotifyEvent& NotifyEvent) override;
    void NativeGameplayAnimNotifyEvent(
        const UAnimSequence* Sequence,
        const FAnimNotifyEvent& NotifyEvent,
        const FString& EventName,
        const FString& Payload) override;
    void NativeCameraShakeAnimNotify(
        const UAnimSequence* Sequence,
        const FAnimNotifyEvent& NotifyEvent,
        const FString& Shake,
        float Scale) override;
    void NativeFootstepSurfaceAnimNotify(
        const UAnimSequence* Sequence,
        const FAnimNotifyEvent& NotifyEvent,
        const FString& EventName,
        const FString& SurfaceName,
        const FString& Payload,
        const FVector& HitLocation) override;
    void NativePlayVFXAnimNotify(
        const UAnimSequence* Sequence,
        const FAnimNotifyEvent& NotifyEvent,
        const FString& Effect,
        const FVector& Location,
        const FName& SocketName,
        bool bAttached,
        float Scale) override;
    void NativeSpawnDecalAnimNotify(
        const UAnimSequence* Sequence,
        const FAnimNotifyEvent& NotifyEvent,
        const FString& Decal,
        const FVector& Location,
        const FVector& Normal,
        const FName& SocketName,
        float Size,
        float Lifetime,
        bool bAlignToHitNormal) override;

private:
    void ClearLuaStateReferences();
    bool LoadScript();
    bool CreateLuaInstance(const sol::table& ScriptClass);
    void CallLuaFunction(const char* FunctionName);
    void CallLuaUpdate(float DeltaTime);
    void CallLuaNotify(const char* FunctionName, const FAnimNotifyEvent& NotifyEvent);
    void CallLuaNotifyTick(const char* FunctionName, const FAnimNotifyEvent& NotifyEvent, float DeltaTime);
    void CallLuaGameplayNotify(const char* FunctionName, const FString& EventName, const FString& Payload, const FAnimNotifyEvent& NotifyEvent);
    void CallLuaCameraShakeNotify(const char* FunctionName, const FString& Shake, float Scale, const FAnimNotifyEvent& NotifyEvent);
    void CallLuaFootstepSurfaceNotify(const char* FunctionName, const FString& EventName, const FString& SurfaceName, const FString& Payload, const FVector& HitLocation, const FAnimNotifyEvent& NotifyEvent);
    void CallLuaPlayVFXNotify(const char* FunctionName, const FString& Effect, const FVector& Location, const FString& SocketName, bool bAttached, float Scale, const FAnimNotifyEvent& NotifyEvent);
    void CallLuaSpawnDecalNotify(const char* FunctionName, const FString& Decal, const FVector& Location, const FVector& Normal, const FString& SocketName, float Size, float Lifetime, bool bAlignToHitNormal, const FAnimNotifyEvent& NotifyEvent);
    void CallNamedLuaNotify(const char* Prefix, const FAnimNotifyEvent& NotifyEvent);
    void CallNamedLuaNotifyTick(const char* Prefix, const FAnimNotifyEvent& NotifyEvent, float DeltaTime);

private:
    FString ScriptName = "LuaAnimation";
    sol::environment ScriptEnv;
    sol::table ScriptClass;
    sol::table ScriptInstance;
};
