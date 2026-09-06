#include "GameFramework/Camera/CameraSequenceManager.h"

#include "Core/Paths.h"
#include "Math/Utils.h"
#include "SimpleJSON/json.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <unordered_set>

namespace
{
    bool TryGetJsonNumber(const json::JSON& Node, float& OutValue)
    {
        bool bOk = false;
        OutValue = static_cast<float>(Node.ToFloat(bOk));
        if (bOk)
        {
            return true;
        }

        OutValue = static_cast<float>(Node.ToInt(bOk));
        return bOk;
    }

    float HermiteInterpolate(float P0, float T0, float P1, float T1, float Alpha, float DeltaTime)
    {
        const float Alpha2 = Alpha * Alpha;
        const float Alpha3 = Alpha2 * Alpha;

        const float H00 = 2.0f * Alpha3 - 3.0f * Alpha2 + 1.0f;
        const float H10 = Alpha3 - 2.0f * Alpha2 + Alpha;
        const float H01 = -2.0f * Alpha3 + 3.0f * Alpha2;
        const float H11 = Alpha3 - Alpha2;

        return H00 * P0 +
            H10 * T0 * DeltaTime +
            H01 * P1 +
            H11 * T1 * DeltaTime;
    }

    float CubicBezier1D(float P0, float P1, float P2, float P3, float T)
    {
        const float OneMinusT = 1.0f - T;
        const float OneMinusT2 = OneMinusT * OneMinusT;
        const float OneMinusT3 = OneMinusT2 * OneMinusT;
        const float T2 = T * T;
        const float T3 = T2 * T;

        return OneMinusT3 * P0 +
            3.0f * OneMinusT2 * T * P1 +
            3.0f * OneMinusT * T2 * P2 +
            T3 * P3;
    }

    float SolveBezierParameterForTime(float TargetTime, float T0, float T1, float T2, float T3)
    {
        float Low = 0.0f;
        float High = 1.0f;
        for (int32 Iteration = 0; Iteration < 18; ++Iteration)
        {
            const float Mid = (Low + High) * 0.5f;
            const float TimeAtMid = CubicBezier1D(T0, T1, T2, T3, Mid);
            if (TimeAtMid < TargetTime)
            {
                Low = Mid;
            }
            else
            {
                High = Mid;
            }
        }

        return (Low + High) * 0.5f;
    }

    FString ToLowerCopy(FString Value)
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

    ECameraSequenceInterpMode ParseInterpMode(const FString& Value)
    {
        const FString Lower = ToLowerCopy(Value);
        if (Lower == "bezier")
        {
            return ECameraSequenceInterpMode::Bezier;
        }

        return ECameraSequenceInterpMode::Linear;
    }

    const char* ToInterpModeString(ECameraSequenceInterpMode Mode)
    {
        switch (Mode)
        {
        case ECameraSequenceInterpMode::Bezier:
            return "Bezier";
        case ECameraSequenceInterpMode::Linear:
        default:
            return "Linear";
        }
    }

    const TArray<FString>& GetBuiltInChannelNamesInternal()
    {
        static const TArray<FString> ChannelNames = {
            "Location.X",
            "Location.Y",
            "Location.Z",
            "Rotation.X",
            "Rotation.Y",
            "Rotation.Z",
            "FOV",
        };
        return ChannelNames;
    }
}

void FCameraSequenceChannel::SortKeys()
{
    std::sort(
        Keys.begin(),
        Keys.end(),
        [](const FCameraSequenceKey& A, const FCameraSequenceKey& B)
        {
            return A.Time < B.Time;
        });
}

