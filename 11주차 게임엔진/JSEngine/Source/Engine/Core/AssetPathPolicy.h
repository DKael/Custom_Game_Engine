#pragma once

#include "Core/Containers/String.h"
#include "Core/CoreTypes.h"

class FAssetPathPolicy
{
public:
	static bool FileExists(const FString& Path);
	static bool IsCurveAssetPath(const FString& Path);
	static bool IsSequenceAssetPath(const FString& Path);
	static bool IsAnimInstanceAssetPath(const FString& Path);
	static bool IsSerializedMaterialAssetPath(const FString& Path);
	static FString MakeCookedStaticMeshBinaryPath(const FString& SourcePath);
	static FString MakeSiblingStaticMeshBinaryPath(const FString& SourcePath);
	static FString MakeStaticMeshCacheBinaryPath(const FString& SourcePath);
	static FString MakeWritableStaticMeshCacheBinaryPath(const FString& SourcePath);

	static FString MakeWritableSkeletonCacheBinaryPath(const FString& SourcePath);
	static FString MakeWritableSkeletalMeshCacheBinaryPath(const FString& SourcePath);
	static FString MakeWritablePhysicsAssetCacheBinaryPath(const FString& SourcePath);
	static FString MakeWritableLegacySkeletalMeshCacheBinaryPath(const FString& SourcePath);
	static FString MakeWritableSkeletalImportManifestPath(const FString& SourcePath);
};
