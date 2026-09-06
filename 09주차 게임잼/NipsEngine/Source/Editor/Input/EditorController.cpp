#include "Editor/Input/EditorController.h"

#include <cmath>
#include "Editor/Selection/SelectionManager.h"
#include "Editor/Settings/EditorSettings.h"
#include "Editor/Viewport/EditorViewportClient.h"
#include "Engine/Collision/RayCollision/RayCollision.h"
#include "Engine/Component/GizmoComponent.h"
#include "Engine/GameFramework/World.h"
#include "Engine/Input/InputSystem.h"
#include "Engine/Math/Utils.h"
#include "Engine/Viewport/ViewportCamera.h"

namespace
{
    constexpr int32 WatchedKeys[] =
    {
        'W', 'A', 'S', 'D', 'Q', 'E',
        VK_LEFT, VK_RIGHT, VK_UP, VK_DOWN,
        VK_SPACE,
    };
}

void FEditorController::SetSelectionManager(FSelectionManager* InSelectionManager)
{
    SelectionManager = InSelectionManager;
    if (SelectionManager && SelectionManager->GetGizmo())
    {
        Gizmo = SelectionManager->GetGizmo();
    }
}

void FEditorController::SetCamera(FViewportCamera* InCamera)
{
    Camera = InCamera;
    ResetTargetLocation();
    SyncAnglesFromCamera();
}

void FEditorController::SetViewportRect(float InX, float InY, float InWidth, float InHeight)
{
    ViewportX = InX;
    ViewportY = InY;
    ViewportWidth = InWidth;
    ViewportHeight = InHeight;
}

void FEditorController::Tick(float DeltaTime)
{
    if (!Camera || !ViewportClient)
    {
        return;
    }

    if (Settings)
    {
        MoveSpeed = Settings->CameraSpeed;
        MoveSensitivity = Settings->CameraMoveSensitivity;
        RotateSensitivity = Settings->CameraRotateSensitivity;
        ZoomSpeed = Settings->CameraZoomSpeed;
    }

    if (!bTargetLocationInitialized)
    {
        ResetTargetLocation();
    }

    TickMouseInput();
    TickKeyboardInput(DeltaTime);
    TickEditorShortcuts();

    constexpr float LocationLerpSpeed = 12.0f;
    const FVector CurrentLocation = Camera->GetLocation();
    const float LerpAlpha = MathUtil::Clamp(DeltaTime * LocationLerpSpeed, 0.0f, 1.0f);
    Camera->SetLocation(CurrentLocation + (TargetLocation - CurrentLocation) * LerpAlpha);
}

void FEditorController::ResetTargetLocation()
{
    if (!Camera)
    {
        return;
    }

    TargetLocation = Camera->GetLocation();
    bTargetLocationInitialized = true;
}

void FEditorController::TickMouseInput()
{
    InputSystem& Input = InputSystem::Get();

    POINT MousePoint = Input.GetMousePos();
    if (ViewportClient->GetWindow())
    {
        MousePoint = ViewportClient->GetWindow()->ScreenToClientPoint(MousePoint);
    }

    const float LocalX = static_cast<float>(MousePoint.x) - ViewportX;
    const float LocalY = static_cast<float>(MousePoint.y) - ViewportY;
    const float DeltaX = static_cast<float>(Input.MouseDeltaX());
    const float DeltaY = static_cast<float>(Input.MouseDeltaY());

    if (Input.MouseMoved())
    {
        OnMouseMoveAbsolute(LocalX, LocalY);
    }

    if (Input.GetKeyDown(VK_RBUTTON) && Input.GetKey(VK_CONTROL))
    {
        if (ViewportClient->RequestActorPlacement(LocalX, LocalY, ViewportX + LocalX, ViewportY + LocalY))
        {
            return;
        }
    }

    if (Input.GetKeyDown(VK_RBUTTON))
    {
        OnRightMouseClick();
    }

    if (Input.GetRightDragging())
    {
        OnRightMouseDrag(DeltaX, DeltaY);
    }

    if (Input.GetMiddleDragging())
    {
        OnMiddleMouseDrag(DeltaX, DeltaY, 1.0f);
    }

    if (!MathUtil::IsNearlyZero(Input.GetScrollNotches()))
    {
        OnWheelScrolled(Input.GetScrollNotches(), 1.0f);
    }

    if (Input.GetKeyDown(VK_LBUTTON))
    {
        OnLeftMouseClick(LocalX, LocalY);
    }

    if (Input.GetLeftDragging())
    {
        OnLeftMouseDrag(LocalX, LocalY);
    }

    if (Input.GetLeftDragEnd())
    {
        OnLeftMouseDragEnd(LocalX, LocalY);
    }

    if (Input.GetKeyUp(VK_LBUTTON) && !Input.GetLeftDragEnd())
    {
        OnLeftMouseButtonUp(LocalX, LocalY);
    }
}