float FCameraSequenceChannel::Evaluate(float TimeSeconds) const
{
    if (Keys.empty())
    {
        return 0.0f;
    }

    if (TimeSeconds <= Keys.front().Time)
    {
        return Keys.front().Value;
    }

    if (TimeSeconds >= Keys.back().Time)
    {
        return Keys.back().Value;
    }

    auto UpperIt = std::upper_bound(
        Keys.begin(),
        Keys.end(),
        TimeSeconds,
        [](float InTime, const FCameraSequenceKey& Key)
        {
            return InTime < Key.Time;
        });
    if (UpperIt == Keys.begin() || UpperIt == Keys.end())
    {
        return UpperIt == Keys.end() ? Keys.back().Value : UpperIt->Value;
    }

    const FCameraSequenceKey& Upper = *UpperIt;
    const FCameraSequenceKey& Lower = *(UpperIt - 1);
    const float DeltaTime = Upper.Time - Lower.Time;
    if (DeltaTime <= MathUtil::SmallNumber)
    {
        return Upper.Value;
    }

    const float Alpha = MathUtil::Clamp((TimeSeconds - Lower.Time) / DeltaTime, 0.0f, 1.0f);
    if (Lower.InterpMode == ECameraSequenceInterpMode::Bezier)
    {
        const float LeaveTime = MathUtil::Clamp(Lower.LeaveTime, MathUtil::SmallNumber, DeltaTime - MathUtil::SmallNumber);
        const float ArriveTime = MathUtil::Clamp(Upper.ArriveTime, MathUtil::SmallNumber, DeltaTime - MathUtil::SmallNumber);
        float Control1Time = Lower.Time + LeaveTime;
        float Control2Time = Upper.Time - ArriveTime;
        if (Control2Time <= Control1Time)
        {
            const float MidTime = (Lower.Time + Upper.Time) * 0.5f;
            Control1Time = std::min(Control1Time, MidTime - MathUtil::SmallNumber);
            Control2Time = std::max(Control2Time, MidTime + MathUtil::SmallNumber);
        }

        const float BezierT = SolveBezierParameterForTime(TimeSeconds, Lower.Time, Control1Time, Control2Time, Upper.Time);
        return CubicBezier1D(
            Lower.Value,
            Lower.Value + Lower.LeaveTangent,
            Upper.Value - Upper.ArriveTangent,
            Upper.Value,
            BezierT);
    }

    return Lower.Value + (Upper.Value - Lower.Value) * Alpha;
}

float FCameraSequenceAsset::EvaluateChannel(const FString& ChannelName, float TimeSeconds) const
{
    auto It = Channels.find(ChannelName);
    if (It == Channels.end())
    {
        return 0.0f;
    }

    return It->second.Evaluate(TimeSeconds);
}

float FCameraSequenceAsset::GetLastKeyTime() const
{
    float LastKeyTime = 0.0f;
    for (const auto& Pair : Channels)
    {
        if (!Pair.second.Keys.empty())
        {
            LastKeyTime = (std::max)(LastKeyTime, Pair.second.Keys.back().Time);
        }
    }
    return LastKeyTime;
}

void FCameraSequenceAsset::RecalculateDuration()
{
    Duration = GetLastKeyTime();
}

FCameraSequenceSample FCameraSequenceAsset::Evaluate(float TimeSeconds, float Scale) const
{
    FCameraSequenceSample Sample;
    Sample.Location.X = EvaluateChannel("Location.X", TimeSeconds) * Scale;
    Sample.Location.Y = EvaluateChannel("Location.Y", TimeSeconds) * Scale;
    Sample.Location.Z = EvaluateChannel("Location.Z", TimeSeconds) * Scale;
    Sample.RotationEuler.X = EvaluateChannel("Rotation.X", TimeSeconds) * Scale;
    Sample.RotationEuler.Y = EvaluateChannel("Rotation.Y", TimeSeconds) * Scale;
    Sample.RotationEuler.Z = EvaluateChannel("Rotation.Z", TimeSeconds) * Scale;
    Sample.FOV = EvaluateChannel("FOV", TimeSeconds) * Scale;
    return Sample;
}

void FCameraSequenceAsset::SortAllChannels()
{
    for (auto& Pair : Channels)
    {
        Pair.second.SortKeys();
    }
    RecalculateDuration();
}

