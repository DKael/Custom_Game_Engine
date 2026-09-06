#pragma once

#include "Math/Vector.h"

class UWorld;
class UGizmoComponent;
class FViewportCamera;
class FSelectionManager;
class FEditorSettings;
class FEditorViewportClient;

class FEditorController
{
public:
    void SetViewportClient(FEditorViewportClient* InViewportClient) { ViewportClient = InViewportClient; }
    void SetWorld(UWorld* InWorld) { World = InWorld; }
    void SetSelectionManager(FSelectionManager* InSelectionManager);
    void SetGizmo(UGizmoComponent* InGizmo) { Gizmo = InGizmo; }
    void SetCamera(FViewportCamera* InCamera);
    void SetSettings(const FEditorSettings* InSettings) { Settings = InSettings; }
    void SetViewportRect(float InX, float InY, float InWidth, float InHeight);

    void Tick(float DeltaTime);
    void ResetTargetLocation();

    float GetMoveSpeed() const { return MoveSpeed; }
    void SetMoveSpeed(float InSpeed) { MoveSpeed = InSpeed; }

    float GetMoveSensitivity() const { return MoveSensitivity; }
    void SetMoveSensitivity(float InSensitivity) { MoveSensitivity = InSensitivity; }

    float GetRotateSensitivity() const { return RotateSensitivity; }
    void SetRotateSensitivity(float InSensitivity) { RotateSensitivity = InSensitivity; }

    float GetZoomSpeed() const { return ZoomSpeed; }
    void SetZoomSpeed(float InSpeed) { ZoomSpeed = InSpeed; }

private:
    void TickMouseInput();
    void TickKeyboardInput(float DeltaTime);
    void TickEditorShortcuts();
    void SyncAnglesFromCamera();
    void UpdateCameraRotation();

    void OnMouseMoveAbsolute(float X, float Y);
    void OnLeftMouseClick(float X, float Y);
    void OnLeftMouseDrag(float X, float Y);
    void OnLeftMouseDragEnd(float X, float Y);
    void OnLeftMouseButtonUp(float X, float Y);
    void OnRightMouseClick();
    void OnRightMouseDrag(float DeltaX, float DeltaY);
    void OnMiddleMouseDrag(float DeltaX, float DeltaY, float DeltaTime);
    void OnKeyPressed(int32 KeyCode);
    void OnKeyDown(int32 KeyCode, float DeltaTime);
    void OnWheelScrolled(float Notch, float DeltaTime);

private:
    FEditorViewportClient* ViewportClient = nullptr;
    UWorld* World = nullptr;
    FSelectionManager* SelectionManager = nullptr;
    UGizmoComponent* Gizmo = nullptr;
    FViewportCamera* Camera = nullptr;
    const FEditorSettings* Settings = nullptr;

    float ViewportX = 0.0f;
    float ViewportY = 0.0f;
    float ViewportWidth = 0.0f;
    float ViewportHeight = 0.0f;

    float Yaw = 0.0f;
    float Pitch = 0.0f;
    float MoveSpeed = 15.0f;
    float MoveSensitivity = 1.0f;
    float RotateSensitivity = 1.0f;
    float ZoomSpeed = 15.0f;

    FVector TargetLocation = FVector::ZeroVector;
    bool bTargetLocationInitialized = false;
};