void FEditorController::TickKeyboardInput(float DeltaTime)
{
    const InputSystem& Input = InputSystem::Get();
    for (int32 KeyCode : WatchedKeys)
    {
        if (Input.GetKeyDown(KeyCode))
        {
            OnKeyPressed(KeyCode);
        }

        if (Input.GetKey(KeyCode))
        {
            OnKeyDown(KeyCode, DeltaTime);
        }
    }
}

void FEditorController::TickEditorShortcuts()
{
    const InputSystem& Input = InputSystem::Get();
    const bool bCtrlDown = Input.GetKey(VK_CONTROL);
    const bool bAltDown = Input.GetKey(VK_MENU);

    if (Input.GetKeyUp('X') && Gizmo)
    {
        Gizmo->SetWorldSpace(!Gizmo->IsWorldSpace());
    }

    if (Input.GetKeyUp(VK_DELETE))
    {
        ViewportClient->DeleteSelectedActors();
    }

    if (Input.GetKeyDown('D') && bCtrlDown && !bAltDown)
    {
        ViewportClient->DuplicateSelectedActors();
    }

    if (Input.GetKeyDown('A') && bCtrlDown && !bAltDown)
    {
        ViewportClient->SelectAllActors();
    }
}

void FEditorController::SyncAnglesFromCamera()
{
    if (!Camera)
    {
        return;
    }

    const FVector Forward = Camera->GetForwardVector().GetSafeNormal();
    Pitch = MathUtil::RadiansToDegrees(std::asin(MathUtil::Clamp(Forward.Z, -1.0f, 1.0f)));
    Yaw = MathUtil::RadiansToDegrees(std::atan2(Forward.Y, Forward.X));
}

void FEditorController::UpdateCameraRotation()
{
    if (!Camera)
    {
        return;
    }

    const float PitchRadians = MathUtil::DegreesToRadians(Pitch);
    const float YawRadians = MathUtil::DegreesToRadians(Yaw);

    FVector Forward(std::cos(PitchRadians) * std::cos(YawRadians),
                    std::cos(PitchRadians) * std::sin(YawRadians),
                    std::sin(PitchRadians));
    Forward = Forward.GetSafeNormal();

    const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward).GetSafeNormal();
    if (Right.IsNearlyZero())
    {
        return;
    }

    const FVector Up = FVector::CrossProduct(Forward, Right).GetSafeNormal();
    FMatrix RotationMatrix = FMatrix::Identity;
    RotationMatrix.SetAxes(Forward, Right, Up);

    FQuat Rotation(RotationMatrix);
    Rotation.Normalize();
    Camera->SetRotation(Rotation);
}

void FEditorController::OnMouseMoveAbsolute(float X, float Y)
{
    if (!Gizmo || !Camera)
    {
        return;
    }

    FRay Ray = Camera->DeprojectScreenToWorld(X, Y, ViewportWidth, ViewportHeight);
    FHitResult HitResult;
    Gizmo->RaycastMesh(Ray, HitResult);
}

