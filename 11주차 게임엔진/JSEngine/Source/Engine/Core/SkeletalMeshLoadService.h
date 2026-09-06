#pragma once

#include "Core/CoreMinimal.h"

class FResourceManager;
class UAnimSequence;
class USkeletalMesh;
struct FSkeletalMesh;

class FSkeletalMeshLoadService
{
public:
	explicit FSkeletalMeshLoadService(FResourceManager& InResourceManager);

	USkeletalMesh* Load(const FString& Path);

private:
	USkeletalMesh* LoadSourceOrCachedBinary(const FString& NormalizedPath);
	USkeletalMesh* LoadSkeletalMeshAssetFile(const FString& NormalizedPath);
	bool LoadExternalSkeletonAsset(FSkeletalMesh* MeshData, const FString& PreferredSkeletonAssetPath);

	USkeletalMesh* FinalizeLoadedMesh(
		FSkeletalMesh* MeshData,
		const FString& ResolvePath,
		const FString& CacheKey,
		const TArray<UAnimSequence*>& ImportedAnimationSequences,
		TArray<FString>* OutSavedAnimationSequencePaths = nullptr);

	FResourceManager& ResourceManager;
};
