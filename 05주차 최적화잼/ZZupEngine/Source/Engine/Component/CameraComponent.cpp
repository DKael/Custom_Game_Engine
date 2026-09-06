#include "Component/CameraComponent.h"
#include <cmath>

DEFINE_CLASS(UCameraComponent, USceneComponent)
REGISTER_FACTORY(UCameraComponent)

FMatrix UCameraComponent::GetViewMatrix() const
{
    UpdateWorldMatrix();

    const FTransform WorldTransform = GetWorldTransform();
    const FTransform ViewSource(WorldTransform.GetRotation(), WorldTransform.GetTranslation(),
                                FVector::OneVector);
    if (bIsViewDirty)
    {
        CachedViewMatrix = ViewSource.ToInverseMatrixWithScale();
        bIsViewDirty = false;
    }
    return CachedViewMatrix;
}

FMatrix UCameraComponent::GetProjectionMatrix() const
{
    if (bIsProjectionDirty)
    {
        if (!CameraState.bIsOrthogonal)
        {
            CachedProjectionMatrix = FMatrix::MakePerspectiveFovLH(
                CameraState.FOV, CameraState.AspectRatio, CameraState.NearZ, CameraState.FarZ);
        }
        else
        {
            const float OrthoHeight = CameraState.OrthoWidth / CameraState.AspectRatio;
            CachedProjectionMatrix =
                FMatrix::MakeOrthographicLH(CameraState.OrthoWidth, OrthoHeight, CameraState.NearZ,
                                            CameraState.FarZ);
        }
        bIsProjectionDirty = false;
    }
    return CachedProjectionMatrix;
}

const FFrustum& UCameraComponent::GetFrustum() const
{
    if (bIsFrustumDirty)
    {
        CachedFrustum.UpdateFromCamera(GetViewMatrix(), GetProjectionMatrix());
        bIsFrustumDirty = false;
    }
    return CachedFrustum;
}

void UCameraComponent::LookAt(const FVector& Target)
{
    const FVector Position = GetWorldLocation();
    const FVector ToTarget = Target - Position;
    const FVector Forward = ToTarget.GetSafeNormal();

    if (Forward.IsNearlyZero())
    {
        return;
    }

    // X-forward, Y-right, Z-up
    // Unreal-like meaning:
    //   Pitch = rotation around Y
    //   Yaw   = rotation around Z
    const float YawDegrees = MathUtil::RadiansToDegrees(std::atan2(Forward.Y, Forward.X));
    const float FlatLength = std::sqrt(Forward.X * Forward.X + Forward.Y * Forward.Y);
    const float PitchDegrees = MathUtil::RadiansToDegrees(std::atan2(Forward.Z, FlatLength));

    FRotator Rot(PitchDegrees, YawDegrees, 0.0f);
    Rot.Normalize();
    SetRelativeRotationQuat(Rot.Quaternion());
    MarkViewDirty();
}

void UCameraComponent::OnResize(int32 Width, int32 Height)
{
    CameraState.AspectRatio = static_cast<float>(Width) / static_cast<float>(Height);
    MarkProjectionDirty();
}

void UCameraComponent::SetCameraState(const FCameraState& NewState)
{
    CameraState = NewState;
    MarkProjectionDirty();
}

