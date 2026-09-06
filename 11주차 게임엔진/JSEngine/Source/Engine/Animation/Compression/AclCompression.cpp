#include "Animation/Compression/AclCompression.h"

#include "Animation/AnimData/AnimDataModel.h"
#include "Animation/AnimData/AnimSequence.h"
#include "Asset/Skeleton.h"

#include <acl/core/error_result.h>
#include <acl/compression/compress.h>
#include <acl/compression/output_stats.h>
#include <acl/compression/track_array.h>
#include <acl/compression/transform_error_metrics.h>
#include <acl/decompression/decompress.h>
#include <acl/decompression/decompression_settings.h>
#include <acl/version.h>
#include <rtm/quatf.h>
#include <rtm/qvvf.h>
#include <rtm/vector4f.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <memory>
#include <utility>

static_assert(ACL_VERSION_MAJOR == 2, "Unexpected ACL major version");
static_assert(ACL_VERSION_MINOR == 1, "Unexpected ACL minor version");
static_assert(ACL_VERSION_PATCH == 0, "Unexpected ACL patch version");

namespace
{
    constexpr uint32 ACL_INVALID_TRACK_INDEX = std::numeric_limits<uint32>::max();

    uint32 ToAclParentTrackIndex(const UAnimSequence* Sequence, const TArray<int32>& BoneToTrackIndex, const FBoneAnimationTrack& Track)
    {
        if (!Sequence || !Sequence->GetSkeleton())
        {
            return ACL_INVALID_TRACK_INDEX;
        }

        const TArray<FSkeletonBone>& Bones = Sequence->GetSkeleton()->GetBones();
        if (Track.BoneTreeIndex < 0 || Track.BoneTreeIndex >= static_cast<int32>(Bones.size()))
        {
            return ACL_INVALID_TRACK_INDEX;
        }

        const int32 ParentBoneIndex = Bones[Track.BoneTreeIndex].ParentIndex;
        if (ParentBoneIndex < 0 || ParentBoneIndex >= static_cast<int32>(BoneToTrackIndex.size()))
        {
            return ACL_INVALID_TRACK_INDEX;
        }

        const int32 ParentTrackIndex = BoneToTrackIndex[ParentBoneIndex];
        return ParentTrackIndex >= 0
            ? static_cast<uint32>(ParentTrackIndex)
            : ACL_INVALID_TRACK_INDEX;
    }

    rtm::qvvf MakeAclTransformSample(const FVector& Translation, const FQuat& Rotation, const FVector& Scale)
    {
        return rtm::qvv_set(
            rtm::quat_normalize(rtm::quat_set(Rotation.X, Rotation.Y, Rotation.Z, Rotation.W)),
            rtm::vector_set(Translation.X, Translation.Y, Translation.Z, 0.0f),
            rtm::vector_set(Scale.X, Scale.Y, Scale.Z, 1.0f));
    }

    FVector FromAclVector(rtm::vector4f_arg0 Value)
    {
        return FVector(
            static_cast<float>(rtm::vector_get_x(Value)),
            static_cast<float>(rtm::vector_get_y(Value)),
            static_cast<float>(rtm::vector_get_z(Value)));
    }

    FQuat FromAclQuat(rtm::quatf_arg0 Value)
    {
        FQuat Rotation(
            static_cast<float>(rtm::quat_get_x(Value)),
            static_cast<float>(rtm::quat_get_y(Value)),
            static_cast<float>(rtm::quat_get_z(Value)),
            static_cast<float>(rtm::quat_get_w(Value)));
        Rotation.Normalize();
        return Rotation;
    }

    class FAclPoseTrackWriter final : public acl::track_writer
    {
    public:
        explicit FAclPoseTrackWriter(TArray<FTransform>& InTrackTransforms)
            : TrackTransforms(InTrackTransforms)
        {
        }

        void RTM_SIMD_CALL write_rotation(uint32_t TrackIndex, rtm::quatf_arg0 Rotation)
        {
            if (TrackIndex < TrackTransforms.size())
            {
                TrackTransforms[TrackIndex].SetRotation(FromAclQuat(Rotation));
            }
        }