void FEditorController::OnLeftMouseClick(float X, float Y)
{
    if (!Camera)
    {
        return;
    }

    FRay Ray = Camera->DeprojectScreenToWorld(X, Y, ViewportWidth, ViewportHeight);
    FHitResult HitResult{};

    if (Gizmo && FRayCollision::RaycastComponent(Gizmo, Ray, HitResult))
    {
        Gizmo->SetPressedOnHandle(true);
        return;
    }

    if (Gizmo)
    {
        Gizmo->SetPressedOnHandle(false);
    }

    if (!World || !SelectionManager)
    {
        return;
    }

    AActor* BestActor = nullptr;
    float ClosestDistance = FLT_MAX;

    FWorldSpatialIndex::FPrimitiveRayQueryScratch QueryScratch;
    TArray<UPrimitiveComponent*> CandidatePrimitives;
    TArray<float> CandidateTs;
    World->GetSpatialIndex().RayQueryPrimitives(Ray, CandidatePrimitives, CandidateTs, QueryScratch);

    const bool bCtrlDown = InputSystem::Get().GetKey(VK_CONTROL);
    for (int32 CandidateIndex = 0; CandidateIndex < static_cast<int32>(CandidatePrimitives.size()); ++CandidateIndex)
    {
        if (CandidateTs[CandidateIndex] > ClosestDistance)
        {
            break;
        }

        UPrimitiveComponent* Primitive = CandidatePrimitives[CandidateIndex];
        AActor* Actor = Primitive ? Primitive->GetOwner() : nullptr;
        if (!Actor || !Actor->GetRootComponent())
        {
            continue;
        }

        HitResult = {};
        if (Primitive->Raycast(Ray, HitResult) && HitResult.Distance < ClosestDistance)
        {
            ClosestDistance = HitResult.Distance;
            BestActor = Actor;
        }
    }

    if (!BestActor)
    {
        if (!bCtrlDown)
        {
            SelectionManager->ClearSelection();
        }
        return;
    }

    if (bCtrlDown)
    {
        SelectionManager->ToggleSelect(BestActor);
    }
    else
    {
        SelectionManager->Select(BestActor);
    }
}

void FEditorController::OnLeftMouseDrag(float X, float Y)
{
    if (!Gizmo || !Camera)
    {
        return;
    }

    FRay Ray = Camera->DeprojectScreenToWorld(X, Y, ViewportWidth, ViewportHeight);
    if (Gizmo->IsPressedOnHandle() && !Gizmo->IsHolding())
    {
        Gizmo->SetHolding(true);
    }

    if (Gizmo->IsHolding())
    {
        Gizmo->UpdateDrag(Ray);
    }
}

void FEditorController::OnLeftMouseDragEnd(float X, float Y)
{
    (void)X;
    (void)Y;

    if (Gizmo)
    {
        Gizmo->DragEnd();
    }
}

void FEditorController::OnLeftMouseButtonUp(float X, float Y)
{
    (void)X;
    (void)Y;

    if (Gizmo)
    {
        Gizmo->SetPressedOnHandle(false);
    }
}

void FEditorController::OnRightMouseClick()
{
    SyncAnglesFromCamera();
}

void FEditorController::OnRightMouseDrag(float DeltaX, float DeltaY)
{
    if (!Camera)
    {
        return;
    }

    const InputSystem& Input = InputSystem::Get();
    if (Input.GetKey(VK_CONTROL) || Input.GetKey(VK_MENU) || Input.GetKey(VK_SHIFT))
    {
        return;
    }

    if (Camera->IsOrthographic())
    {
        const float PanScale = Camera->GetOrthoHeight() * 0.002f;
        const FVector Right = Camera->GetEffectiveRight();
        const FVector Up = Camera->GetEffectiveUp();
        TargetLocation += Right * (-DeltaX * PanScale) + Up * (DeltaY * PanScale);
        return;
    }

    const float RotationSpeed = 0.15f * RotateSensitivity;
    Yaw += DeltaX * RotationSpeed;
    Pitch -= DeltaY * RotationSpeed;
    Pitch = MathUtil::Clamp(Pitch, -89.0f, 89.0f);
    UpdateCameraRotation();
}