FRay UCameraComponent::DeprojectScreenToWorld(float MouseX, float MouseY, float ScreenWidth,
                                              float ScreenHeight)
{
    float NdcX = (2.0f * MouseX) / ScreenWidth - 1.0f;
    float NdcY = 1.0f - (2.0f * MouseY) / ScreenHeight;

    FVector NdcNear(NdcX, NdcY, 1.0f);
    FVector NdcFar(NdcX, NdcY, 0.0f);

    FMatrix ViewProj = GetViewMatrix() * GetProjectionMatrix();
    FMatrix InverseViewProjection = ViewProj.GetInverse();

    FVector WorldNear = InverseViewProjection.TransformPosition(NdcNear);
    FVector WorldFar = InverseViewProjection.TransformPosition(NdcFar);

    const XMVector DirV =
        DirectX::XMVectorSubtract(WorldFar.ToXMVector(), WorldNear.ToXMVector());
    const float Length = DirectX::XMVectorGetX(DirectX::XMVector3Length(DirV));
    const FVector Direction =
        (Length > 1e-4f) ? FVector(DirectX::XMVector3Normalize(DirV)) : FVector(1, 0, 0);

    // SetDirection()을 통해 InvD도 함께 초기화
    return FRay(WorldNear, Direction);
}

void UCameraComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    USceneComponent::GetEditableProperties(OutProps);
    OutProps.push_back({"FOV", EPropertyType::Float, &CameraState.FOV, 0.1f, 3.14f, 0.01f});
    OutProps.push_back({"Near Z", EPropertyType::Float, &CameraState.NearZ, 0.01f, 100.0f, 0.01f});
    OutProps.push_back({"Far Z", EPropertyType::Float, &CameraState.FarZ, 1.0f, 100000.0f, 10.0f});
    OutProps.push_back({"Orthographic", EPropertyType::Bool, &CameraState.bIsOrthogonal});
    if (CameraState.bIsOrthogonal)
    {
        OutProps.push_back(
            {"Ortho Width", EPropertyType::Float, &CameraState.OrthoWidth, 0.1f, 1000.0f, 0.5f});
    }
}

void UCameraComponent::SetViewRotationDegrees(float PitchDegrees, float YawDegrees)
{
    PitchDegrees = MathUtil::Clamp(PitchDegrees, -89.0f, 89.0f);

    FRotator Rot(PitchDegrees, YawDegrees, 0.0f);
    Rot.Normalize();
    SetRelativeRotationQuat(Rot.Quaternion());
    MarkViewDirty();
}

float UCameraComponent::GetPitchDegrees() const { return GetRelativeRotator().Pitch; }

float UCameraComponent::GetYawDegrees() const { return GetRelativeRotator().Yaw; }

void UCameraComponent::AddYawInput(float DeltaYawDegrees)
{
    if (std::abs(DeltaYawDegrees) < 1e-6f)
    {
        return;
    }

    AddRelativeYaw(DeltaYawDegrees);
    MarkViewDirty();
}

void UCameraComponent::AddPitchInput(float DeltaPitchDegrees)
{
    if (std::abs(DeltaPitchDegrees) < 1e-6f)
    {
        return;
    }

    AddRelativePitch(DeltaPitchDegrees);
    MarkViewDirty();
}

FVector UCameraComponent::GetForwardVector() const
{
    return GetRelativeQuat().GetForwardVector();
}

FVector UCameraComponent::GetRightVector() const
{
    return GetRelativeQuat().GetRightVector();
}

FVector UCameraComponent::GetUpVector() const
{
    return GetRelativeQuat().GetUpVector();
}

void UCameraComponent::MoveForward(float Distance)
{
    if (std::abs(Distance) < 1e-6f)
    {
        return;
    }

    const FVector NewLocation = GetWorldLocation() + GetForwardVector() * Distance;
    SetWorldLocation(NewLocation);
    MarkViewDirty();
}

void UCameraComponent::MoveRight(float Distance)
{
    if (std::abs(Distance) < 1e-6f)
    {
        return;
    }

    const FVector NewLocation = GetWorldLocation() + GetRightVector() * Distance;
    SetWorldLocation(NewLocation);
    MarkViewDirty();
}

void UCameraComponent::MoveUp(float Distance)
{
    if (std::abs(Distance) < 1e-6f)
    {
        return;
    }

    const FVector NewLocation = GetWorldLocation() + GetUpVector() * Distance;
    SetWorldLocation(NewLocation);
    MarkViewDirty();
}
