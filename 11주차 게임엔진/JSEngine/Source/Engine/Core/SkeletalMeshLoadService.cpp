#include "Core/SkeletalMeshLoadService.h"

#include "Animation/AnimData/AnimSequence.h"
#include "Asset/PhysicsAsset.h"
#include "Asset/Skeleton.h"
#include "Core/AssetPathPolicy.h"
#include "Core/Logging/Log.h"
#include "Core/Paths.h"
#include "Core/ResourceManager.h"
#include "Object/ObjectFactory.h"
#include "SimpleJSON/json.hpp"

#include <algorithm>
#include <chrono>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <string>

namespace
{
    void DestroyOwnedSkeleton(USkeleton*& Skeleton)
    {
        if (!Skeleton)
        {
            return;
        }

        if (UObjectManager::Get().ContainsObject(Skeleton))
        {
            UObjectManager::Get().DestroyObject(Skeleton);
        }
        Skeleton = nullptr;
    }

    void DestroyImportedSequence(UAnimSequence*& Sequence)
    {
        if (!Sequence)
        {
            return;
        }

        UObjectManager::Get().DestroyObject(Sequence);
        Sequence = nullptr;
    }

    void DestroyImportedSequences(TArray<UAnimSequence*>& Sequences)
    {
        for (UAnimSequence*& Sequence : Sequences)
        {
            DestroyImportedSequence(Sequence);
        }
        Sequences.clear();
    }

    void DestroyPhysicsAsset(UPhysicsAsset*& PhysicsAsset)
    {
        if (!PhysicsAsset)
        {
            return;
        }

        UObjectManager::Get().DestroyObject(PhysicsAsset);
        PhysicsAsset = nullptr;
    }

    void DestroyLoadedMeshData(FSkeletalMesh*& MeshData)
    {
        if (!MeshData)
        {
            return;
        }

        DestroyOwnedSkeleton(MeshData->Skeleton);

        delete MeshData;
        MeshData = nullptr;
    }

	FString SanitizeAssetFileName(const FString& Name)
	{
		FString Result = Name.empty() ? FString("AnimSequence") : Name;
		for (char& Ch : Result)
		{
			const bool bInvalid =
				Ch == '<' || Ch == '>' || Ch == ':' || Ch == '"' ||
				Ch == '/' || Ch == '\\' || Ch == '|' || Ch == '?' || Ch == '*';
			if (bInvalid || static_cast<unsigned char>(Ch) < 32)
			{
				Ch = '_';
			}
		}
		return Result;
	}

	FString MakeSequenceAssetPath(const FString& SourcePath, const UAnimSequence* Sequence, int32 SequenceIndex)
	{
		const std::filesystem::path SourceFsPath(FPaths::ToWide(FPaths::Normalize(SourcePath)));
		const FString MeshName = SanitizeAssetFileName(FPaths::ToUtf8(SourceFsPath.stem().wstring()));
		FString SequenceName = Sequence ? FString(Sequence->GetName()) : FString();
		if (SequenceName.empty())
		{
			SequenceName = "Sequence_" + std::to_string(SequenceIndex);
		}
		SequenceName = SanitizeAssetFileName(SequenceName);

		const std::filesystem::path AssetPath =
			std::filesystem::path(L"Asset") /
			L"Animation" /
			FPaths::ToWide(MeshName) /
			(FPaths::ToWide(SequenceName) + L".sequence");

		return FPaths::Normalize(FPaths::ToUtf8(AssetPath.generic_wstring()));
	}

	struct FSkeletalImportManifest
	{
		FString SourcePath;
		uint64 SourceFileWriteTime = 0;
		FString SkeletalMeshAssetPath;
		FString SkeletonAssetPath;
		FString PhysicsAssetPath;
		TArray<FString> AnimationSequencePaths;
	};

	uint64 ParseUInt64String(const FString& Value)
	{
		try
		{
			return static_cast<uint64>(std::stoull(Value));
		}
		catch (...)
		{
			return 0;
		}
	}

	FString ToStoredAssetPath(const FString& Path)
	{
		return FPaths::ToProjectRelativePath(Path);
	}

	FString ReadManifestPath(json::JSON& Root, const char* Key, const char* LegacyKey)
	{
		if (Root.hasKey(Key))
		{
			return ToStoredAssetPath(Root[Key].ToString());
		}
		if (LegacyKey && Root.hasKey(LegacyKey))
		{
			return ToStoredAssetPath(Root[LegacyKey].ToString());
		}
		return FString();
	}

