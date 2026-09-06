#pragma once

#include "GameFramework/Actor.h"

class UCameraComponent;
class USceneComponent;
class UStaticMeshComponent;

class APawn : public AActor
{
public:
    DECLARE_CLASS(APawn, AActor)
    APawn() = default;

    void InitDefaultComponents() override;
    UStaticMeshComponent* GetCharacterComponent() const;
    UCameraComponent* GetCameraComponent() const;
    void SetControllerScriptPath(const FString& InPath) { ControllerScriptPath = InPath; }
    const FString& GetControllerScriptPath() const { return ControllerScriptPath; }

    FVector GetForwardVector() const;
    FVector GetRightVector() const;
    FVector GetUpVector() const;
    void ResetBoatMovement();
    void UpdateBoatMovement(
        float DeltaTime,
        float ThrottleInput,
        float SteerInput,
        float Mass,
        float ForwardAccel,
        float ReverseAccel,
        float BrakeAccel,
        float LinearDrag,
        float TurnAccel,
        float TurnDrag,
        float MaxForwardSpeed,
        float MaxReverseSpeed,
        float MaxYawSpeed,
        float MinSteerAuthority,
        float SpeedEpsilon);
    float GetBoatForwardSpeed() const { return BoatForwardSpeed; }
    float GetBoatYawSpeed() const { return BoatYawSpeed; }

private:
    FString ControllerScriptPath = "Asset/Scripts/PlayerController.lua";
    float BoatForwardSpeed = 0.0f;
    float BoatYawSpeed = 0.0f;
};
