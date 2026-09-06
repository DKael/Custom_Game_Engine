#pragma once

#include "Core/CoreMinimal.h"
#include "Engine/Geometry/Ray.h"
#include "Object/ObjectFactory.h"
#include "Component/SceneComponent.h"
#include "Math/Matrix.h"
#include "Math/Utils.h"
#include "Math/Vector.h"

#include "Engine/Geometry/Frustum.h"

struct FCameraState
{
    float FOV = 3.14159265358979f / 3.0f;
    float AspectRatio = 16.0f / 9.0f;
    float NearZ = 0.1f;
    float FarZ = 1000.0f;
    float OrthoWidth = 10.0f;
    bool  bIsOrthogonal = false;
};

class UCameraComponent : public USceneComponent
{
  public:
    DECLARE_CLASS(UCameraComponent, USceneComponent)

    UCameraComponent() = default;
    ~UCameraComponent() = default;

    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;

    void                LookAt(const FVector& Target);
    void                SetCameraState(const FCameraState& NewState);
    const FCameraState& GetCameraState() const { return CameraState; }

    void SetFOV(float InFOV)
    {
        CameraState.FOV = InFOV;
        MarkProjectionDirty();
    }
    void SetOrthoWidth(float InWidth)
    {
        CameraState.OrthoWidth = InWidth;
        MarkProjectionDirty();
    }
    void SetOrthographic(bool bOrtho)
    {
        CameraState.bIsOrthogonal = bOrtho;
        MarkProjectionDirty();
    }

    void OnResize(int32 Width, int32 Height);

    FMatrix GetViewMatrix() const;
    FMatrix GetProjectionMatrix() const;

    float GetFOV() const { return CameraState.FOV; }
    float GetNearPlane() const { return CameraState.NearZ; }
    float GetFarPlane() const { return CameraState.FarZ; }
    float GetOrthoWidth() const { return CameraState.OrthoWidth; }
    bool  IsOrthogonal() const { return CameraState.bIsOrthogonal; }

    FRay DeprojectScreenToWorld(float MouseX, float MouseY, float ScreenWidth, float ScreenHeight);

  public:
    //	Unreal-style editor camera helpers
    void AddYawInput(float DeltaYawDegrees);
    void AddPitchInput(float DeltaPitchDegrees);

    void MoveForward(float Distance);
    void MoveRight(float Distance);
    void MoveUp(float Distance);

    float GetPitchDegrees() const;
    float GetYawDegrees() const;

    FVector GetForwardVector() const;
    FVector GetRightVector() const;
    FVector GetUpVector() const;

    void            SetFrustum(const FFrustum& InFrustum) { CachedFrustum = InFrustum; bIsFrustumDirty = false; }
    const FFrustum& GetFrustum() const;

  private:
    void SetViewRotationDegrees(float PitchDegrees, float YawDegrees);
    void MarkViewDirty() const
    {
        bIsViewDirty = true;
        bIsFrustumDirty = true;
    }
    void MarkProjectionDirty() const
    {
        bIsProjectionDirty = true;
        bIsFrustumDirty = true;
    }

  private:
    FCameraState CameraState;

    mutable FMatrix CachedViewMatrix;
    mutable FMatrix CachedProjectionMatrix;
    mutable bool    bIsViewDirty = true;
    mutable bool    bIsProjectionDirty = true;

    mutable FFrustum CachedFrustum;
    mutable bool     bIsFrustumDirty = true;
};