	void ApplySplitAssetPaths(
		FSkeletalMesh* MeshData,
		const FString& SkeletonAssetPath,
		TArray<UAnimSequence*>& AnimationSequences,
		UPhysicsAsset* PhysicsAsset,
		const FString& PhysicsAssetPath)
	{
		if (!MeshData)
		{
			return;
		}

		const FString StoredSkeletonAssetPath = ToStoredAssetPath(SkeletonAssetPath);
		const FString StoredPhysicsAssetPath = ToStoredAssetPath(PhysicsAssetPath);

		MeshData->SkeletonAssetPath = StoredSkeletonAssetPath;
		if (MeshData->Skeleton && MeshData->Skeleton->GetSkeletonData())
		{
			MeshData->Skeleton->GetSkeletonData()->PathFileName = StoredSkeletonAssetPath;
		}

		for (UAnimSequence* Sequence : AnimationSequences)
		{
			if (!Sequence)
			{
				continue;
			}

			Sequence->SetSkeleton(MeshData->Skeleton);
			Sequence->SetSkeletonAssetPath(StoredSkeletonAssetPath);
		}

		if (PhysicsAsset && PhysicsAsset->GetPhysicsAssetData())
		{
			PhysicsAsset->GetPhysicsAssetData()->PathFileName = StoredPhysicsAssetPath;
			PhysicsAsset->GetPhysicsAssetData()->SkeletonAssetPath = StoredSkeletonAssetPath;
			PhysicsAsset->SetSkeleton(MeshData->Skeleton);
		}
	}

	bool LoadSkeletalImportManifest(const FString& ManifestPath, FSkeletalImportManifest& OutManifest)
	{
		std::ifstream ManifestFile(std::filesystem::path(FPaths::ToAbsolute(FPaths::ToWide(ManifestPath))));
		if (!ManifestFile.is_open())
		{
			return false;
		}

		FString FileContent((std::istreambuf_iterator<char>(ManifestFile)), std::istreambuf_iterator<char>());
		json::JSON Root = json::JSON::Load(FileContent);
		if (Root.JSONType() != json::JSON::Class::Object)
		{
			return false;
		}

		if (!Root.hasKey("Type") || Root["Type"].ToString() != "SkeletalImportManifest")
		{
			return false;
		}

		const long Version = Root.hasKey("Version") ? Root["Version"].ToInt() : 0;
		if (Version != 1)
		{
			return false;
		}

		OutManifest.SourcePath = Root.hasKey("SourcePath") ? ToStoredAssetPath(Root["SourcePath"].ToString()) : FString();
		OutManifest.SourceFileWriteTime = Root.hasKey("SourceFileWriteTime")
			? ParseUInt64String(Root["SourceFileWriteTime"].ToString())
			: 0;
		OutManifest.SkeletalMeshAssetPath = ReadManifestPath(Root, "SkeletalMeshAssetPath", "SkeletalMeshBinaryPath");
		OutManifest.SkeletonAssetPath = ReadManifestPath(Root, "SkeletonAssetPath", "SkeletonBinaryPath");
		OutManifest.PhysicsAssetPath = ReadManifestPath(Root, "PhysicsAssetPath", "PhysicsAssetBinaryPath");
		OutManifest.AnimationSequencePaths.clear();

		if (Root.hasKey("AnimationSequencePaths") &&
			Root["AnimationSequencePaths"].JSONType() == json::JSON::Class::Array)
		{
			json::JSON& SequencePaths = Root["AnimationSequencePaths"];
			for (int32 Index = 0; Index < SequencePaths.length(); ++Index)
			{
				OutManifest.AnimationSequencePaths.push_back(ToStoredAssetPath(SequencePaths.at(static_cast<unsigned>(Index)).ToString()));
			}
		}

		return !OutManifest.SourcePath.empty() &&
			OutManifest.SourceFileWriteTime != 0 &&
			!OutManifest.SkeletalMeshAssetPath.empty() &&
			!OutManifest.SkeletonAssetPath.empty();
	}

