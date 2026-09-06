#pragma once

#include "Asset/IAssetLoader.h"

class UAnimInstanceAsset;

class FAnimInstanceAssetLoader : public IAssetLoader
{
public:
    UAnimInstanceAsset* Load(const FString& Path) const;
    bool Save(const FString& Path, const UAnimInstanceAsset* Asset) const;

    bool SupportsExtension(const FString& Extension) const override;
    FString GetLoaderName() const override;
};
