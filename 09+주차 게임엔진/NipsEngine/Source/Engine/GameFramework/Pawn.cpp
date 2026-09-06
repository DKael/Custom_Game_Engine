#include "GameFramework/Pawn.h"

#include "Component/CameraComponent.h"
#include "Component/SceneComponent.h"
#include "Component/StaticMeshComponent.h"

DEFINE_CLASS(APawn, AActor)
REGISTER_FACTORY(APawn)

void APawn::InitDefaultComponents()
{
    ControllerScriptPath = "Asset/Scripts/PlayerController.lua";

    USceneComponent* RootComponent = AddComponent<USceneComponent>();
    SetRootComponent(RootComponent);

    UStaticMeshComponent* CharacterComponent = AddComponent<UStaticMeshComponent>();
    CharacterComponent->AttachToComponent(RootComponent);

    UCameraComponent* CameraComponent = AddComponent<UCameraComponent>();
    CameraComponent->AttachToComponent(RootComponent);
    CameraComponent->SetRelativeLocation(FVector(-6.0f, 0.0f, 4.0f));
    CameraComponent->LookAt(RootComponent->GetWorldLocation());
}

UCameraComponent* APawn::GetCameraComponent() const
{
    for (UActorComponent* Component : GetComponents())
    {
        if (UCameraComponent* CameraComponent = Cast<UCameraComponent>(Component))
        {
            return CameraComponent;
        }
    }

    return nullptr;
}

UStaticMeshComponent* APawn::GetCharacterComponent() const
{
    USceneComponent* Root = GetRootComponent();
    if (!Root)
    {
        return nullptr;
    }

    for (USceneComponent* Child : Root->GetChildren())
    {
        if (UStaticMeshComponent* CharacterComponent = Cast<UStaticMeshComponent>(Child))
        {
            return CharacterComponent;
        }
    }

    return nullptr;
}

FVector APawn::GetForwardVector() const
{
    if (USceneComponent* CharacterComponent = GetCharacterComponent())
    {
        return CharacterComponent->GetForwardVector();
    }

    return FVector(1.0f, 0.0f, 0.0f);
}

FVector APawn::GetRightVector() const
{
    if (USceneComponent* CharacterComponent = GetCharacterComponent())
    {
        return CharacterComponent->GetRightVector();
    }

    return FVector(0.0f, 1.0f, 0.0f);
}

FVector APawn::GetUpVector() const
{
    if (USceneComponent* CharacterComponent = GetCharacterComponent())
    {
        return CharacterComponent->GetUpVector();
    }

    return FVector(0.0f, 0.0f, 1.0f);
}