void FCameraSequenceManager::RefreshSequenceList()
{
    AvailableSequencePaths.clear();
    UniqueStemToPath.clear();
    DuplicateStems.clear();
    bSequencePathCacheInitialized = true;

    const std::filesystem::path RootPath(FPaths::ToAbsolute(FPaths::ToWide(SequenceRoot)));
    std::error_code Ec;
    std::filesystem::create_directories(RootPath, Ec);
    if (Ec || !std::filesystem::exists(RootPath, Ec))
    {
        return;
    }

    TMap<FString, int32> StemCounts;
    for (auto It = std::filesystem::recursive_directory_iterator(RootPath, Ec);
        !Ec && It != std::filesystem::recursive_directory_iterator();
        It.increment(Ec))
    {
        if (!It->is_regular_file(Ec))
        {
            continue;
        }

        FString Extension = ToLowerCopy(FPaths::ToUtf8(It->path().extension().generic_wstring()));
        if (Extension != ".json")
        {
            continue;
        }

        const FString RelativePath = NormalizeRelativeSequencePath(
            FPaths::ToRelativeString(It->path().generic_wstring()));
        AvailableSequencePaths.push_back(RelativePath);

        const FString Stem = FPaths::ToUtf8(It->path().stem().generic_wstring());
        ++StemCounts[Stem];
        if (StemCounts[Stem] == 1)
        {
            UniqueStemToPath[Stem] = RelativePath;
        }
    }

    std::sort(AvailableSequencePaths.begin(), AvailableSequencePaths.end());
    AvailableSequencePaths.erase(
        std::unique(AvailableSequencePaths.begin(), AvailableSequencePaths.end()),
        AvailableSequencePaths.end());

    for (const auto& Pair : StemCounts)
    {
        if (Pair.second > 1)
        {
            DuplicateStems.push_back(Pair.first);
            UniqueStemToPath.erase(Pair.first);
        }
    }

    std::sort(DuplicateStems.begin(), DuplicateStems.end());
}

const TArray<FString>& FCameraSequenceManager::GetAvailableSequencePaths()
{
    if (!bSequencePathCacheInitialized)
    {
        RefreshSequenceList();
    }

    return AvailableSequencePaths;
}

const TArray<FString>& FCameraSequenceManager::GetBuiltInChannelNames() const
{
    return GetBuiltInChannelNamesInternal();
}

FString FCameraSequenceManager::ResolveSequenceIdentifier(const FString& Identifier)
{
    if (Identifier.empty())
    {
        return FString();
    }

    GetAvailableSequencePaths();

    if (IsPathLikeIdentifier(Identifier))
    {
        const FString Normalized = NormalizeRelativeSequencePath(Identifier);
        const auto It = std::find(AvailableSequencePaths.begin(), AvailableSequencePaths.end(), Normalized);
        if (It == AvailableSequencePaths.end())
        {
            UE_LOG("[CameraSequence] Sequence path not found: %s\n", Normalized.c_str());
            return FString();
        }
        return Normalized;
    }

    if (std::find(DuplicateStems.begin(), DuplicateStems.end(), Identifier) != DuplicateStems.end())
    {
        UE_LOG("[CameraSequence] Duplicate sequence stem '%s'. Use a full relative path instead.\n", Identifier.c_str());
        return FString();
    }

    auto It = UniqueStemToPath.find(Identifier);
    if (It == UniqueStemToPath.end())
    {
        UE_LOG("[CameraSequence] Sequence stem not found: %s\n", Identifier.c_str());
        return FString();
    }

    return It->second;
}

std::shared_ptr<const FCameraSequenceAsset> FCameraSequenceManager::LoadSequence(const FString& Identifier)
{
    const FString ResolvedPath = ResolveSequenceIdentifier(Identifier);
    if (ResolvedPath.empty())
    {
        return nullptr;
    }

    return LoadSequenceByPath(ResolvedPath);
}

std::shared_ptr<const FCameraSequenceAsset> FCameraSequenceManager::LoadSequenceByPath(const FString& RelativePath)
{
    const FString NormalizedPath = NormalizeRelativeSequencePath(RelativePath);
    if (NormalizedPath.empty())
    {
        return nullptr;
    }

    auto CachedIt = SequenceCache.find(NormalizedPath);
    if (CachedIt != SequenceCache.end() && CachedIt->second)
    {
        return CachedIt->second;
    }

    auto LoadedAsset = std::make_shared<FCameraSequenceAsset>();
    if (!LoadSequenceFromDisk(NormalizedPath, *LoadedAsset))
    {
        return nullptr;
    }

    SequenceCache[NormalizedPath] = LoadedAsset;
    return LoadedAsset;
}

bool FCameraSequenceManager::TryLoadEditableSequence(const FString& Identifier, FString& OutResolvedPath, FCameraSequenceAsset& OutAsset)
{
    OutResolvedPath = ResolveSequenceIdentifier(Identifier);
    if (OutResolvedPath.empty())
    {
        return false;
    }

    return LoadSequenceFromDisk(OutResolvedPath, OutAsset);
}

bool FCameraSequenceManager::ReloadSequence(const FString& Identifier)
{
    const FString ResolvedPath = ResolveSequenceIdentifier(Identifier);
    if (ResolvedPath.empty())
    {
        return false;
    }

    SequenceCache.erase(ResolvedPath);
    return LoadSequenceByPath(ResolvedPath) != nullptr;
}