	bool SaveSkeletalImportManifest(const FString& ManifestPath, const FSkeletalImportManifest& Manifest)
	{
		if (Manifest.SourcePath.empty() ||
			Manifest.SourceFileWriteTime == 0 ||
			Manifest.SkeletalMeshAssetPath.empty() ||
			Manifest.SkeletonAssetPath.empty())
		{
			return false;
		}

		json::JSON Root = json::JSON::Make(json::JSON::Class::Object);
		Root["Type"] = "SkeletalImportManifest";
		Root["Version"] = 1;
		Root["SourcePath"] = ToStoredAssetPath(Manifest.SourcePath);
		Root["SourceFileWriteTime"] = std::to_string(Manifest.SourceFileWriteTime);
		Root["SkeletalMeshAssetPath"] = ToStoredAssetPath(Manifest.SkeletalMeshAssetPath);
		Root["SkeletonAssetPath"] = ToStoredAssetPath(Manifest.SkeletonAssetPath);
		Root["PhysicsAssetPath"] = ToStoredAssetPath(Manifest.PhysicsAssetPath);

		Root["AnimationSequencePaths"] = json::Array();
		for (const FString& SequencePath : Manifest.AnimationSequencePaths)
		{
			Root["AnimationSequencePaths"].append(ToStoredAssetPath(SequencePath));
		}

		std::error_code ErrorCode;
		const std::filesystem::path FilePath(FPaths::ToAbsolute(FPaths::ToWide(ManifestPath)));
		std::filesystem::create_directories(FilePath.parent_path(), ErrorCode);

		std::ofstream OutFile(FilePath);
		if (!OutFile.is_open())
		{
			return false;
		}

		OutFile << Root.dump(4);
		return OutFile.good();
	}

	bool IsSkeletalImportManifestFresh(
		const FSkeletalImportManifest& Manifest,
		const FString& NormalizedSourcePath,
		uint64 SourceWriteTime,
		const FString& ExpectedSkeletalMeshAssetPath,
		const FString& ExpectedSkeletonAssetPath,
		const FString& ExpectedPhysicsAssetPath)
	{
		if (ToStoredAssetPath(Manifest.SourcePath) != ToStoredAssetPath(NormalizedSourcePath) ||
			Manifest.SourceFileWriteTime != SourceWriteTime ||
			SourceWriteTime == 0)
		{
			return false;
		}

		if (ToStoredAssetPath(Manifest.SkeletalMeshAssetPath) != ToStoredAssetPath(ExpectedSkeletalMeshAssetPath) ||
			ToStoredAssetPath(Manifest.SkeletonAssetPath) != ToStoredAssetPath(ExpectedSkeletonAssetPath))
		{
			return false;
		}

		if (!Manifest.PhysicsAssetPath.empty() &&
			ToStoredAssetPath(Manifest.PhysicsAssetPath) != ToStoredAssetPath(ExpectedPhysicsAssetPath))
		{
			return false;
		}

		if (!FAssetPathPolicy::FileExists(Manifest.SkeletalMeshAssetPath) ||
			!FAssetPathPolicy::FileExists(Manifest.SkeletonAssetPath))
		{
			return false;
		}

		if (!Manifest.PhysicsAssetPath.empty() &&
			!FAssetPathPolicy::FileExists(Manifest.PhysicsAssetPath))
		{
			return false;
		}

		for (const FString& SequencePath : Manifest.AnimationSequencePaths)
		{
			if (!FAssetPathPolicy::FileExists(SequencePath))
			{
				return false;
			}
		}

		return true;
	}
}

FSkeletalMeshLoadService::FSkeletalMeshLoadService(FResourceManager& InResourceManager)
	: ResourceManager(InResourceManager)
{
}

bool FSkeletalMeshLoadService::LoadExternalSkeletonAsset(
	FSkeletalMesh* MeshData,
	const FString& PreferredSkeletonAssetPath)
{
	if (!MeshData)
	{
		return false;
	}

	const FString SkeletonAssetPath = ToStoredAssetPath(
		!PreferredSkeletonAssetPath.empty() ? PreferredSkeletonAssetPath : MeshData->SkeletonAssetPath);
	if (SkeletonAssetPath.empty())
	{
		return MeshData->HasValidSkeletonData();
	}

	USkeleton* LoadedSkeleton = UObjectManager::Get().CreateObject<USkeleton>();
	if (!LoadedSkeleton || !ResourceManager.BinarySerializer.LoadSkeleton(SkeletonAssetPath, *LoadedSkeleton))
	{
		if (LoadedSkeleton)
		{
			DestroyOwnedSkeleton(LoadedSkeleton);
		}
		return false;
	}

	if (MeshData->Skeleton && MeshData->Skeleton != LoadedSkeleton)
	{
		DestroyOwnedSkeleton(MeshData->Skeleton);
	}

	MeshData->Skeleton = LoadedSkeleton;
	MeshData->SkeletonAssetPath = SkeletonAssetPath;
	if (LoadedSkeleton->GetSkeletonData())
	{
		LoadedSkeleton->GetSkeletonData()->PathFileName = SkeletonAssetPath;
	}

	return true;
}

