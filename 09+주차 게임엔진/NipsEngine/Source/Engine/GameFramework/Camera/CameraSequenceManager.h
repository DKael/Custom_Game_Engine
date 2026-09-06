#pragma once

#include "Core/Singleton.h"
#include "Core/Containers/Array.h"
#include "Core/Containers/Map.h"
#include "Math/Vector.h"

#include <memory>

enum class ECameraSequenceInterpMode
{
    Linear,
    Bezier,
};

struct FCameraSequenceKey
{
    float Time = 0.0f;
    float Value = 0.0f;
    ECameraSequenceInterpMode InterpMode = ECameraSequenceInterpMode::Linear;
    float ArriveTime = 0.1f;
    float LeaveTime = 0.1f;
    float ArriveTangent = 0.0f;
    float LeaveTangent = 0.0f;
};

struct FCameraSequenceChannel
{
    TArray<FCameraSequenceKey> Keys;

    void SortKeys();
    float Evaluate(float TimeSeconds) const;
};

struct FCameraSequenceSample
{
    FVector Location = FVector::ZeroVector;
    FVector RotationEuler = FVector::ZeroVector;
    float FOV = 0.0f;
};

struct FCameraSequenceAsset
{
    FString Name;
    float Duration = 0.0f;
    TMap<FString, FCameraSequenceChannel> Channels;

    float GetLastKeyTime() const;
    void RecalculateDuration();
    float EvaluateChannel(const FString& ChannelName, float TimeSeconds) const;
    FCameraSequenceSample Evaluate(float TimeSeconds, float Scale = 1.0f) const;
    void SortAllChannels();
};

class FCameraSequenceManager : public TSingleton<FCameraSequenceManager>
{
    friend class TSingleton<FCameraSequenceManager>;

public:
    static constexpr const char* SequenceRoot = "Asset/Sequences";

    void RefreshSequenceList();

    const TArray<FString>& GetAvailableSequencePaths();
    const TArray<FString>& GetBuiltInChannelNames() const;

    FString ResolveSequenceIdentifier(const FString& Identifier);
    std::shared_ptr<const FCameraSequenceAsset> LoadSequence(const FString& Identifier);
    std::shared_ptr<const FCameraSequenceAsset> LoadSequenceByPath(const FString& RelativePath);
    bool TryLoadEditableSequence(const FString& Identifier, FString& OutResolvedPath, FCameraSequenceAsset& OutAsset);
    bool ReloadSequence(const FString& Identifier);
    bool SaveSequence(const FString& RelativePath, const FCameraSequenceAsset& Asset);

private:
    FCameraSequenceManager() = default;

    FString NormalizeRelativeSequencePath(const FString& RelativePath) const;
    bool IsPathLikeIdentifier(const FString& Identifier) const;
    bool LoadSequenceFromDisk(const FString& RelativePath, FCameraSequenceAsset& OutAsset) const;
    bool SaveSequenceToDisk(const FString& RelativePath, const FCameraSequenceAsset& Asset) const;

private:
    TArray<FString> AvailableSequencePaths;
    TMap<FString, FString> UniqueStemToPath;
    TArray<FString> DuplicateStems;
    TMap<FString, std::shared_ptr<FCameraSequenceAsset>> SequenceCache;
    bool bSequencePathCacheInitialized = false;
};
