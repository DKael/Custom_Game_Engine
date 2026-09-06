#pragma once

#include "Core/Containers/Array.h"
#include <functional>

class AActor;
class UActorComponent;

struct FComponentMenuEntry
{
    const char* DisplayName;
    const char* Category;
    std::function<UActorComponent*(AActor*)> Register;
};

// Editor component menu registry provider.
struct FEditorComponentFactory
{
    static const TArray<FComponentMenuEntry>& GetMenuRegistry();
};
