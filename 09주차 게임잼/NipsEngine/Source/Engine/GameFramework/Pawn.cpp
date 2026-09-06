#include "GameFramework/Pawn.h"

#include "Component/CameraComponent.h"
#include "Component/SceneComponent.h"
#include "Component/StaticMeshComponent.h"

DEFINE_CLASS(APawn, AActor)
REGISTER_FACTORY(APawn)

namespace
{
    float MoveToward(float Current, float Target, float MaxDelta)
    {
        if (Current < Target)
        {
            return std::min(Current + MaxDelta, Target);
        }

        if (Current > Target)
        {
            return std::max(Current - MaxDelta, Target);
        }

        return Target;
    }

    float EvaluateSteerAuthority(float SpeedRatio, float MinSteerAuthority)
    {
        const float ClampedSpeedRatio = MathUtil::Clamp(SpeedRatio, 0.0f, 1.0f);
        const float CurvedSpeedRatio =
            ClampedSpeedRatio * ClampedSpeedRatio * (3.0f - 2.0f * ClampedSpeedRatio);

        return ClampedSpeedRatio * MinSteerAuthority + CurvedSpeedRatio * (1.0f - MinSteerAuthority);
    }
}

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

void APawn::ResetBoatMovement()
{
    BoatForwardSpeed = 0.0f;
    BoatYawSpeed = 0.0f;
}

void APawn::UpdateBoatMovement(
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
    float SpeedEpsilon)
{
    UStaticMeshComponent* CharacterComponent = GetCharacterComponent();
    USceneComponent* RootComponent = GetRootComponent();
    if (CharacterComponent == nullptr || RootComponent == nullptr)
    {
        return;
    }

    const float SafeDeltaTime = MathUtil::Clamp(DeltaTime, 0.0001f, 0.05f);
    const float SafeMass = std::max(1.0f, Mass);
    const float AccelScale = 1.0f / SafeMass;
    const float ForwardAccelPerTick = ForwardAccel * AccelScale;
    const float ReverseAccelPerTick = ReverseAccel * AccelScale;
    const float BrakeAccelPerTick = BrakeAccel * AccelScale;
    const float LinearDragPerTick = LinearDrag * AccelScale;
    const float TurnAccelPerTick = TurnAccel * AccelScale;
    const float TurnDragPerTick = TurnDrag * AccelScale;
    const float MaxForwardSpeedForMass = MaxForwardSpeed;
    const float MaxReverseSpeedForMass = MaxReverseSpeed;
    const float MaxYawSpeedForMass = MaxYawSpeed;

    if (ThrottleInput > 0.0f)
    {
        BoatForwardSpeed += ForwardAccelPerTick * SafeDeltaTime;
    }
    else if (ThrottleInput < 0.0f)
    {
        if (BoatForwardSpeed > 0.0f)
        {
            BoatForwardSpeed -= BrakeAccelPerTick * SafeDeltaTime;
        }
        else
        {
            BoatForwardSpeed -= ReverseAccelPerTick * SafeDeltaTime;
        }
    }
    else
    {
        BoatForwardSpeed = MoveToward(BoatForwardSpeed, 0.0f, LinearDragPerTick * SafeDeltaTime);
    }

    BoatForwardSpeed = MathUtil::Clamp(BoatForwardSpeed, -MaxReverseSpeedForMass, MaxForwardSpeedForMass);
    if (MathUtil::Abs(BoatForwardSpeed) < SpeedEpsilon)
    {
        BoatForwardSpeed = 0.0f;
    }

    const float SpeedRatio = MathUtil::Clamp(MathUtil::Abs(BoatForwardSpeed) / std::max(MaxForwardSpeedForMass, 0.001f), 0.0f, 1.0f);
    const float SteerAuthority = EvaluateSteerAuthority(SpeedRatio, MinSteerAuthority);
    if (SteerInput != 0.0f)
    {
        BoatYawSpeed += SteerInput * TurnAccelPerTick * SteerAuthority * SafeDeltaTime;
    }
    else
    {
        BoatYawSpeed = MoveToward(BoatYawSpeed, 0.0f, TurnDragPerTick * SafeDeltaTime);
    }

    BoatYawSpeed = MathUtil::Clamp(BoatYawSpeed, -MaxYawSpeedForMass, MaxYawSpeedForMass);
    if (MathUtil::Abs(BoatYawSpeed) < SpeedEpsilon)
    {
        BoatYawSpeed = 0.0f;
    }

    if (BoatYawSpeed != 0.0f)
    {
        CharacterComponent->Rotate(BoatYawSpeed * SafeDeltaTime, 0.0f);
    }

    const FVector Forward = GetForwardVector();
    const FVector MoveDelta = Forward * (BoatForwardSpeed * SafeDeltaTime);
    if (MathUtil::Abs(MoveDelta.X) > SpeedEpsilon || MathUtil::Abs(MoveDelta.Y) > SpeedEpsilon)
    {
        AddActorWorldOffset(FVector(MoveDelta.X, MoveDelta.Y, 0.0f));
    }
}

