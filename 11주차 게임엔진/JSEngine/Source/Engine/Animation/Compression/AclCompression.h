#pragma once

#include "Core/CoreMinimal.h"

#include <acl/core/ansi_allocator.h>
#include <acl/core/compressed_tracks.h>
#include <acl/core/iallocator.h>

class UAnimSequence;

struct FAclLibraryInfo
{
    int32 Major = 0;
    int32 Minor = 0;
    int32 Patch = 0;
};

struct FAclCompressedClipInfo
{
    uint32 SizeBytes = 0;
    uint32 Hash = 0;
    uint32 TrackCount = 0;
    uint32 SamplesPerTrack = 0;
    float SampleRate = 0.0f;
    float Duration = 0.0f;
};

struct FAclCompressionBuildResult
{
    bool bSuccess = false;
    FString ErrorMessage;
    uint32 RawSizeBytes = 0;
    uint32 CompressedSizeBytes = 0;
    uint32 TrackCount = 0;
    uint32 SamplesPerTrack = 0;
    float SampleRate = 0.0f;
    float Duration = 0.0f;
};

class FAclAnimSequenceRuntimeData
{
public:
    FAclAnimSequenceRuntimeData() = default;
    ~FAclAnimSequenceRuntimeData();

    FAclAnimSequenceRuntimeData(const FAclAnimSequenceRuntimeData&) = delete;
    FAclAnimSequenceRuntimeData& operator=(const FAclAnimSequenceRuntimeData&) = delete;

    acl::iallocator& GetAllocator();

    bool TakeCompressedTracks(acl::compressed_tracks* InCompressedTracks);
    bool CopyCompressedTracks(const void* Data, uint32 SizeBytes);
    void Reset();

    bool IsValid() const;
    const FAclCompressedClipInfo& GetInfo() const;
    const acl::compressed_tracks* GetCompressedTracks() const;
    acl::compressed_tracks* GetMutableCompressedTracks();
    const uint8* GetCompressedData() const;
    uint32 GetCompressedSizeBytes() const;

private:
    bool RefreshInfo();

    acl::ansi_allocator Allocator;
    acl::compressed_tracks* CompressedTracks = nullptr;
    FAclCompressedClipInfo Info;
};

class FAclCompression
{
public:
    static FAclLibraryInfo GetLibraryInfo();
    static FAclCompressionBuildResult BuildRuntimeDataFromRawSequence(UAnimSequence* Sequence);
    static bool DecompressPoseToTrackTransforms(const UAnimSequence* Sequence, float SampleTime, TArray<FTransform>& OutTrackTransforms);
};
