#pragma once

#include "Core/Containers/Array.h"
#include "Core/Containers/String.h"

namespace ActorTagRegistry
{
    // Runtime/editor discovered tags in this session.
    const TArray<FString>& GetUsedTagsView();

    // Config-defined tags loaded from Config/ActorTags.txt.
    const TArray<FString>& GetConfigTags();

    // Combined list used by editor tag pickers:
    // Built-in tags + config tags + discovered used tags.
    const TArray<FString>& GetProjectTags();

    // Registers a tag into the in-memory used-tag list.
    void RegisterUsedTag(const FString& InTag);

    // Persists a tag into Config/ActorTags.txt (append, no duplicates).
    bool PromoteTagToConfig(const FString& InTag);
}
