#include "Editor/Animation/AnimationSequenceNotifyAssetPickerHelpers.h"

#include "Camera/CameraShakeBase.h"
#include "Core/Paths.h"
#include "Editor/Asset/EditorAssetService.h"
#include "Editor/EditorEngine.h"
#include "Engine/Runtime/Engine.h"
#include "Object/ObjectFactory.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <initializer_list>
#include <unordered_map>

namespace
{
    // Generic asset-picker support helpers
    FString ToLowerAscii(FString Value)
    {
        std::transform(
            Value.begin(),
            Value.end(),
            Value.begin(),
            [](unsigned char Ch)
            {
                return static_cast<char>(std::tolower(Ch));
            });
        return Value;
    }

    FString ToProjectRelativePath(const std::filesystem::path& AbsolutePath)
    {
        std::error_code Ec;
        std::filesystem::path Relative = std::filesystem::relative(
            AbsolutePath,
            std::filesystem::path(FPaths::RootDir()),
            Ec);
        if (Ec)
        {
            Relative = AbsolutePath.lexically_normal();
        }

        return FPaths::Normalize(FPaths::ToUtf8(Relative.generic_wstring()));
    }

    FString LowerExtension(const std::filesystem::path& Path)
    {
        return ToLowerAscii(FPaths::ToUtf8(Path.extension().wstring()));
    }

    bool ExtensionMatches(const FString& Extension, std::initializer_list<const char*> Candidates)
    {
        for (const char* Candidate : Candidates)
        {
            if (Extension == Candidate)
            {
                return true;
            }
        }

        return false;
    }

    // Asset-kind picker entry builders
    void AddSoundCueEntries(TArray<FNotifyAssetPickerEntry>& OutEntries)
    {
        // SoundCue payloads store project-relative audio asset paths. The picker
        // only provides editor-visible choices for that same runtime-facing
        // string value; it does not introduce a separate editor-only identifier.
        const std::filesystem::path AudioRoot =
            (std::filesystem::path(FPaths::RootDir()) / L"Asset" / L"Audio").lexically_normal();
        if (!std::filesystem::exists(AudioRoot))
        {
            return;
        }

        std::error_code Ec;
        for (const std::filesystem::directory_entry& Entry :
             std::filesystem::recursive_directory_iterator(AudioRoot, Ec))
        {
            if (Ec)
            {
                break;
            }

            if (!Entry.is_regular_file())
            {
                continue;
            }

            const FString Extension = LowerExtension(Entry.path());
            if (!ExtensionMatches(Extension, { ".wav", ".ogg", ".mp3", ".flac" }))
            {
                continue;
            }

            FNotifyAssetPickerEntry PickerEntry;
            PickerEntry.StoredValue = ToProjectRelativePath(Entry.path());
            PickerEntry.DisplayName = FPaths::ToUtf8(Entry.path().stem().wstring());
            OutEntries.push_back(std::move(PickerEntry));
        }
    }

    void AddVfxEntries(TArray<FNotifyAssetPickerEntry>& OutEntries)
    {
        // VFX payloads store the asset-service path string used by editor lookup
        // and runtime-facing notify dispatch. Prefer the editor display name when
        // available, but keep the stored path untouched.
        UEditorEngine* EditorEngine = Cast<UEditorEngine>(GEngine);
        if (!EditorEngine)
        {
            return;
        }

        const TArray<FEditorAssetItem>& ParticleAssets =
            EditorEngine->GetAssetService().GetAssets(EEditorAssetType::Particle);
        OutEntries.reserve(OutEntries.size() + ParticleAssets.size());
        for (const FEditorAssetItem& AssetItem : ParticleAssets)
        {
            if (AssetItem.Path.empty())
            {
                continue;
            }

            FNotifyAssetPickerEntry PickerEntry;
            PickerEntry.StoredValue = AssetItem.Path;
            PickerEntry.DisplayName =
                AssetItem.DisplayName.empty() ? AssetItem.Path : AssetItem.DisplayName;
            OutEntries.push_back(std::move(PickerEntry));
        }
    }

    void AddCameraShakeEntries(TArray<FNotifyAssetPickerEntry>& OutEntries)
    {
        // CameraShake payloads store the registered pattern class name. The
        // picker trims the leading 'U' only for display; the serialized payload
        // keeps the original class identifier intact.
        TArray<const UClass*> RegisteredClasses;
        FObjectFactory::Get().GetRegisteredClasses(RegisteredClasses);
        const UClass* BaseClass = UCameraShakePattern::StaticClass();

        OutEntries.reserve(OutEntries.size() + RegisteredClasses.size());
        for (const UClass* Class : RegisteredClasses)
        {
            if (!Class || Class == BaseClass || !Class->IsA(BaseClass) || !Class->GetName())
            {
                continue;
            }

            FNotifyAssetPickerEntry PickerEntry;
            PickerEntry.StoredValue = Class->GetName();
            PickerEntry.DisplayName = PickerEntry.StoredValue;
            if (!PickerEntry.DisplayName.empty() && PickerEntry.DisplayName.front() == 'U')
            {
                PickerEntry.DisplayName.erase(PickerEntry.DisplayName.begin());
            }

            OutEntries.push_back(std::move(PickerEntry));
        }
    }

