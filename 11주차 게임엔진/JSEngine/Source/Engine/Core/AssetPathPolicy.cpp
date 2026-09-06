#include "Core/AssetPathPolicy.h"

#include "Core/Paths.h"

#include <algorithm>
#include <cctype>
#include <cwctype>
#include <filesystem>

namespace
{
	bool EndsWith(const FString& Value, const FString& Suffix)
	{
		return Value.size() >= Suffix.size() &&
			Value.compare(Value.size() - Suffix.size(), Suffix.size(), Suffix) == 0;
	}

	FString MakeWritableAssetPath(const std::filesystem::path& RelativeDir, const std::filesystem::path& FileName)
	{
		const std::filesystem::path AbsoluteDir = std::filesystem::path(FPaths::RootDir()) / RelativeDir;
		if (!std::filesystem::exists(AbsoluteDir))
		{
			std::filesystem::create_directories(AbsoluteDir);
		}

		const std::filesystem::path RelativePath = RelativeDir / FileName;
		return FPaths::Normalize(FPaths::ToUtf8(RelativePath.generic_wstring()));
	}
}

bool FAssetPathPolicy::FileExists(const FString& Path)
{
	std::error_code Ec;
	return std::filesystem::exists(std::filesystem::path(FPaths::ToAbsolute(FPaths::ToWide(Path))), Ec) && !Ec;
}

bool FAssetPathPolicy::IsCurveAssetPath(const FString& Path)
{
	std::filesystem::path FsPath(FPaths::ToWide(FPaths::Normalize(Path)));
	FString FileName = FPaths::ToUtf8(FsPath.filename().wstring());
	std::transform(
		FileName.begin(),
		FileName.end(),
		FileName.begin(),
		[](unsigned char Ch)
		{
			return static_cast<char>(std::tolower(Ch));
		});

	return EndsWith(FileName, ".curve");
}

bool FAssetPathPolicy::IsSequenceAssetPath(const FString& Path)
{
	std::filesystem::path FsPath(FPaths::ToWide(FPaths::Normalize(Path)));
	std::wstring Extension = FsPath.extension().wstring();
	std::transform(Extension.begin(), Extension.end(), Extension.begin(), ::towlower);
	return Extension == L".sequence";
}

bool FAssetPathPolicy::IsAnimInstanceAssetPath(const FString& Path)
{
	std::filesystem::path FsPath(FPaths::ToWide(FPaths::Normalize(Path)));
	std::wstring Extension = FsPath.extension().wstring();
	std::transform(Extension.begin(), Extension.end(), Extension.begin(), ::towlower);
	return Extension == L".animinstance";
}

bool FAssetPathPolicy::IsSerializedMaterialAssetPath(const FString& Path)
{
	std::filesystem::path FsPath(FPaths::ToWide(FPaths::Normalize(Path)));
	std::wstring Extension = FsPath.extension().wstring();
	std::transform(Extension.begin(), Extension.end(), Extension.begin(), ::towlower);
	return Extension == L".mat" || Extension == L".matinst";
}

FString FAssetPathPolicy::MakeCookedStaticMeshBinaryPath(const FString& SourcePath)
{
	std::filesystem::path SourceFsPath(FPaths::ToWide(FPaths::Normalize(SourcePath)));
	const std::filesystem::path AssetMeshRoot = std::filesystem::path(L"Asset") / L"Mesh";
	std::filesystem::path SubPath = SourceFsPath.lexically_normal().lexically_relative(AssetMeshRoot);
	if (SubPath.empty())
	{
		SubPath = SourceFsPath.filename();
	}
	else
	{
		for (const std::filesystem::path& Part : SubPath)
		{
			if (Part == L"..")
			{
				SubPath = SourceFsPath.filename();
				break;
			}
		}
	}

	SubPath.replace_extension(L".bin");
	return FPaths::ToUtf8((std::filesystem::path(L"Asset") / L"Cooked" / L"Mesh" / SubPath).generic_wstring());
}

FString FAssetPathPolicy::MakeSiblingStaticMeshBinaryPath(const FString& SourcePath)
{
	std::filesystem::path SourceFsPath(FPaths::ToWide(FPaths::Normalize(SourcePath)));
	SourceFsPath.replace_extension(L".bin");
	return FPaths::ToUtf8(SourceFsPath.generic_wstring());
}