USkeletalMesh* FSkeletalMeshLoadService::Load(const FString& Path)
{
	const FString NormalizedPath = ToStoredAssetPath(Path);

	if (USkeletalMesh* FoundMesh = ResourceManager.FindSkeletalMesh(NormalizedPath))
	{
		return FoundMesh;
	}

	std::filesystem::path RequestedFsPath(FPaths::ToWide(NormalizedPath));
	std::wstring RequestedExt = RequestedFsPath.extension().wstring();
	std::transform(RequestedExt.begin(), RequestedExt.end(), RequestedExt.begin(), ::towlower);
	if (RequestedExt == L".skmesh")
	{
		return LoadSkeletalMeshAssetFile(NormalizedPath);
	}

	ResourceManager.LoadMaterial(NormalizedPath, EMaterialShaderType::SurfaceLit);

	return LoadSourceOrCachedBinary(NormalizedPath);
}

USkeletalMesh* FSkeletalMeshLoadService::LoadSkeletalMeshAssetFile(const FString& NormalizedPath)
{
	const auto BinaryStart = std::chrono::steady_clock::now();

	FSkeletalMesh* LoadedMeshData = new FSkeletalMesh();
	if (!ResourceManager.BinarySerializer.LoadSkeletalMesh(NormalizedPath, *LoadedMeshData))
	{
		DestroyLoadedMeshData(LoadedMeshData);
		UE_LOG_WARNING("[SkeletalMeshLoad] Failed skeletal mesh asset load | Path=%s", NormalizedPath.c_str());
		return nullptr;
	}

	const FString ResolvePath = LoadedMeshData->PathFileName.empty()
		? NormalizedPath
		: ToStoredAssetPath(LoadedMeshData->PathFileName);

	const FString SkeletonAssetPath = ToStoredAssetPath(LoadedMeshData->SkeletonAssetPath);
	if (!LoadExternalSkeletonAsset(LoadedMeshData, SkeletonAssetPath))
	{
		DestroyLoadedMeshData(LoadedMeshData);
		UE_LOG_WARNING("[SkeletalMeshLoad] Failed skeletal mesh asset skeleton load | Path=%s | Skeleton=%s",
		               NormalizedPath.c_str(), SkeletonAssetPath.c_str());
		return nullptr;
	}

	if (!ResolvePath.empty() && FAssetPathPolicy::FileExists(ResolvePath))
	{
		ResourceManager.LoadMaterial(ResolvePath, EMaterialShaderType::SurfaceLit);
	}

	LoadedMeshData->PathFileName = ResolvePath.empty() ? NormalizedPath : ResolvePath;
	USkeletalMesh* LoadedMesh = FinalizeLoadedMesh(
		LoadedMeshData,
		ResolvePath,
		NormalizedPath,
		TArray<UAnimSequence*>{},
		nullptr);

	const auto BinaryEnd = std::chrono::steady_clock::now();
	const double BinaryLoadSec = std::chrono::duration<double>(BinaryEnd - BinaryStart).count();
	UE_LOG("[SkeletalMeshLoad] Source=SkeletalMeshAsset | Path=%s | BinarySec=%.6f | Source=%s",
	       NormalizedPath.c_str(),
	       BinaryLoadSec,
	       ResolvePath.c_str());

	return LoadedMesh;
}

