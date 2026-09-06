#include "Engine/Input/ViewportInputRouter.h"

#if WITH_EDITOR
FEditorController& FViewportInputRouter::GetEditorController()
{
    return EditorController;
}

const FEditorController& FViewportInputRouter::GetEditorController() const
{
    return EditorController;
}

FPIEController& FViewportInputRouter::GetPIEController()
{
    return PIEController;
}

const FPIEController& FViewportInputRouter::GetPIEController() const
{
    return PIEController;
}
#endif

void FViewportInputRouter::Tick(float DeltaTime)
{
    switch (Mode)
    {
#if WITH_EDITOR
    case EViewportInputMode::Editor:
        EditorController.Tick(DeltaTime);
        break;

    case EViewportInputMode::PIE:
        PIEController.Tick(DeltaTime, GameInputController);
        if (PIEController.ShouldBlockGameplayInput())
        {
            EditorController.Tick(DeltaTime);
        }
        else
        {
            GameInputController.Tick(DeltaTime);
        }
        break;
#endif

    case EViewportInputMode::Standalone:
        GameInputController.Tick(DeltaTime);
        break;
    }
}