bool FCameraSequenceManager::SaveSequence(const FString& RelativePath, const FCameraSequenceAsset& Asset)
{
    const FString NormalizedPath = NormalizeRelativeSequencePath(RelativePath);
    if (NormalizedPath.empty())
    {
        return false;
    }

    if (!SaveSequenceToDisk(NormalizedPath, Asset))
    {
        return false;
    }

    auto CachedAsset = std::make_shared<FCameraSequenceAsset>(Asset);
    CachedAsset->SortAllChannels();
    SequenceCache[NormalizedPath] = CachedAsset;
    RefreshSequenceList();
    return true;
}

FString FCameraSequenceManager::NormalizeRelativeSequencePath(const FString& RelativePath) const
{
    if (RelativePath.empty())
    {
        return FString();
    }

    FString Normalized = FPaths::Normalize(RelativePath);
    std::replace(Normalized.begin(), Normalized.end(), '\\', '/');
    return Normalized;
}

bool FCameraSequenceManager::IsPathLikeIdentifier(const FString& Identifier) const
{
    const FString Lower = ToLowerCopy(Identifier);
    return Identifier.find('/') != FString::npos ||
        Identifier.find('\\') != FString::npos ||
        (Lower.size() >= 5 && Lower.rfind(".json") == Lower.size() - 5);
}

bool FCameraSequenceManager::LoadSequenceFromDisk(const FString& RelativePath, FCameraSequenceAsset& OutAsset) const
{
    const std::filesystem::path AbsolutePath(FPaths::ToAbsolute(FPaths::ToWide(RelativePath)));
    std::ifstream File(AbsolutePath, std::ios::in | std::ios::binary);
    if (!File.is_open())
    {
        UE_LOG("[CameraSequence] Failed to open sequence file: %s\n", RelativePath.c_str());
        return false;
    }

    FString FileContent((std::istreambuf_iterator<char>(File)), std::istreambuf_iterator<char>());
    if (FileContent.empty())
    {
        UE_LOG("[CameraSequence] Sequence file is empty: %s\n", RelativePath.c_str());
        return false;
    }

    json::JSON Root = json::JSON::Load(FileContent);
    if (Root.JSONType() != json::JSON::Class::Object)
    {
        UE_LOG("[CameraSequence] Sequence root must be an object: %s\n", RelativePath.c_str());
        return false;
    }

    FCameraSequenceAsset LoadedAsset;
    if (Root.hasKey("name"))
    {
        bool bOk = false;
        LoadedAsset.Name = Root.at("name").ToString(bOk);
        if (!bOk)
        {
            LoadedAsset.Name.clear();
        }
    }

    if (Root.hasKey("channels"))
    {
        const json::JSON& ChannelsNode = Root.at("channels");
        if (ChannelsNode.JSONType() == json::JSON::Class::Object)
        {
            for (const auto& ChannelPair : ChannelsNode.ObjectRange())
            {
                const FString ChannelName = ChannelPair.first;
                const json::JSON& ChannelArray = ChannelPair.second;
                if (ChannelArray.JSONType() != json::JSON::Class::Array)
                {
                    continue;
                }

                FCameraSequenceChannel Channel;
                for (const auto& KeyNode : ChannelArray.ArrayRange())
                {
                    if (KeyNode.JSONType() != json::JSON::Class::Object)
                    {
                        continue;
                    }

                    FCameraSequenceKey Key;
                    if (KeyNode.hasKey("time"))
                    {
                        TryGetJsonNumber(KeyNode.at("time"), Key.Time);
                    }
                    if (KeyNode.hasKey("value"))
                    {
                        TryGetJsonNumber(KeyNode.at("value"), Key.Value);
                    }
                    if (KeyNode.hasKey("interp"))
                    {
                        bool bOk = false;
                        Key.InterpMode = ParseInterpMode(KeyNode.at("interp").ToString(bOk));
                    }
                    if (KeyNode.hasKey("arriveTime"))
                    {
                        TryGetJsonNumber(KeyNode.at("arriveTime"), Key.ArriveTime);
                    }
                    if (KeyNode.hasKey("leaveTime"))
                    {
                        TryGetJsonNumber(KeyNode.at("leaveTime"), Key.LeaveTime);
                    }
                    if (KeyNode.hasKey("arriveTangent"))
                    {
                        TryGetJsonNumber(KeyNode.at("arriveTangent"), Key.ArriveTangent);
                    }
                    if (KeyNode.hasKey("leaveTangent"))
                    {
                        TryGetJsonNumber(KeyNode.at("leaveTangent"), Key.LeaveTangent);
                    }

                    Channel.Keys.push_back(Key);
                }

                Channel.SortKeys();
                LoadedAsset.Channels[ChannelName] = Channel;
            }
        }
    }

    if (LoadedAsset.Name.empty())
    {
        LoadedAsset.Name = FPaths::ToUtf8(AbsolutePath.stem().generic_wstring());
    }

    LoadedAsset.SortAllChannels();
    if (Root.hasKey("duration"))
    {
        float LegacyDuration = 0.0f;
        TryGetJsonNumber(Root.at("duration"), LegacyDuration);
    }
    OutAsset = LoadedAsset;
    return true;
}