        void RTM_SIMD_CALL write_translation(uint32_t TrackIndex, rtm::vector4f_arg0 Translation)
        {
            if (TrackIndex < TrackTransforms.size())
            {
                TrackTransforms[TrackIndex].SetTranslation(FromAclVector(Translation));
            }
        }

        void RTM_SIMD_CALL write_scale(uint32_t TrackIndex, rtm::vector4f_arg0 Scale)
        {
            if (TrackIndex < TrackTransforms.size())
            {
                TrackTransforms[TrackIndex].SetScale3D(FromAclVector(Scale));
            }
        }

    private:
        TArray<FTransform>& TrackTransforms;
    };
}

FAclAnimSequenceRuntimeData::~FAclAnimSequenceRuntimeData()
{
    Reset();
}

acl::iallocator& FAclAnimSequenceRuntimeData::GetAllocator()
{
    return Allocator;
}

bool FAclAnimSequenceRuntimeData::TakeCompressedTracks(acl::compressed_tracks* InCompressedTracks)
{
    Reset();

    if (!InCompressedTracks)
    {
        return true;
    }

    const acl::error_result ValidationResult = InCompressedTracks->is_valid(false);
    if (ValidationResult.any())
    {
        UE_LOG_ERROR("[AclCompression] Invalid compressed tracks: %s", ValidationResult.c_str());
        Allocator.deallocate(InCompressedTracks, InCompressedTracks->get_size());
        return false;
    }

    CompressedTracks = InCompressedTracks;
    return RefreshInfo();
}

bool FAclAnimSequenceRuntimeData::CopyCompressedTracks(const void* Data, uint32 SizeBytes)
{
    Reset();

    if (!Data || SizeBytes == 0)
    {
        return false;
    }

    void* Buffer = Allocator.allocate(SizeBytes, alignof(acl::compressed_tracks));
    if (!Buffer)
    {
        UE_LOG_ERROR("[AclCompression] Failed to allocate compressed track buffer | Bytes=%u", SizeBytes);
        return false;
    }

    std::memcpy(Buffer, Data, SizeBytes);

    acl::error_result MakeResult;
    acl::compressed_tracks* Tracks = acl::make_compressed_tracks(Buffer, &MakeResult);
    if (!Tracks)
    {
        UE_LOG_ERROR("[AclCompression] Invalid compressed track buffer: %s", MakeResult.c_str());
        Allocator.deallocate(Buffer, SizeBytes);
        return false;
    }

    CompressedTracks = Tracks;
    return RefreshInfo();
}

void FAclAnimSequenceRuntimeData::Reset()
{
    if (CompressedTracks)
    {
        Allocator.deallocate(CompressedTracks, CompressedTracks->get_size());
        CompressedTracks = nullptr;
    }

    Info = FAclCompressedClipInfo{};
}

bool FAclAnimSequenceRuntimeData::IsValid() const
{
    return CompressedTracks != nullptr;
}

const FAclCompressedClipInfo& FAclAnimSequenceRuntimeData::GetInfo() const
{
    return Info;
}

const acl::compressed_tracks* FAclAnimSequenceRuntimeData::GetCompressedTracks() const
{
    return CompressedTracks;
}

acl::compressed_tracks* FAclAnimSequenceRuntimeData::GetMutableCompressedTracks()
{
    return CompressedTracks;
}

const uint8* FAclAnimSequenceRuntimeData::GetCompressedData() const
{
    return reinterpret_cast<const uint8*>(CompressedTracks);
}

uint32 FAclAnimSequenceRuntimeData::GetCompressedSizeBytes() const
{
    return Info.SizeBytes;
}

bool FAclAnimSequenceRuntimeData::RefreshInfo()
{
    if (!CompressedTracks)
    {
        Info = FAclCompressedClipInfo{};
        return false;
    }

    const acl::error_result ValidationResult = CompressedTracks->is_valid(false);
    if (ValidationResult.any())
    {
        UE_LOG_ERROR("[AclCompression] Invalid compressed tracks metadata: %s", ValidationResult.c_str());
        Reset();
        return false;
    }

    Info.SizeBytes = CompressedTracks->get_size();
    Info.Hash = CompressedTracks->get_hash();
    Info.TrackCount = CompressedTracks->get_num_tracks();
    Info.SamplesPerTrack = CompressedTracks->get_num_samples_per_track();
    Info.SampleRate = CompressedTracks->get_sample_rate();
    Info.Duration = CompressedTracks->get_duration();
    return true;
}