FString FAssetPathPolicy::MakeStaticMeshCacheBinaryPath(const FString& SourcePath)
{
	std::filesystem::path SourceFsPath(FPaths::ToWide(FPaths::Normalize(SourcePath)));
	std::filesystem::path BinaryFileName = SourceFsPath.stem();
	BinaryFileName += L".bin";
	return FPaths::ToUtf8((std::filesystem::path(L"Asset") / L"Mesh" / L"Bin" / BinaryFileName).generic_wstring());
}

FString FAssetPathPolicy::MakeWritableStaticMeshCacheBinaryPath(const FString& SourcePath)
{
	const FString NormalizedSourcePath = FPaths::Normalize(SourcePath);
	std::filesystem::path SourceFsPath(FPaths::ToWide(NormalizedSourcePath));

	std::filesystem::path BinaryFileName = SourceFsPath.stem();
	BinaryFileName += ".bin";
	return MakeWritableAssetPath(std::filesystem::path(L"Asset") / L"Mesh" / L"Bin", BinaryFileName);
}

FString FAssetPathPolicy::MakeWritableSkeletalMeshCacheBinaryPath(const FString& SourcePath)
{
	// StaticMesh와 stem 이 겹칠 수 있어 SkeletalMesh 전용 루트로 분리.
	const FString NormalizedSourcePath = FPaths::Normalize(SourcePath);
	std::filesystem::path SourceFsPath(FPaths::ToWide(NormalizedSourcePath));

	std::filesystem::path BinaryFileName = SourceFsPath.stem();
	BinaryFileName += ".skmesh";
	return MakeWritableAssetPath(std::filesystem::path(L"Asset") / L"SkeletalMesh" / L"Bin", BinaryFileName);
}

FString FAssetPathPolicy::MakeWritableSkeletonCacheBinaryPath(const FString& SourcePath)
{
	const FString NormalizedSourcePath = FPaths::Normalize(SourcePath);
	std::filesystem::path SourceFsPath(FPaths::ToWide(NormalizedSourcePath));

	std::filesystem::path BinaryFileName = SourceFsPath.stem();
	BinaryFileName += ".skeleton";
	return MakeWritableAssetPath(std::filesystem::path(L"Asset") / L"Skeleton" / L"Bin", BinaryFileName);
}

FString FAssetPathPolicy::MakeWritablePhysicsAssetCacheBinaryPath(const FString& SourcePath)
{
	const FString NormalizedSourcePath = FPaths::Normalize(SourcePath);
	std::filesystem::path SourceFsPath(FPaths::ToWide(NormalizedSourcePath));

	std::filesystem::path BinaryFileName = SourceFsPath.stem();
	BinaryFileName += ".physic";
	return MakeWritableAssetPath(std::filesystem::path(L"Asset") / L"PhysicsAsset" / L"Bin", BinaryFileName);
}

FString FAssetPathPolicy::MakeWritableLegacySkeletalMeshCacheBinaryPath(const FString& SourcePath)
{
	const FString NormalizedSourcePath = FPaths::Normalize(SourcePath);
	std::filesystem::path SourceFsPath(FPaths::ToWide(NormalizedSourcePath));

	std::filesystem::path BinaryFileName = SourceFsPath.stem();
	BinaryFileName += ".bin";
	return MakeWritableAssetPath(std::filesystem::path(L"Asset") / L"SkeletalMesh" / L"Bin", BinaryFileName);
}

FString FAssetPathPolicy::MakeWritableSkeletalImportManifestPath(const FString& SourcePath)
{
	const FString NormalizedSourcePath = FPaths::Normalize(SourcePath);
	std::filesystem::path SourceFsPath(FPaths::ToWide(NormalizedSourcePath));

	std::filesystem::path ManifestFileName = SourceFsPath.stem();
	ManifestFileName += ".manifest";
	return MakeWritableAssetPath(std::filesystem::path(L"Asset") / L"ImportManifest" / L"Skeletal", ManifestFileName);
}
