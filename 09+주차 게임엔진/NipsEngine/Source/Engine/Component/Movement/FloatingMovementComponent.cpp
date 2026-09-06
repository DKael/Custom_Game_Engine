#include "FloatingMovementComponent.h"

#include <cmath>

#include "Component/PrimitiveComponent.h"
#include "GameFramework/Actor.h"
#include "Object/ObjectFactory.h"

DEFINE_CLASS(UFloatingMovementComponent, UMovementComponent)
REGISTER_FACTORY(UFloatingMovementComponent)

void UFloatingMovementComponent::Serialize(FArchive& Ar)
{
    UMovementComponent::Serialize(Ar);
    Ar << "BobAmplitude" << BobAmplitude;
    Ar << "BobFrequency" << BobFrequency;
    Ar << "BobPhase" << BobPhase;
    Ar << "DriftDirection" << DriftDirection;
    Ar << "DriftSpeed" << DriftSpeed;
    Ar << "TiltAmplitude" << TiltAmplitude;
    Ar << "TiltFrequency" << TiltFrequency;
    Ar << "UseDeterministicPhase" << bUseDeterministicPhase;

    if (Ar.IsLoading())
    {
        bHasCachedBaseTransform = false;
        ElapsedTime = 0.0f;
        LastAppliedTiltOffset = FVector::ZeroVector;
    }
}

void UFloatingMovementComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    UMovementComponent::GetEditableProperties(OutProps);
    OutProps.push_back({ "Bob Amplitude", EPropertyType::Float, &BobAmplitude, 0.0f, 5.0f, 0.01f });
    OutProps.push_back({ "Bob Frequency", EPropertyType::Float, &BobFrequency, 0.0f, 5.0f, 0.01f });
    OutProps.push_back({ "Bob Phase", EPropertyType::Float, &BobPhase, -360.0f, 360.0f, 1.0f });
    OutProps.push_back({ "Drift Direction", EPropertyType::Vec3, &DriftDirection, 0.0f, 0.0f, 0.01f });
    OutProps.push_back({ "Drift Speed", EPropertyType::Float, &DriftSpeed, 0.0f, 5.0f, 0.01f });
    OutProps.push_back({ "Tilt Amplitude", EPropertyType::Vec3, &TiltAmplitude, 0.0f, 0.0f, 0.1f });
    OutProps.push_back({ "Tilt Frequency", EPropertyType::Float, &TiltFrequency, 0.0f, 5.0f, 0.01f });
    OutProps.push_back({ "Deterministic Phase", EPropertyType::Bool, &bUseDeterministicPhase });
}

void UFloatingMovementComponent::TickComponent(float DeltaTime)
{
    if (!UpdatedComponent || DeltaTime <= 0.0f)
    {
        return;
    }

    UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(UpdatedComponent);
    if (bUpdateOnlyIfRendered && PrimitiveComponent && !PrimitiveComponent->IsVisible())
    {
        return;
    }

    CacheBaseTransformIfNeeded();
    ElapsedTime += DeltaTime;

    constexpr float TwoPi = 6.28318530718f;
    const float PhaseRadians = BobPhase * (3.14159265359f / 180.0f);
    const float BobWave = std::sin(ElapsedTime * BobFrequency * TwoPi + PhaseRadians);
    const float TiltWave = std::sin(ElapsedTime * TiltFrequency * TwoPi + PhaseRadians);

    // Drift를 증분으로 적용해 외부 이동 시스템(BoatInputSystem 등)과 공존.
    // XY는 현재 위치 기준으로 유지하고, Z만 BaseLocation.Z 기준으로 Floating 효과 적용.
    const FVector DriftDelta = DriftDirection.GetSafeNormal2D() * (DriftSpeed * DeltaTime);
    FVector NewLocation = UpdatedComponent->GetWorldLocation();
    UpdatedComponent->SetWorldLocation(NewLocation);

    const FVector CurrentRotation = UpdatedComponent->GetRelativeRotation();
    const FVector ExternalRotation = CurrentRotation - LastAppliedTiltOffset;

    FVector NewTiltOffset = FVector::ZeroVector;
    NewTiltOffset.X = TiltWave * TiltAmplitude.X;
    NewTiltOffset.Y = std::cos(ElapsedTime * TiltFrequency * TwoPi + PhaseRadians) * TiltAmplitude.Y;
    NewTiltOffset.Z = TiltWave * TiltAmplitude.Z;

    UpdatedComponent->SetRelativeRotation(ExternalRotation + NewTiltOffset);
    LastAppliedTiltOffset = NewTiltOffset;
}

void UFloatingMovementComponent::CacheBaseTransformIfNeeded()
{
    if (bHasCachedBaseTransform || !UpdatedComponent)
    {
        return;
    }

    BaseLocation = UpdatedComponent->GetWorldLocation();
    BaseRotation = UpdatedComponent->GetRelativeRotation();
    LastAppliedTiltOffset = FVector::ZeroVector;

    if (bUseDeterministicPhase)
    {
        if (AActor* Owner = GetOwner())
        {
            const uint32 Seed = Owner->GetUUID();
            BobPhase += static_cast<float>(Seed % 360u);
        }
    }

    bHasCachedBaseTransform = true;
}
