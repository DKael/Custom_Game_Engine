#include "Editor/Input/PIEController.h"

#include "Engine/Input/GameInputController.h"
#include "Engine/Input/InputSystem.h"

void FPIEController::Tick(float DeltaTime, FGameInputController& GameInputController)
{
    (void)DeltaTime;

    InputSystem& Input = InputSystem::Get();
    if (Input.GetKeyDown(VK_ESCAPE) && EndPIECallback)
    {
        EndPIECallback();
        GameInputController.SetUIMode(true);
        return;
    }

    if (Input.GetKeyDown(VK_F4))
    {
        bHostControlReleased = !bHostControlReleased;
    }

    GameInputController.SetUIMode(bHostControlReleased);
}

void FPIEController::Reset()
{
    bHostControlReleased = false;
}
