#include "ProjectileComponent.h"
#include "Object/ObjectFactory.h"
#include "SceneComponent.h"
#include "GameFramework/AActor.h"

DEFINE_CLASS(UProjectileMovementComponent, UMovementComponent)
REGISTER_FACTORY(UProjectileMovementComponent)

void UProjectileMovementComponent::TickComponent(float DeltaTime)
{
    if (!MoveComponent)
        return;

    MoveComponent->SetRelativeLocation(MoveComponent->GetRelativeLocation() + CalculateVelocity(DeltaTime));
}

FVector UProjectileMovementComponent::CalculateVelocity(float DeltaTime)
{
    if (!MoveComponent)
        return FVector::Zero();

    if (bIsGravityEnabled)
        Acceleration.Z = Acceleration.Z + GravitationalAcceleration * DeltaTime;

    Velocity += Acceleration * DeltaTime;

    return Velocity * DeltaTime;
}

void UProjectileMovementComponent::BeginPlay()
{
    Velocity = InitialVelocity;
    Acceleration = InitialAcceleration;
}

void UProjectileMovementComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    UMovementComponent::GetEditableProperties(OutProps);

    OutProps.push_back({"InitialVelocity", EPropertyType::Vec3, &InitialVelocity});
    OutProps.push_back({"InitialAcceleration", EPropertyType::Vec3, &InitialAcceleration});

    OutProps.push_back({"GravityEnabled", EPropertyType::Bool, &bIsGravityEnabled});
    OutProps.push_back({"GravitationalAcceleration", EPropertyType::Float, &GravitationalAcceleration});
}

UProjectileMovementComponent* UProjectileMovementComponent::Duplicate()
{
    UProjectileMovementComponent* NewComp = UObjectManager::Get().CreateObject<UProjectileMovementComponent>();

    NewComp->SetActive(this->IsActive());
    NewComp->SetOwner(nullptr);

    NewComp->Velocity = Velocity;
    NewComp->InitialVelocity = InitialVelocity;

    NewComp->Acceleration = Acceleration;
    NewComp->InitialAcceleration = InitialAcceleration;

    NewComp->GravitationalAcceleration = GravitationalAcceleration;
    NewComp->bIsGravityEnabled = bIsGravityEnabled;

    return NewComp;
}