    void FinalizeEntries(TArray<FNotifyAssetPickerEntry>& Entries)
    {
        // Sorting and duplicate-label disambiguation happen after each asset kind
        // contributes raw entries so the picker presents one stable list while
        // still preserving the original serialized value for selection.
        std::stable_sort(
            Entries.begin(),
            Entries.end(),
            [](const FNotifyAssetPickerEntry& Left, const FNotifyAssetPickerEntry& Right)
            {
                const FString LeftDisplay = ToLowerAscii(Left.DisplayName);
                const FString RightDisplay = ToLowerAscii(Right.DisplayName);
                if (LeftDisplay != RightDisplay)
                {
                    return LeftDisplay < RightDisplay;
                }

                return Left.StoredValue < Right.StoredValue;
            });

        std::unordered_map<FString, int32> DisplayCounts;
        for (const FNotifyAssetPickerEntry& Entry : Entries)
        {
            ++DisplayCounts[Entry.DisplayName];
        }

        for (FNotifyAssetPickerEntry& Entry : Entries)
        {
            if (DisplayCounts[Entry.DisplayName] > 1)
            {
                Entry.DisplayName += " (" + Entry.StoredValue + ")";
            }
        }

        Entries.erase(
            std::unique(
                Entries.begin(),
                Entries.end(),
                [](const FNotifyAssetPickerEntry& Left, const FNotifyAssetPickerEntry& Right)
                {
                    return Left.StoredValue == Right.StoredValue;
                }),
            Entries.end());
    }

    // Asset-kind display and fallback label helpers
    FString BuildSoundCueDisplayLabel(const FString& StoredValue)
    {
        // Friendly labels are editor-only sugar. The payload still stores the
        // full project-relative path even when we render just the filename stem.
        const std::filesystem::path AssetPath(FPaths::ToWide(StoredValue));
        const FString Stem = FPaths::ToUtf8(AssetPath.stem().wstring());
        return Stem.empty() ? StoredValue : Stem;
    }

    FString BuildVfxDisplayLabel(const FString& StoredValue)
    {
        return StoredValue;
    }

    FString BuildCameraShakeDisplayLabel(const FString& StoredValue)
    {
        // Camera shake classes keep the registered type name in payload text.
        // Only the leading 'U' is trimmed for a friendlier picker label.
        if (!StoredValue.empty() && StoredValue.front() == 'U')
        {
            return StoredValue.substr(1);
        }

        return StoredValue;
    }

    FString BuildNotifyAssetDisplayLabel(EAnimNotifyPayloadAssetKind AssetKind, const FString& StoredValue)
    {
        // If the stored value does not match the current picker inventory (for
        // example after a rename or a custom manual override), keep showing a
        // readable fallback label instead of hiding the serialized value.
        switch (AssetKind)
        {
        case EAnimNotifyPayloadAssetKind::SoundCue:
            return BuildSoundCueDisplayLabel(StoredValue);
        case EAnimNotifyPayloadAssetKind::CameraShake:
            return BuildCameraShakeDisplayLabel(StoredValue);
        case EAnimNotifyPayloadAssetKind::Vfx:
            return BuildVfxDisplayLabel(StoredValue);
        case EAnimNotifyPayloadAssetKind::None:
        default:
            return StoredValue;
        }
    }

}

TArray<FNotifyAssetPickerEntry> BuildNotifyAssetPickerEntries(EAnimNotifyPayloadAssetKind AssetKind)
{
    TArray<FNotifyAssetPickerEntry> Entries;
    switch (AssetKind)
    {
    case EAnimNotifyPayloadAssetKind::SoundCue:
        AddSoundCueEntries(Entries);
        break;
    case EAnimNotifyPayloadAssetKind::Vfx:
        AddVfxEntries(Entries);
        break;
    case EAnimNotifyPayloadAssetKind::CameraShake:
        AddCameraShakeEntries(Entries);
        break;
    case EAnimNotifyPayloadAssetKind::None:
    default:
        break;
    }

    FinalizeEntries(Entries);
    return Entries;
}

FString ResolveNotifyAssetPickerDisplayName(
    EAnimNotifyPayloadAssetKind AssetKind,
    const FString& StoredValue,
    const TArray<FNotifyAssetPickerEntry>& Entries)
{
    if (StoredValue.empty())
    {
        return {};
    }

    for (const FNotifyAssetPickerEntry& Entry : Entries)
    {
        if (Entry.StoredValue == StoredValue)
        {
            return Entry.DisplayName;
        }
    }

    return BuildNotifyAssetDisplayLabel(AssetKind, StoredValue);
}

const char* GetNotifyAssetPickerEmptyMessage(EAnimNotifyPayloadAssetKind AssetKind)
{
    switch (AssetKind)
    {
    case EAnimNotifyPayloadAssetKind::SoundCue:
        return "No audio assets were found under Asset/Audio.";
    case EAnimNotifyPayloadAssetKind::Vfx:
        return "No preview VFX assets are available.";
    case EAnimNotifyPayloadAssetKind::CameraShake:
        return "No registered camera shake pattern classes are available.";
    case EAnimNotifyPayloadAssetKind::None:
    default:
        return "No picker entries are available.";
    }
}

const char* GetNotifyAssetPickerManualFallbackMessage(EAnimNotifyPayloadAssetKind AssetKind)
{
    switch (AssetKind)
    {
    case EAnimNotifyPayloadAssetKind::SoundCue:
        return "Pick a project-relative sound asset path here, or use Advanced Raw Payload to enter a custom sound key.";
    case EAnimNotifyPayloadAssetKind::Vfx:
        return "Pick a VFX asset-service path here, or use Advanced Raw Payload for a manual override.";
    case EAnimNotifyPayloadAssetKind::CameraShake:
        return "Pick a registered camera shake pattern class here, or use Advanced Raw Payload for a manual override.";
    case EAnimNotifyPayloadAssetKind::None:
    default:
        return "Use Advanced Raw Payload for a manual override.";
    }
}