FAclLibraryInfo FAclCompression::GetLibraryInfo()
{
    (void)sizeof(acl::compression_settings);
    (void)sizeof(acl::decompression_context<acl::default_transform_decompression_settings>);
    (void)sizeof(rtm::qvvf);

    return FAclLibraryInfo{
        static_cast<int32>(ACL_VERSION_MAJOR),
        static_cast<int32>(ACL_VERSION_MINOR),
        static_cast<int32>(ACL_VERSION_PATCH)
    };
}

FAclCompressionBuildResult FAclCompression::BuildRuntimeDataFromRawSequence(UAnimSequence* Sequence)
{
    FAclCompressionBuildResult Result;

    if (!Sequence || !Sequence->DataModel)
    {
        Result.ErrorMessage = "Sequence or data model is null";
        return Result;
    }

    UAnimDataModel* DataModel = Sequence->DataModel;
    const uint32 TrackCount = static_cast<uint32>(DataModel->BoneAnimationTracks.size());
    const uint32 SamplesPerTrack = DataModel->NumberOfKeys > 0
        ? static_cast<uint32>(DataModel->NumberOfKeys)
        : static_cast<uint32>(DataModel->NumberOfFrames);
    const float SampleRate = static_cast<float>(DataModel->FrameRate.AsDecimal());

    Result.TrackCount = TrackCount;
    Result.SamplesPerTrack = SamplesPerTrack;
    Result.SampleRate = SampleRate;

    if (TrackCount == 0 || SamplesPerTrack == 0)
    {
        Result.ErrorMessage = "No transform tracks or samples";
        return Result;
    }

    if (!std::isfinite(SampleRate) || SampleRate <= 0.0f)
    {
        Result.ErrorMessage = "Invalid sample rate";
        return Result;
    }

    int32 MaxBoneTreeIndex = -1;
    for (const FBoneAnimationTrack& Track : DataModel->BoneAnimationTracks)
    {
        MaxBoneTreeIndex = std::max(MaxBoneTreeIndex, Track.BoneTreeIndex);
    }

    TArray<int32> BoneToTrackIndex;
    if (MaxBoneTreeIndex >= 0)
    {
        BoneToTrackIndex.assign(static_cast<size_t>(MaxBoneTreeIndex) + 1, -1);
        for (uint32 TrackIndex = 0; TrackIndex < TrackCount; ++TrackIndex)
        {
            const int32 BoneTreeIndex = DataModel->BoneAnimationTracks[TrackIndex].BoneTreeIndex;
            if (BoneTreeIndex >= 0 && BoneTreeIndex < static_cast<int32>(BoneToTrackIndex.size()))
            {
                BoneToTrackIndex[BoneTreeIndex] = static_cast<int32>(TrackIndex);
            }
        }
    }

    std::unique_ptr<FAclAnimSequenceRuntimeData> NewRuntimeData = std::make_unique<FAclAnimSequenceRuntimeData>();
    acl::iallocator& Allocator = NewRuntimeData->GetAllocator();
    acl::track_array_qvvf RawTrackList(Allocator, TrackCount);

    for (uint32 TrackIndex = 0; TrackIndex < TrackCount; ++TrackIndex)
    {
        const FBoneAnimationTrack& SourceTrack = DataModel->BoneAnimationTracks[TrackIndex];
        const FRawAnimSequenceTrack& RawTrack = SourceTrack.InternalTrackData;

        if (RawTrack.PosKeys.size() != SamplesPerTrack ||
            RawTrack.RotKeys.size() != SamplesPerTrack ||
            RawTrack.ScaleKeys.size() != SamplesPerTrack)
        {
            Result.ErrorMessage = "Raw track sample counts are not uniform";
            return Result;
        }

        acl::track_desc_transformf TrackDesc;
        TrackDesc.output_index = TrackIndex;
        TrackDesc.parent_index = ToAclParentTrackIndex(Sequence, BoneToTrackIndex, SourceTrack);

        acl::track_qvvf AclTrack = acl::track_qvvf::make_reserve(TrackDesc, Allocator, SamplesPerTrack, SampleRate);
        for (uint32 SampleIndex = 0; SampleIndex < SamplesPerTrack; ++SampleIndex)
        {
            AclTrack[SampleIndex] = MakeAclTransformSample(
                RawTrack.PosKeys[SampleIndex],
                RawTrack.RotKeys[SampleIndex],
                RawTrack.ScaleKeys[SampleIndex]);
        }

        RawTrackList[TrackIndex] = std::move(AclTrack);
    }

    const acl::error_result RawTracksValidation = RawTrackList.is_valid();
    if (RawTracksValidation.any())
    {
        Result.ErrorMessage = RawTracksValidation.c_str();
        return Result;
    }

    acl::compression_settings Settings = acl::get_default_compression_settings();
    acl::qvvf_transform_error_metric ErrorMetric;
    Settings.error_metric = &ErrorMetric;

    acl::output_stats Stats;
    acl::compressed_tracks* CompressedTracks = nullptr;
    const acl::error_result CompressionResult = acl::compress_track_list(Allocator, RawTrackList, Settings, CompressedTracks, Stats);
    if (CompressionResult.any() || !CompressedTracks)
    {
        Result.ErrorMessage = CompressionResult.any() ? CompressionResult.c_str() : "ACL compression returned null compressed tracks";
        if (CompressedTracks)
        {
            Allocator.deallocate(CompressedTracks, CompressedTracks->get_size());
        }
        return Result;
    }

    Result.RawSizeBytes = RawTrackList.get_raw_size();

    if (!NewRuntimeData->TakeCompressedTracks(CompressedTracks))
    {
        Result.ErrorMessage = "Failed to attach ACL compressed tracks";
        return Result;
    }

    const FAclCompressedClipInfo& ClipInfo = NewRuntimeData->GetInfo();
    Result.bSuccess = true;
    Result.CompressedSizeBytes = ClipInfo.SizeBytes;
    Result.TrackCount = ClipInfo.TrackCount;
    Result.SamplesPerTrack = ClipInfo.SamplesPerTrack;
    Result.SampleRate = ClipInfo.SampleRate;
    Result.Duration = ClipInfo.Duration;

    Sequence->SetAclRuntimeData(NewRuntimeData.release());
    return Result;
}

