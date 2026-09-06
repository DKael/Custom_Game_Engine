#include "Editor/Document/AssetEditorRegistry.h"

#include "Core/AssetPathPolicy.h"
#include "Core/Paths.h"
#include "Editor/EditorEngine.h"

bool FAssetEditorRegistry::OpenAsset(UEditorEngine* EditorEngine, const FString& AssetPath)
{
    if (!EditorEngine)
    {
        return false;
    }

    const FString NormalizedPath = FPaths::Normalize(AssetPath);
    if (FAssetPathPolicy::IsSequenceAssetPath(NormalizedPath))
    {
        EditorEngine->GetMainPanel().OpenAnimationSequenceAsset(NormalizedPath);
        return true;
    }

    return false;
}

bool FAssetEditorRegistry::SupportsAssetPath(const FString& AssetPath)
{
    return FAssetPathPolicy::IsSequenceAssetPath(FPaths::Normalize(AssetPath));
}
