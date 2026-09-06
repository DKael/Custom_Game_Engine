#pragma once

#include "Core/CoreMinimal.h"

class UEditorEngine;

class FAssetEditorRegistry
{
public:
    static bool OpenAsset(UEditorEngine* EditorEngine, const FString& AssetPath);
    static bool SupportsAssetPath(const FString& AssetPath);
};
