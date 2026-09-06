#include "Editor/Animation/AnimationSequencePreviewMeshResolver.h"

#include "Animation/AnimData/AnimSequence.h"
#include "Asset/SkeletalMesh.h"
#include "Core/Paths.h"
#include "Core/ResourceManager.h"

#include <algorithm>
#include <cctype>
#include <filesystem>

namespace
{
    FString NormalizeProjectAssetPath(const FString& Path)
    {
        const FString NormalizedPath = FPaths::Normalize(Path);
        if (NormalizedPath.empty())
        {
            return {};
        }

        const std::filesystem::path FsPath(FPaths::ToWide(NormalizedPath));
        if (!FsPath.is_absolute())
        {
            return NormalizedPath;
        }

        const std::filesystem::path RootPath(FPaths::RootDir());
        std::error_code ErrorCode;
        const std::filesystem::path RelativePath = std::filesystem::relative(FsPath, RootPath, ErrorCode);
        if (ErrorCode || RelativePath.empty())
        {
            return NormalizedPath;
        }

        for (const std::filesystem::path& Part : RelativePath)
        {
            if (Part == L"..")
            {
                return NormalizedPath;
            }
        }

        return FPaths::Normalize(FPaths::ToUtf8(RelativePath.generic_wstring()));
    }

    FString GetMeshStemFromSequencePath(const FString& SequencePath)
    {
        if (SequencePath.empty())
        {
            return {};
        }

        const std::filesystem::path SequenceFsPath(FPaths::ToWide(SequencePath));
        const std::filesystem::path ParentPath = SequenceFsPath.parent_path();
        return ParentPath.empty() ? FString() : FPaths::ToString(ParentPath.filename().generic_wstring());
    }

    FString GetAssetStem(const FString& AssetPath)
    {
        if (AssetPath.empty())
        {
            return {};
        }

        const std::filesystem::path AssetFsPath(FPaths::ToWide(FPaths::Normalize(AssetPath)));
        return FPaths::ToString(AssetFsPath.stem().generic_wstring());
    }

    FString GetLowerExtension(const FString& AssetPath)
    {
        if (AssetPath.empty())
        {
            return {};
        }

        FString Extension = FPaths::ToString(
            std::filesystem::path(FPaths::ToWide(FPaths::Normalize(AssetPath))).extension().generic_wstring());
        std::transform(
            Extension.begin(),
            Extension.end(),
            Extension.begin(),
            [](unsigned char Character)
            {
                return static_cast<char>(std::tolower(Character));
            });
        return Extension;
    }

    bool TryBuildMeshPathFromSkeletonPath(
        const FString& SkeletonAssetPath,
        const TArray<FString>& SkeletalMeshPaths,
        FString& OutMeshPath)
    {
        if (SkeletonAssetPath.empty())
        {
            return false;
        }

        std::filesystem::path MeshFsPath(FPaths::ToWide(FPaths::Normalize(SkeletonAssetPath)));
        if (MeshFsPath.empty())
        {
            return false;
        }

        MeshFsPath.replace_extension(L".skmesh");
        const std::filesystem::path BinDir = MeshFsPath.parent_path();
        const std::filesystem::path AssetTypeDir = BinDir.parent_path();
        if (!AssetTypeDir.empty())
        {
            MeshFsPath = AssetTypeDir.parent_path() / L"SkeletalMesh" / BinDir.filename() / MeshFsPath.filename();
        }

        const FString ExpectedMeshPath = FPaths::Normalize(FPaths::ToUtf8(MeshFsPath.generic_wstring()));
        const FString ExpectedRelativeMeshPath = NormalizeProjectAssetPath(ExpectedMeshPath);
        for (const FString& CandidatePath : SkeletalMeshPaths)
        {
            const FString NormalizedCandidatePath = NormalizeProjectAssetPath(CandidatePath);
            if (NormalizedCandidatePath == ExpectedMeshPath ||
                NormalizedCandidatePath == ExpectedRelativeMeshPath)
            {
                OutMeshPath = CandidatePath;
                return true;
            }
        }

        return false;
    }
}

bool FAnimationSequencePreviewMeshResolver::Resolve(
    const FString& SequencePath,
    const UAnimSequence* Sequence,
    FString& OutMeshPath) const
{
    OutMeshPath.clear();

    const TArray<FString> SkeletalMeshPaths = FResourceManager::Get().GetSkeletalMeshPaths();
    const FString SkeletonAssetPath = Sequence
        ? NormalizeProjectAssetPath(Sequence->GetSkeletonAssetPath())
        : FString();
    if (!SkeletonAssetPath.empty())
    {
        if (TryBuildMeshPathFromSkeletonPath(SkeletonAssetPath, SkeletalMeshPaths, OutMeshPath))
        {
            return true;
        }

        // Keep the existing fallback policy: prefer skeleton-matched binary
        // meshes first, then widen to the full discovered mesh list, and only
        // fall back to the sequence-folder stem heuristic when metadata fails.
        const FString SkeletonStem = GetAssetStem(SkeletonAssetPath);
        TArray<FString> CandidatePaths;
        CandidatePaths.reserve(SkeletalMeshPaths.size());
        for (const FString& CandidatePath : SkeletalMeshPaths)
        {
            if (SkeletonStem.empty() || GetAssetStem(CandidatePath) == SkeletonStem)
            {
                CandidatePaths.push_back(CandidatePath);
            }
        }

        std::stable_sort(
            CandidatePaths.begin(),
            CandidatePaths.end(),
            [](const FString& Left, const FString& Right)
            {
                const bool bLeftIsBinary = GetLowerExtension(Left) == ".skmesh";
                const bool bRightIsBinary = GetLowerExtension(Right) == ".skmesh";
                if (bLeftIsBinary != bRightIsBinary)
                {
                    return bLeftIsBinary;
                }

                const bool bLeftIsFbx = GetLowerExtension(Left) == ".fbx";
                const bool bRightIsFbx = GetLowerExtension(Right) == ".fbx";
                if (bLeftIsFbx != bRightIsFbx)
                {
                    return !bLeftIsFbx;
                }

                return Left < Right;
            });

        auto TryResolveFromCandidates = [&](const TArray<FString>& Paths) -> bool
        {
            for (const FString& CandidatePath : Paths)
            {
                USkeletalMesh* CandidateMesh = FResourceManager::Get().FindSkeletalMesh(CandidatePath);
                if (!CandidateMesh)
                {
                    CandidateMesh = FResourceManager::Get().LoadSkeletalMesh(CandidatePath);
                }

                if (!CandidateMesh)
                {
                    continue;
                }

                if (NormalizeProjectAssetPath(CandidateMesh->GetSkeletonAssetPath()) == SkeletonAssetPath)
                {
                    OutMeshPath = CandidatePath;
                    return true;
                }
            }

            return false;
        };

        if (TryResolveFromCandidates(CandidatePaths))
        {
            return true;
        }

        if (CandidatePaths.size() != SkeletalMeshPaths.size() && TryResolveFromCandidates(SkeletalMeshPaths))
        {
            return true;
        }
    }

    const FString MeshStem = GetMeshStemFromSequencePath(SequencePath);
    if (MeshStem.empty())
    {
        return false;
    }

    for (const FString& CandidatePath : SkeletalMeshPaths)
    {
        if (GetAssetStem(CandidatePath) == MeshStem)
        {
            OutMeshPath = CandidatePath;
            return true;
        }
    }

    return false;
}