bool FCameraSequenceManager::SaveSequenceToDisk(const FString& RelativePath, const FCameraSequenceAsset& Asset) const
{
    FCameraSequenceAsset SortedAsset = Asset;
    SortedAsset.SortAllChannels();
    SortedAsset.RecalculateDuration();

    json::JSON Root = json::Object();
    Root["name"] = SortedAsset.Name;
    Root["duration"] = SortedAsset.Duration;

    json::JSON ChannelsNode = json::Object();
    const TArray<FString>& BuiltInChannels = GetBuiltInChannelNamesInternal();
    std::unordered_set<FString> WrittenChannels;
    for (const FString& BuiltInChannel : BuiltInChannels)
    {
        auto It = SortedAsset.Channels.find(BuiltInChannel);
        if (It == SortedAsset.Channels.end())
        {
            continue;
        }

        json::JSON ChannelArray = json::Array();
        for (const FCameraSequenceKey& Key : It->second.Keys)
        {
            json::JSON KeyNode = json::Object();
            KeyNode["time"] = Key.Time;
            KeyNode["value"] = Key.Value;
            KeyNode["interp"] = ToInterpModeString(Key.InterpMode);
            if (Key.InterpMode == ECameraSequenceInterpMode::Bezier)
            {
                KeyNode["arriveTime"] = Key.ArriveTime;
                KeyNode["leaveTime"] = Key.LeaveTime;
                KeyNode["arriveTangent"] = Key.ArriveTangent;
                KeyNode["leaveTangent"] = Key.LeaveTangent;
            }
            ChannelArray.append(KeyNode);
        }

        ChannelsNode[BuiltInChannel] = ChannelArray;
        WrittenChannels.insert(BuiltInChannel);
    }

    for (const auto& Pair : SortedAsset.Channels)
    {
        if (WrittenChannels.find(Pair.first) != WrittenChannels.end())
        {
            continue;
        }

        json::JSON ChannelArray = json::Array();
        for (const FCameraSequenceKey& Key : Pair.second.Keys)
        {
            json::JSON KeyNode = json::Object();
            KeyNode["time"] = Key.Time;
            KeyNode["value"] = Key.Value;
            KeyNode["interp"] = ToInterpModeString(Key.InterpMode);
            if (Key.InterpMode == ECameraSequenceInterpMode::Bezier)
            {
                KeyNode["arriveTime"] = Key.ArriveTime;
                KeyNode["leaveTime"] = Key.LeaveTime;
                KeyNode["arriveTangent"] = Key.ArriveTangent;
                KeyNode["leaveTangent"] = Key.LeaveTangent;
            }
            ChannelArray.append(KeyNode);
        }

        ChannelsNode[Pair.first] = ChannelArray;
    }

    Root["channels"] = ChannelsNode;

    const std::filesystem::path AbsolutePath(FPaths::ToAbsolute(FPaths::ToWide(RelativePath)));
    std::error_code Ec;
    std::filesystem::create_directories(AbsolutePath.parent_path(), Ec);
    if (Ec)
    {
        UE_LOG("[CameraSequence] Failed to create sequence directory: %s\n", RelativePath.c_str());
        return false;
    }

    std::ofstream File(AbsolutePath, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!File.is_open())
    {
        UE_LOG("[CameraSequence] Failed to write sequence file: %s\n", RelativePath.c_str());
        return false;
    }

    File << Root.dump();
    return File.good();
}
