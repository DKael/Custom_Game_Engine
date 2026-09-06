#include "RotationMovementComponent.h"

#include "Math/Utils.h"
#include "Object/ObjectFactory.h"
#include "SceneComponent.h"

#include <cstring>

DEFINE_CLASS(URotationMovementComponent, UMovementComponent)
REGISTER_FACTORY(URotationMovementComponent)

void URotationMovementComponent::TickComponent(float DeltaTime)
{
    if (!MoveComponent)
    {
        return;
    }

    const FQuat OldRotation = MoveComponent->GetRelativeQuat();
    FQuat DeltaRotation = FQuat::Identity;

    if (bUseAxisAngleRotation)
    {
        DeltaRotation = FQuat(
            GetSafeRotationAxis(),
            MathUtil::DegreesToRadians(AngularSpeedDegreesPerSecond * DeltaTime));
    }
    else
    {
        DeltaRotation = (RotationRate * DeltaTime).Quaternion();
    }

    const FQuat NewRotation = bRotationInLocalSpace ? (OldRotation * DeltaRotation) : (DeltaRotation * OldRotation);

    FVector DeltaLocation = FVector::ZeroVector;
    if (!PivotTranslation.IsZero())
    {
        const FVector OldPivot = OldRotation.RotateVector(PivotTranslation);
        const FVector NewPivot = NewRotation.RotateVector(PivotTranslation);
        DeltaLocation = OldPivot - NewPivot;
    }

    MoveComponent->SetRelativeLocation(MoveComponent->GetRelativeLocation() + DeltaLocation);
    MoveComponent->SetRelativeRotation(NewRotation.Euler());
}

void URotationMovementComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    UMovementComponent::GetEditableProperties(OutProps);
    OutProps.push_back({"RotationRate", EPropertyType::Vec3, &RotationRate});
    OutProps.push_back({"PivotTranslation", EPropertyType::Vec3, &PivotTranslation});
    OutProps.push_back({"bRotationInLocalSpace", EPropertyType::Bool, &bRotationInLocalSpace});
    OutProps.push_back({"bUseAxisAngleRotation", EPropertyType::Bool, &bUseAxisAngleRotation});
    OutProps.push_back({"RotationAxis", EPropertyType::Vec3, &RotationAxis});
    OutProps.push_back({"AngularSpeedDegreesPerSecond", EPropertyType::Float, &AngularSpeedDegreesPerSecond});
}

void URotationMovementComponent::PostEditProperty(const char* PropertyName)
{
    UMovementComponent::PostEditProperty(PropertyName);

    if (PropertyName != nullptr && std::strcmp(PropertyName, "RotationAxis") == 0)
    {
        RotationAxis = GetSafeRotationAxis();
    }
}

URotationMovementComponent* URotationMovementComponent::Duplicate()
{
    URotationMovementComponent* NewComp = UObjectManager::Get().CreateObject<URotationMovementComponent>();

    NewComp->SetActive(this->IsActive());
    NewComp->SetOwner(nullptr);

    NewComp->RotationRate = RotationRate;
    NewComp->PivotTranslation = PivotTranslation;
    NewComp->bRotationInLocalSpace = bRotationInLocalSpace;
    NewComp->bUseAxisAngleRotation = bUseAxisAngleRotation;
    NewComp->RotationAxis = RotationAxis;
    NewComp->AngularSpeedDegreesPerSecond = AngularSpeedDegreesPerSecond;

    return NewComp;
}

void URotationMovementComponent::SetRotationRate(const FRotator& InRotationRate)
{
    RotationRate = InRotationRate;
    bUseAxisAngleRotation = false;
}

void URotationMovementComponent::SetRotationRateEuler(const FVector& InEulerDegreesPerSecond)
{
    SetRotationRate(FRotator::MakeFromEuler(InEulerDegreesPerSecond));
}

void URotationMovementComponent::SetAxisAngleRotation(
    const FVector& InRotationAxis,
    float InAngularSpeedDegreesPerSecond)
{
    RotationAxis = InRotationAxis.GetSafeNormal();
    if (RotationAxis.IsNearlyZero())
    {
        RotationAxis = FVector::UpVector;
    }

    AngularSpeedDegreesPerSecond = InAngularSpeedDegreesPerSecond;
    bUseAxisAngleRotation = true;
}

FVector URotationMovementComponent::GetSafeRotationAxis() const
{
    const FVector SafeAxis = RotationAxis.GetSafeNormal();
    return SafeAxis.IsNearlyZero() ? FVector::UpVector : SafeAxis;
}
