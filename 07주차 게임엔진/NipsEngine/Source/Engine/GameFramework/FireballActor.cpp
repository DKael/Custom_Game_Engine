#include "FireballActor.h"
#include "Component/StaticMeshComponent.h"
#include "Core/ResourceManager.h"
#include "Component/FireBallComponent.h"
#include "Component/ProjectileComponent.h"

DEFINE_CLASS(AFireballActor, AActor)
REGISTER_FACTORY(AFireballActor)

void AFireballActor::InitDefaultComponents()
{
    auto* Sphere = AddComponent<UStaticMeshComponent>();
    Sphere->SetStaticMesh(FResourceManager::Get().LoadStaticMesh("Asset/Mesh/Sphere.obj"));
    SetRootComponent(Sphere);

    auto* Fireball = AddComponent<UFireBallComponent>();
    Fireball->AttachToComponent(Sphere);

    Fireball->SetIntensity(2.f);
    Fireball->SetRadius(5.f);
    Fireball->SetRadiusFallOff(0.4f);

	auto* ProjectileMovement = AddComponent<UProjectileMovementComponent>();

	ProjectileMovement->SetInitialVelocity({5.f, 0.f, 0.f});


}