bool FAclCompression::DecompressPoseToTrackTransforms(const UAnimSequence* Sequence, float SampleTime, TArray<FTransform>& OutTrackTransforms)
{
    if (!Sequence || !Sequence->DataModel || !Sequence->HasAclRuntimeData())
    {
        return false;
    }

    const FAclAnimSequenceRuntimeData* RuntimeData = Sequence->GetAclRuntimeData();
    const acl::compressed_tracks* CompressedTracks = RuntimeData ? RuntimeData->GetCompressedTracks() : nullptr;
    if (!CompressedTracks)
    {
        return false;
    }

    const uint32 TrackCount = static_cast<uint32>(Sequence->DataModel->BoneAnimationTracks.size());
    if (TrackCount == 0 || CompressedTracks->get_num_tracks() != TrackCount)
    {
        UE_LOG_WARNING("[AclCompression] Track count mismatch while decompressing | Sequence=%s | DataModelTracks=%u | AclTracks=%u",
            Sequence->GetName().c_str(),
            TrackCount,
            CompressedTracks->get_num_tracks());
        return false;
    }

    acl::decompression_context<acl::debug_transform_decompression_settings> Context;
    if (!Context.initialize(*CompressedTracks))
    {
        UE_LOG_WARNING("[AclCompression] Failed to initialize decompression context | Sequence=%s",
            Sequence->GetName().c_str());
        return false;
    }

    OutTrackTransforms.assign(TrackCount, FTransform::Identity);
    FAclPoseTrackWriter Writer(OutTrackTransforms);
    Context.seek(SampleTime, acl::sample_rounding_policy::none);
    Context.decompress_tracks(Writer);
    return true;
}
