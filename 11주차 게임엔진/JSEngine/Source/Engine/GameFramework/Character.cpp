#include "Character.h"
#include "Component/SkeletalMeshComponent.h"
#include "Component/CapsuleComponent.h"
#include "Core/ResourceManager.h"

void ACharacter::InitDefaultComponents()
{
    CapsuleComponent = AddComponent<UCapsuleComponent>();
    SetRootComponent(CapsuleComponent);

    MeshComponent = AddComponent<USkeletalMeshComponent>();
    MeshComponent->SetSkeletalMesh(FResourceManager::Get().LoadSkeletalMesh("Asset/SkeletalMesh/SimpleCharacter.fbx"));
    MeshComponent->AttachToComponent(CapsuleComponent);
}
