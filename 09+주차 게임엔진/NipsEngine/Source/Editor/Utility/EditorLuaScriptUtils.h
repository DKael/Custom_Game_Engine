#pragma once

#include "Core/Containers/String.h"
#include "Core/CoreTypes.h"

class UScriptComponent;

namespace EditorLuaScriptUtils
{
    bool CreateActorTypeScript(UScriptComponent* ScriptComponent);
    bool ApplyScriptToSameTypeActors(UScriptComponent* ScriptComponent, int32& OutUpdatedActors, int32& OutSkippedActors);
    bool OpenScriptInExternalEditor(const FString& ScriptPath);
}