void FEditorController::OnMiddleMouseDrag(float DeltaX, float DeltaY, float DeltaTime)
{
    if (!Camera)
    {
        return;
    }

    const InputSystem& Input = InputSystem::Get();
    if (Input.GetKey(VK_CONTROL) || Input.GetKey(VK_MENU) || Input.GetKey(VK_SHIFT))
    {
        return;
    }

    const float PanScale = (Camera->IsOrthographic() ? Camera->GetOrthoHeight() * 0.002f : 20.0f) * MoveSensitivity;
    const FVector Right = Camera->GetEffectiveRight();
    const FVector Up = Camera->GetEffectiveUp();
    TargetLocation += (Right * (-DeltaX * PanScale) + Up * (DeltaY * PanScale)) * DeltaTime;
}

void FEditorController::OnKeyPressed(int32 KeyCode)
{
    switch (KeyCode)
    {
    case VK_SPACE:
        if (Gizmo)
        {
            Gizmo->SetNextMode();
        }
        break;

    case VK_LEFT:
    case VK_RIGHT:
    case VK_UP:
    case VK_DOWN:
        SyncAnglesFromCamera();
        break;
    }
}

void FEditorController::OnKeyDown(int32 KeyCode, float DeltaTime)
{
    if (!Camera)
    {
        return;
    }

    const InputSystem& Input = InputSystem::Get();
    if (Input.GetKey(VK_CONTROL) || Input.GetKey(VK_MENU) || Input.GetKey(VK_SHIFT))
    {
        return;
    }

    if (!Camera->IsOrthographic())
    {
        FVector Move = FVector::ZeroVector;
        const float ActualMoveSpeed = MoveSpeed * MoveSensitivity;

        switch (KeyCode)
        {
        case 'W':
            Move += Camera->GetForwardVector() * ActualMoveSpeed * DeltaTime;
            break;
        case 'S':
            Move -= Camera->GetForwardVector() * ActualMoveSpeed * DeltaTime;
            break;
        case 'D':
            Move += Camera->GetRightVector() * ActualMoveSpeed * DeltaTime;
            break;
        case 'A':
            Move -= Camera->GetRightVector() * ActualMoveSpeed * DeltaTime;
            break;
        case 'E':
            Move += FVector::UpVector * ActualMoveSpeed * DeltaTime;
            break;
        case 'Q':
            Move -= FVector::UpVector * ActualMoveSpeed * DeltaTime;
            break;
        default:
            break;
        }

        TargetLocation += Move;
    }

    constexpr float AngleVelocity = 60.0f;
    const float RotateSpeed = AngleVelocity * RotateSensitivity;
    bool bRotationChanged = false;

    switch (KeyCode)
    {
    case VK_LEFT:
        Yaw -= RotateSpeed * DeltaTime;
        bRotationChanged = true;
        break;
    case VK_RIGHT:
        Yaw += RotateSpeed * DeltaTime;
        bRotationChanged = true;
        break;
    case VK_UP:
        Pitch += RotateSpeed * DeltaTime;
        bRotationChanged = true;
        break;
    case VK_DOWN:
        Pitch -= RotateSpeed * DeltaTime;
        bRotationChanged = true;
        break;
    default:
        break;
    }

    if (bRotationChanged)
    {
        Pitch = MathUtil::Clamp(Pitch, -89.0f, 89.0f);
        UpdateCameraRotation();
    }
}

void FEditorController::OnWheelScrolled(float Notch, float DeltaTime)
{
    if (!Camera || MathUtil::IsNearlyZero(Notch))
    {
        return;
    }

    if (Camera->IsOrthographic())
    {
        const float NewOrthoHeight = Camera->GetOrthoHeight() - Notch * 300.0f * MoveSensitivity * DeltaTime;
        Camera->SetOrthoHeight(MathUtil::Clamp(NewOrthoHeight, 0.1f, 1000.0f));
        return;
    }

    const FVector NewLocation = Camera->GetLocation() + Camera->GetForwardVector() * (Notch * ZoomSpeed);
    Camera->SetLocation(NewLocation);
    ResetTargetLocation();
}
