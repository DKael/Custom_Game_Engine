#pragma once

#include "Core/Containers/String.h"

class AActor;

namespace LuaScriptPathUtils
{
    FString SanitizeFileName(const FString& Name);
    FString GetActorScriptTypeName(const AActor* Actor);
    FString GetActorTypeScriptPath(const AActor* Actor);
}
