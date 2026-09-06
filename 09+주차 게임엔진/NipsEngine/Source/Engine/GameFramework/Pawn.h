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

private:
    FString ControllerScriptPath = "Asset/Scripts/PlayerController.lua";
};
