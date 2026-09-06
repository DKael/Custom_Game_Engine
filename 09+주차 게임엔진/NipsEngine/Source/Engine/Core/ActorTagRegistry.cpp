#include "Core/ActorTagRegistry.h"

#include "Core/ActorTags.h"
#include "Core/Paths.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>

namespace
{
    bool ContainsTag(const TArray<FString>& Tags, const FString& Tag)
    {
        return std::find(Tags.begin(), Tags.end(), Tag) != Tags.end();
    }

    FString TrimCopy(const FString& Text)
    {
        if (Text.empty())
        {
            return FString();
        }

        size_t Begin = 0;
        size_t End = Text.size();

        while (Begin < End && std::isspace(static_cast<unsigned char>(Text[Begin])) != 0)
        {
            ++Begin;
        }

        while (End > Begin && std::isspace(static_cast<unsigned char>(Text[End - 1])) != 0)
        {
            --End;
        }

        return Text.substr(Begin, End - Begin);
    }

    TArray<FString>& GetUsedTags()
    {
        static TArray<FString> Tags;
        return Tags;
    }

    std::filesystem::path GetConfigTagFilePath()
    {
        return std::filesystem::path(FPaths::RootDir()) / L"Config" / L"ActorTags.txt";
    }

    TArray<FString>& GetMutableConfigTags()
    {
        static bool bLoaded = false;
        static TArray<FString> Tags;
        if (bLoaded)
        {
            return Tags;
        }

        bLoaded = true;

        const std::filesystem::path ConfigPath = GetConfigTagFilePath();
        if (!std::filesystem::exists(ConfigPath))
        {
            return Tags;
        }

        std::ifstream File(ConfigPath);
        if (!File.is_open())
        {
            return Tags;
        }

        FString Line;
        while (std::getline(File, Line))
        {
            const FString Trimmed = TrimCopy(Line);
            if (Trimmed.empty() || Trimmed[0] == '#')
            {
                continue;
            }

            if (!ContainsTag(Tags, Trimmed))
            {
                Tags.push_back(Trimmed);
            }
        }

        return Tags;
    }
}

namespace ActorTagRegistry
{
    const TArray<FString>& GetUsedTagsView()
    {
        return GetUsedTags();
    }

    const TArray<FString>& GetConfigTags()
    {
        return GetMutableConfigTags();
    }

    void RegisterUsedTag(const FString& InTag)
    {
        const FString Normalized = ActorTags::Normalize(TrimCopy(InTag));
        if (Normalized.empty() || Normalized == ActorTags::Untagged)
        {
            return;
        }

        TArray<FString>& UsedTags = GetUsedTags();
        if (!ContainsTag(UsedTags, Normalized))
        {
            UsedTags.push_back(Normalized);
        }
    }

    bool PromoteTagToConfig(const FString& InTag)
    {
        const FString Normalized = ActorTags::Normalize(TrimCopy(InTag));
        if (Normalized.empty() || Normalized == ActorTags::Untagged)
        {
            return false;
        }

        RegisterUsedTag(Normalized);

        TArray<FString>& ConfigTags = GetMutableConfigTags();
        if (ContainsTag(ConfigTags, Normalized))
        {
            return true;
        }

        const std::filesystem::path ConfigPath = GetConfigTagFilePath();
        std::error_code DirectoryError;
        std::filesystem::create_directories(ConfigPath.parent_path(), DirectoryError);
        if (DirectoryError)
        {
            return false;
        }

        std::ofstream File(ConfigPath, std::ios::out | std::ios::app);
        if (!File.is_open())
        {
            return false;
        }

        File << Normalized << "\n";
        if (!File.good())
        {
            return false;
        }

        ConfigTags.push_back(Normalized);
        return true;
    }

    const TArray<FString>& GetProjectTags()
    {
        static TArray<FString> Cached;
        Cached = ActorTags::GetBuiltinTags();

        for (const FString& Tag : GetConfigTags())
        {
            if (!ContainsTag(Cached, Tag))
            {
                Cached.push_back(Tag);
            }
        }

        for (const FString& Tag : GetUsedTagsView())
        {
            if (!ContainsTag(Cached, Tag))
            {
                Cached.push_back(Tag);
            }
        }

        return Cached;
    }
}