USkeletalMesh* FSkeletalMeshLoadService::LoadSourceOrCachedBinary(const FString& NormalizedPath)
{
	FStaticMeshLoadOptions LoadOptions;
	const FString SkeletalMeshAssetPath = FAssetPathPolicy::MakeWritableSkeletalMeshCacheBinaryPath(NormalizedPath);
	const FString SkeletonAssetPath = FAssetPathPolicy::MakeWritableSkeletonCacheBinaryPath(NormalizedPath);
	const FString PhysicsAssetPath = FAssetPathPolicy::MakeWritablePhysicsAssetCacheBinaryPath(NormalizedPath);
	const FString LegacySkeletalMeshBinaryPath = FAssetPathPolicy::MakeWritableLegacySkeletalMeshCacheBinaryPath(NormalizedPath);
	const FString ImportManifestPath = FAssetPathPolicy::MakeWritableSkeletalImportManifestPath(NormalizedPath);
	const uint64 SourceWriteTime = ResourceManager.GetFileWriteTimeTicks(NormalizedPath);

	FSkeletalMesh* LoadedMeshData = nullptr;
	TArray<UAnimSequence*> ImportedAnimationSequences;
	UPhysicsAsset* ImportedPhysicsAsset = nullptr;
	FString SavedPhysicsAssetPath;
	bool bLoadedFromImportManifest = false;
	bool bLoadedLegacyBinary = false;
	bool bSplitCacheReadyForManifest = false;
	double BinaryLoadSec = 0.0;
	double SourceLoadSec = 0.0;

	FSkeletalImportManifest CachedManifest;
	if (LoadSkeletalImportManifest(ImportManifestPath, CachedManifest) &&
		IsSkeletalImportManifestFresh(
			CachedManifest,
			NormalizedPath,
			SourceWriteTime,
			SkeletalMeshAssetPath,
			SkeletonAssetPath,
			PhysicsAssetPath))
	{
		const auto BinaryStart = std::chrono::steady_clock::now();

		LoadedMeshData = new FSkeletalMesh();
		if (!ResourceManager.BinarySerializer.LoadSkeletalMesh(CachedManifest.SkeletalMeshAssetPath, *LoadedMeshData))
		{
			DestroyLoadedMeshData(LoadedMeshData);
		}
		else if (!LoadExternalSkeletonAsset(LoadedMeshData, CachedManifest.SkeletonAssetPath))
		{
			DestroyLoadedMeshData(LoadedMeshData);
		}

		const auto BinaryEnd = std::chrono::steady_clock::now();
		BinaryLoadSec = std::chrono::duration<double>(BinaryEnd - BinaryStart).count();

		if (LoadedMeshData)
		{
			LoadedMeshData->PathFileName = NormalizedPath;
			bLoadedFromImportManifest = true;
			UE_LOG("[SkeletalMeshLoad] Source=ImportManifest | Path=%s | BinarySec=%.6f | Manifest=%s",
			       NormalizedPath.c_str(), BinaryLoadSec, ImportManifestPath.c_str());
		}
	}

	// Manifest 도입 이전의 v1~v3 embedded .bin만 migration 후보로 수용한다.
	if (LoadedMeshData == nullptr && SourceWriteTime != 0)
	{
		FSkeletalMeshBinaryHeader BinaryHeader;
		const bool bHasBinaryHeader = ResourceManager.BinarySerializer.ReadSkeletalMeshHeader(LegacySkeletalMeshBinaryPath, BinaryHeader);
		bLoadedLegacyBinary = bHasBinaryHeader &&
			BinaryHeader.Version < 4 &&
			BinaryHeader.SourceFileWriteTime == SourceWriteTime;
	}

	if (LoadedMeshData == nullptr && bLoadedLegacyBinary)
	{
		const auto BinaryStart = std::chrono::steady_clock::now();

		LoadedMeshData = new FSkeletalMesh();
		if (!ResourceManager.BinarySerializer.LoadSkeletalMesh(LegacySkeletalMeshBinaryPath, *LoadedMeshData, &ImportedAnimationSequences))
		{
			DestroyLoadedMeshData(LoadedMeshData);
		}

		if (LoadedMeshData && bLoadedLegacyBinary)
		{
			ApplySplitAssetPaths(LoadedMeshData, SkeletonAssetPath, ImportedAnimationSequences, nullptr, FString());

			bool bSaveSkeletonOk = false;
			if (LoadedMeshData->Skeleton)
			{
				bSaveSkeletonOk = ResourceManager.BinarySerializer.SaveSkeleton(
					SkeletonAssetPath,
					NormalizedPath,
					*LoadedMeshData->Skeleton);
			}

			if (bSaveSkeletonOk)
			{
				const bool bSaveConvertedMeshOk = ResourceManager.BinarySerializer.SaveSkeletalMesh(
					SkeletalMeshAssetPath,
					NormalizedPath,
					*LoadedMeshData);
				bSplitCacheReadyForManifest = bSaveConvertedMeshOk;
				if (bSaveConvertedMeshOk)
				{
					LoadedMeshData->PathFileName = NormalizedPath;
					UE_LOG("[SkeletalMeshLoad] Legacy skeletal cache converted | Path=%s | Skeleton=%s | Mesh=%s | Animations=%zu",
					       NormalizedPath.c_str(),
					       SkeletonAssetPath.c_str(),
					       SkeletalMeshAssetPath.c_str(),
					       ImportedAnimationSequences.size());
				}
				else
				{
					UE_LOG_WARNING("[SkeletalMeshLoad] Legacy skeletal mesh cache conversion failed; legacy cache kept | Path=%s | BinaryPath=%s",
					               NormalizedPath.c_str(),
					               SkeletalMeshAssetPath.c_str());
				}
			}
			else
			{
				UE_LOG_WARNING("[SkeletalMeshLoad] Legacy skeleton cache conversion failed; legacy mesh cache kept | Path=%s | SkeletonBinaryPath=%s",
				               NormalizedPath.c_str(),
				               SkeletonAssetPath.c_str());
			}

			if (LoadedMeshData && bSaveSkeletonOk &&
				!LoadExternalSkeletonAsset(LoadedMeshData, SkeletonAssetPath))
			{
				DestroyLoadedMeshData(LoadedMeshData);
			}
		}

		const auto BinaryEnd = std::chrono::steady_clock::now();
		BinaryLoadSec = std::chrono::duration<double>(BinaryEnd - BinaryStart).count();
	}

	// Binary/manifest 실패 또는 누락이면 FBX에서 import 후 split cache와 manifest를 굽는다.
	if (LoadedMeshData == nullptr)
	{
		const auto SourceStart = std::chrono::steady_clock::now();
		FImportedSkeletalAsset ImportedAsset = ResourceManager.FbxImporter.ImportSkeletalAsset(NormalizedPath, LoadOptions);
		LoadedMeshData = ImportedAsset.SkeletalMesh;
		ImportedAnimationSequences = ImportedAsset.AnimationSequences;
		ImportedPhysicsAsset = ImportedAsset.PhysicsAsset;
		const auto SourceEnd = std::chrono::steady_clock::now();
		SourceLoadSec = std::chrono::duration<double>(SourceEnd - SourceStart).count();

		if (!LoadedMeshData)
		{
			DestroyImportedSequences(ImportedAnimationSequences);
			DestroyPhysicsAsset(ImportedPhysicsAsset);
			UE_LOG_ERROR("[SkeletalMeshLoad] Failed | Path=%s | BinarySec=%.6f | FbxSec=%.6f",
				NormalizedPath.c_str(), BinaryLoadSec, SourceLoadSec);
			return nullptr;
		}

		ApplySplitAssetPaths(LoadedMeshData, SkeletonAssetPath, ImportedAnimationSequences, ImportedPhysicsAsset, PhysicsAssetPath);

		bool bSaveSkeletonOk = false;
		if (LoadedMeshData->Skeleton)
		{
			bSaveSkeletonOk = ResourceManager.BinarySerializer.SaveSkeleton(
				SkeletonAssetPath,
				NormalizedPath,
				*LoadedMeshData->Skeleton);
			if (bSaveSkeletonOk)
			{
				UE_LOG("[SkeletalMeshLoad] Skeleton cache saved | Path=%s | BinaryPath=%s",
				       NormalizedPath.c_str(), SkeletonAssetPath.c_str());
			}
			else
			{
				UE_LOG_WARNING("[SkeletalMeshLoad] Skeleton cache save failed | Path=%s | BinaryPath=%s",
				               NormalizedPath.c_str(), SkeletonAssetPath.c_str());
			}
		}

		bool bSavePhysicsAssetOk = false;
		if (ImportedPhysicsAsset)
		{
			bSavePhysicsAssetOk = ResourceManager.BinarySerializer.SavePhysicsAsset(
				PhysicsAssetPath,
				NormalizedPath,
				*ImportedPhysicsAsset);
			if (bSavePhysicsAssetOk)
			{
				SavedPhysicsAssetPath = PhysicsAssetPath;
				UE_LOG("[SkeletalMeshLoad] Physics asset saved | Path=%s | AssetPath=%s",
				       NormalizedPath.c_str(), PhysicsAssetPath.c_str());
			}
			else
			{
				UE_LOG_WARNING("[SkeletalMeshLoad] Physics asset save failed | Path=%s | AssetPath=%s",
				               NormalizedPath.c_str(), PhysicsAssetPath.c_str());
			}
		}

		// Material 포인터는 직렬화 대상이 아니므로 이 시점에 그대로 굽고, resolve는 Finalize에서 한 번만.
		const bool bSaveBinaryOk = ResourceManager.BinarySerializer.SaveSkeletalMesh(SkeletalMeshAssetPath, NormalizedPath, *LoadedMeshData);
		bSplitCacheReadyForManifest = bSaveSkeletonOk && bSaveBinaryOk && (!ImportedPhysicsAsset || bSavePhysicsAssetOk);
		if (bSaveBinaryOk)
		{
			LoadedMeshData->PathFileName = NormalizedPath;
			UE_LOG("[SkeletalMeshLoad] Source=FBX | Path=%s | FbxSec=%.6f | BinarySave=OK | BinaryPath=%s",
			       NormalizedPath.c_str(), SourceLoadSec, SkeletalMeshAssetPath.c_str());
		}
		else
		{
			UE_LOG_WARNING("[SkeletalMeshLoad] Source=FBX | Path=%s | FbxSec=%.6f | BinarySave=FAIL | BinaryPath=%s",
			               NormalizedPath.c_str(), SourceLoadSec, SkeletalMeshAssetPath.c_str());
		}
	}
	else
	{
		if (!bLoadedFromImportManifest)
		{
			UE_LOG("[SkeletalMeshLoad] Source=Binary | Path=%s | BinarySec=%.6f | BinaryPath=%s",
			       NormalizedPath.c_str(), BinaryLoadSec, LegacySkeletalMeshBinaryPath.c_str());
		}
	}

	TArray<FString> SavedAnimationSequencePaths;
	USkeletalMesh* LoadedMesh = FinalizeLoadedMesh(
		LoadedMeshData,
		NormalizedPath,
		NormalizedPath,
		ImportedAnimationSequences,
		&SavedAnimationSequencePaths);

	const bool bAllImportedSequencesSaved = SavedAnimationSequencePaths.size() == ImportedAnimationSequences.size();
	if (LoadedMesh && bSplitCacheReadyForManifest && bAllImportedSequencesSaved)
	{
		FSkeletalImportManifest NewManifest;
		NewManifest.SourcePath = NormalizedPath;
		NewManifest.SourceFileWriteTime = SourceWriteTime;
		NewManifest.SkeletalMeshAssetPath = SkeletalMeshAssetPath;
		NewManifest.SkeletonAssetPath = SkeletonAssetPath;
		NewManifest.PhysicsAssetPath = SavedPhysicsAssetPath;
		NewManifest.AnimationSequencePaths = SavedAnimationSequencePaths;

		if (SaveSkeletalImportManifest(ImportManifestPath, NewManifest))
		{
			UE_LOG("[SkeletalMeshLoad] Import manifest saved | Path=%s | Manifest=%s",
			       NormalizedPath.c_str(), ImportManifestPath.c_str());
		}
		else
		{
			UE_LOG_WARNING("[SkeletalMeshLoad] Failed to save import manifest | Path=%s | Manifest=%s",
			               NormalizedPath.c_str(), ImportManifestPath.c_str());
		}
	}
	else if (LoadedMesh && bSplitCacheReadyForManifest && !bAllImportedSequencesSaved)
	{
		UE_LOG_WARNING("[SkeletalMeshLoad] Import manifest skipped because not all animation sequences were saved | Path=%s | Saved=%zu | Imported=%zu",
		               NormalizedPath.c_str(),
		               SavedAnimationSequencePaths.size(),
		               ImportedAnimationSequences.size());
	}

	DestroyPhysicsAsset(ImportedPhysicsAsset);

	return LoadedMesh;
}

