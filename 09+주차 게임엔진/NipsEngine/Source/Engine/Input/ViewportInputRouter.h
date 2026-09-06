#pragma once

#include "Core/CoreTypes.h"
#include "Engine/Input/GameInputController.h"

class FEditorController;
class FPIEController;

#if WITH_EDITOR
#include "Editor/Input/EditorController.h"
#include "Editor/Input/PIEController.h"
#endif

enum class EViewportInputMode : uint8
{
    Editor,
    PIE,
    Standalone,
};

class FViewportInputRouter
{
public:
    void SetMode(EViewportInputMode InMode) { Mode = InMode; }
    EViewportInputMode GetMode() const { return Mode; }

    void Tick(float DeltaTime);

    FGameInputController& GetGameInputController() { return GameInputController; }
    const FGameInputController& GetGameInputController() const { return GameInputController; }

    FEditorController& GetEditorController();
    const FEditorController& GetEditorController() const;

    FPIEController& GetPIEController();
    const FPIEController& GetPIEController() const;

private:
    EViewportInputMode Mode = EViewportInputMode::Standalone;
    FGameInputController GameInputController;

#if WITH_EDITOR
    FEditorController EditorController;
    FPIEController PIEController;
#endif
};