USkeletalMesh* FSkeletalMeshLoadService::FinalizeLoadedMesh(
	FSkeletalMesh* MeshData,
	const FString& ResolvePath,
	const FString& CacheKey,
	const TArray<UAnimSequence*>& ImportedAnimationSequences,
	TArray<FString>* OutSavedAnimationSequencePaths)
{
	ResourceManager.ResolveSkeletalMeshMaterialSlots(ResolvePath, MeshData);

	USkeletalMesh* LoadedMesh = UObjectManager::Get().CreateObject<USkeletalMesh>();
	LoadedMesh->SetMeshData(MeshData);

	ResourceManager.SkeletalMeshMap[CacheKey] = LoadedMesh;
	if (std::find(ResourceManager.SkeletalMeshFilePaths.begin(), ResourceManager.SkeletalMeshFilePaths.end(), CacheKey)
		== ResourceManager.SkeletalMeshFilePaths.end())
	{
		ResourceManager.SkeletalMeshFilePaths.push_back(CacheKey);
	}

	const FString MeshAssetPath = ToStoredAssetPath(LoadedMesh->GetAssetPathFileName());
	if (!MeshAssetPath.empty() && MeshAssetPath != CacheKey)
	{
		ResourceManager.SkeletalMeshMap[MeshAssetPath] = LoadedMesh;
		if (std::find(ResourceManager.SkeletalMeshFilePaths.begin(), ResourceManager.SkeletalMeshFilePaths.end(), MeshAssetPath)
			== ResourceManager.SkeletalMeshFilePaths.end())
		{
			ResourceManager.SkeletalMeshFilePaths.push_back(MeshAssetPath);
		}
	}

	const auto SequenceSaveTotalStart = std::chrono::steady_clock::now();
	int32 AttemptedSequenceSaveCount = 0;
	int32 SavedSequenceSaveCount = 0;
	int32 FailedSequenceSaveCount = 0;
	for (int32 SequenceIndex = 0; SequenceIndex < static_cast<int32>(ImportedAnimationSequences.size()); ++SequenceIndex)
	{
		UAnimSequence* Sequence = ImportedAnimationSequences[SequenceIndex];
		if (!Sequence)
		{
			continue;
		}

		if (!Sequence->GetSkeleton())
		{
			Sequence->SetSkeleton(LoadedMesh->GetSkeleton());
		}
		if (Sequence->GetSkeletonAssetPath().empty())
		{
			Sequence->SetSkeletonAssetPath(ToStoredAssetPath(LoadedMesh->GetSkeletonAssetPath()));
		}

		const FString SequenceAssetPath = MakeSequenceAssetPath(ResolvePath, Sequence, SequenceIndex);
		++AttemptedSequenceSaveCount;
		if (ResourceManager.SaveAnimSequence(SequenceAssetPath, Sequence))
		{
			++SavedSequenceSaveCount;
			if (OutSavedAnimationSequencePaths)
			{
				OutSavedAnimationSequencePaths->push_back(SequenceAssetPath);
			}

			DestroyImportedSequence(Sequence);
		}
		else
		{
			++FailedSequenceSaveCount;
			UE_LOG_WARNING("[SkeletalMeshLoad] Failed to save animation sequence asset | Mesh=%s | Sequence=%s | Path=%s",
			               CacheKey.c_str(),
			               Sequence->GetName().c_str(),
			               SequenceAssetPath.c_str());
			DestroyImportedSequence(Sequence);
		}
	}

	if (!ImportedAnimationSequences.empty())
	{
		const auto SequenceSaveTotalEnd = std::chrono::steady_clock::now();
		const double SequenceSaveTotalSec = std::chrono::duration<double>(SequenceSaveTotalEnd - SequenceSaveTotalStart).count();
		UE_LOG("[SkeletalMeshLoad] Animation Sequence Save Total | Format=RawBinary | Mesh=%s | Imported=%zu | Attempted=%d | Saved=%d | Failed=%d | Sec=%.6f | Ms=%.3f",
		       CacheKey.c_str(),
		       ImportedAnimationSequences.size(),
		       AttemptedSequenceSaveCount,
		       SavedSequenceSaveCount,
		       FailedSequenceSaveCount,
		       SequenceSaveTotalSec,
		       SequenceSaveTotalSec * 1000.0);
	}

	UE_LOG("[SkeletalMeshLoad] Loaded | Path=%s | Vertices=%zu | Indices=%zu | Bones=%zu | Sections=%zu",
	       CacheKey.c_str(),
	       LoadedMesh->GetVertices().size(),
	       LoadedMesh->GetIndices().size(),
	       LoadedMesh->GetBones().size(),
	       LoadedMesh->GetSections().size());

	return LoadedMesh;
}
