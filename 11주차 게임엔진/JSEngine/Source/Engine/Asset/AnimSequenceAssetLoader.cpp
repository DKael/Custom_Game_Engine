#include "Asset/AnimSequenceAssetLoader.h"

#include "Animation/AnimData/AnimDataModel.h"
#include "Animation/AnimData/AnimSequence.h"
#include "Animation/Compression/AclCompression.h"
#include "Core/Logging/Log.h"
#include "Core/Paths.h"
#include "Object/ObjectFactory.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>

namespace
{
    FString NormalizeSequencePath(const FString& Path)
    {
        return FPaths::ToProjectRelativePath(Path);
    }

    bool IsSequenceAssetPath(const FString& Path)
    {
        FString LowerPath = FPaths::Normalize(Path);
        std::transform(
            LowerPath.begin(),
            LowerPath.end(),
            LowerPath.begin(),
            [](unsigned char Ch)
            {
                return static_cast<char>(std::tolower(Ch));
            });

        return std::filesystem::path(FPaths::ToWide(LowerPath)).extension() == L".sequence";
    }

    constexpr uint32 ANIM_SEQUENCE_BINARY_MAGIC = 0x51455341; // 'ASEQ'
    constexpr uint32 ANIM_SEQUENCE_BINARY_VERSION = 2;

    constexpr uint32 MAX_SEQUENCE_STRING_LENGTH = 4096;
    constexpr uint32 MAX_SEQUENCE_TRACK_COUNT = 65'536;
    constexpr uint32 MAX_SEQUENCE_KEY_COUNT = 1'000'000;
    constexpr uint32 MAX_SEQUENCE_CURVE_COUNT = 4096;
    constexpr uint32 MAX_SEQUENCE_NOTIFY_TRACK_COUNT = 4096;
    constexpr uint32 MAX_SEQUENCE_NOTIFY_EVENT_COUNT = 65'536;
    constexpr uint32 MAX_SEQUENCE_ACL_BYTES = 512u * 1024u * 1024u;

    struct FAnimSequenceBinaryHeader
    {
        uint32 MagicNumber = ANIM_SEQUENCE_BINARY_MAGIC;
        uint32 Version = ANIM_SEQUENCE_BINARY_VERSION;
        int32 FrameRateNumerator = 30;
        int32 FrameRateDenominator = 1;
        int32 NumberOfFrames = 0;
        int32 NumberOfKeys = 0;
        uint32 TrackCount = 0;
        uint32 FloatCurveCount = 0;
        uint32 NotifyTrackCount = 0;
        uint32 AclCompressedSizeBytes = 0;
    };

    bool IsValidSequenceCount(size_t Count, uint32 MaxCount)
    {
        return Count <= static_cast<size_t>(std::numeric_limits<uint32>::max())
            && Count <= static_cast<size_t>(MaxCount);
    }

    bool IsValidSequenceString(const FString& Value)
    {
        return Value.length() <= static_cast<size_t>(MAX_SEQUENCE_STRING_LENGTH);
    }

    void WriteUInt32LE(std::ofstream& Out, uint32 Value)
    {
        unsigned char Bytes[4];
        Bytes[0] = static_cast<unsigned char>((Value >> 0) & 0xFF);
        Bytes[1] = static_cast<unsigned char>((Value >> 8) & 0xFF);
        Bytes[2] = static_cast<unsigned char>((Value >> 16) & 0xFF);
        Bytes[3] = static_cast<unsigned char>((Value >> 24) & 0xFF);
        Out.write(reinterpret_cast<const char*>(Bytes), 4);
    }

    void WriteInt32LE(std::ofstream& Out, int32 Value)
    {
        WriteUInt32LE(Out, static_cast<uint32>(Value));
    }

    void WriteFloatLE(std::ofstream& Out, float Value)
    {
        static_assert(sizeof(float) == sizeof(uint32), "float size must be 4 bytes");

        uint32 Bits = 0;
        std::memcpy(&Bits, &Value, sizeof(float));
        WriteUInt32LE(Out, Bits);
    }

    bool ReadUInt32LE(std::ifstream& In, uint32& OutValue)
    {
        unsigned char Bytes[4] = {};
        In.read(reinterpret_cast<char*>(Bytes), 4);
        if (!In.good())
        {
            return false;
        }

        OutValue =
            (static_cast<uint32>(Bytes[0]) << 0) |
            (static_cast<uint32>(Bytes[1]) << 8) |
            (static_cast<uint32>(Bytes[2]) << 16) |
            (static_cast<uint32>(Bytes[3]) << 24);
        return true;
    }

    bool ReadInt32LE(std::ifstream& In, int32& OutValue)
    {
        uint32 Bits = 0;
        if (!ReadUInt32LE(In, Bits))
        {
            return false;
        }

        OutValue = static_cast<int32>(Bits);
        return true;
    }

    bool ReadFloatLE(std::ifstream& In, float& OutValue)
    {
        uint32 Bits = 0;
        if (!ReadUInt32LE(In, Bits))
        {
            return false;
        }

        std::memcpy(&OutValue, &Bits, sizeof(float));
        return true;
    }

    void WriteStringBinary(std::ofstream& Out, const FString& Value)
    {
        const uint32 Length = static_cast<uint32>(Value.length());
        WriteUInt32LE(Out, Length);
        if (Length > 0)
        {
            Out.write(Value.data(), Length);
        }
    }

    bool ReadStringBinary(std::ifstream& In, FString& OutValue)
    {
        uint32 Length = 0;
        if (!ReadUInt32LE(In, Length))
        {
            OutValue.clear();
            return false;
        }

        if (Length > MAX_SEQUENCE_STRING_LENGTH)
        {
            In.setstate(std::ios::failbit);
            OutValue.clear();
            return false;
        }

        OutValue.resize(Length);
        if (Length > 0)
        {
            In.read(OutValue.data(), Length);
            if (!In.good())
            {
                OutValue.clear();
                return false;
            }
        }

        return true;
    }

    void WriteBinaryHeader(std::ofstream& Out, const FAnimSequenceBinaryHeader& Header)
    {
        WriteUInt32LE(Out, Header.MagicNumber);
        WriteUInt32LE(Out, Header.Version);
        WriteInt32LE(Out, Header.FrameRateNumerator);
        WriteInt32LE(Out, Header.FrameRateDenominator);
        WriteInt32LE(Out, Header.NumberOfFrames);
        WriteInt32LE(Out, Header.NumberOfKeys);
        WriteUInt32LE(Out, Header.TrackCount);
        WriteUInt32LE(Out, Header.FloatCurveCount);
        WriteUInt32LE(Out, Header.NotifyTrackCount);
        WriteUInt32LE(Out, Header.AclCompressedSizeBytes);
    }

    bool ReadBinaryHeader(std::ifstream& In, FAnimSequenceBinaryHeader& OutHeader)
    {
        return ReadUInt32LE(In, OutHeader.MagicNumber)
            && ReadUInt32LE(In, OutHeader.Version)
            && ReadInt32LE(In, OutHeader.FrameRateNumerator)
            && ReadInt32LE(In, OutHeader.FrameRateDenominator)
            && ReadInt32LE(In, OutHeader.NumberOfFrames)
            && ReadInt32LE(In, OutHeader.NumberOfKeys)
            && ReadUInt32LE(In, OutHeader.TrackCount)
            && ReadUInt32LE(In, OutHeader.FloatCurveCount)
            && ReadUInt32LE(In, OutHeader.NotifyTrackCount)
            && ReadUInt32LE(In, OutHeader.AclCompressedSizeBytes);
    }

    bool IsSupportedBinaryHeader(const FAnimSequenceBinaryHeader& Header)
    {
        return Header.MagicNumber == ANIM_SEQUENCE_BINARY_MAGIC
            && Header.Version == ANIM_SEQUENCE_BINARY_VERSION
            && Header.TrackCount <= MAX_SEQUENCE_TRACK_COUNT
            && Header.FloatCurveCount <= MAX_SEQUENCE_CURVE_COUNT
            && Header.NotifyTrackCount <= MAX_SEQUENCE_NOTIFY_TRACK_COUNT
            && Header.AclCompressedSizeBytes > 0
            && Header.AclCompressedSizeBytes <= MAX_SEQUENCE_ACL_BYTES;
    }

    bool TryReadBinaryMagic(const FString& NormalizedPath, uint32& OutMagic)
    {
        OutMagic = 0;
        std::ifstream InFile(
            std::filesystem::path(FPaths::ToAbsolute(FPaths::ToWide(NormalizedPath))),
            std::ios::binary);
        return InFile.is_open() && ReadUInt32LE(InFile, OutMagic);
    }

    void WriteVectorBinary(std::ofstream& Out, const FVector& Value)
    {
        WriteFloatLE(Out, Value.X);
        WriteFloatLE(Out, Value.Y);
        WriteFloatLE(Out, Value.Z);
    }

    bool ReadVectorBinary(std::ifstream& In, FVector& OutValue)
    {
        return ReadFloatLE(In, OutValue.X)
            && ReadFloatLE(In, OutValue.Y)
            && ReadFloatLE(In, OutValue.Z);
    }

    void WriteQuatBinary(std::ofstream& Out, const FQuat& Value)
    {
        WriteFloatLE(Out, Value.X);
        WriteFloatLE(Out, Value.Y);
        WriteFloatLE(Out, Value.Z);
        WriteFloatLE(Out, Value.W);
    }

    bool ReadQuatBinary(std::ifstream& In, FQuat& OutValue)
    {
        return ReadFloatLE(In, OutValue.X)
            && ReadFloatLE(In, OutValue.Y)
            && ReadFloatLE(In, OutValue.Z)
            && ReadFloatLE(In, OutValue.W);
    }

    void WriteColorBinary(std::ofstream& Out, const FColor& Value)
    {
        WriteFloatLE(Out, Value.R);
        WriteFloatLE(Out, Value.G);
        WriteFloatLE(Out, Value.B);
        WriteFloatLE(Out, Value.A);
    }

    bool ReadColorBinary(std::ifstream& In, FColor& OutValue)
    {
        return ReadFloatLE(In, OutValue.R)
            && ReadFloatLE(In, OutValue.G)
            && ReadFloatLE(In, OutValue.B)
            && ReadFloatLE(In, OutValue.A);
    }

    void WriteGuidBinary(std::ofstream& Out, const FGuid& Value)
    {
        WriteUInt32LE(Out, Value.A);
        WriteUInt32LE(Out, Value.B);
        WriteUInt32LE(Out, Value.C);
        WriteUInt32LE(Out, Value.D);
    }

    bool ReadGuidBinary(std::ifstream& In, FGuid& OutValue)
    {
        return ReadUInt32LE(In, OutValue.A)
            && ReadUInt32LE(In, OutValue.B)
            && ReadUInt32LE(In, OutValue.C)
            && ReadUInt32LE(In, OutValue.D);
    }

    void WriteVectorKeyArrayBinary(std::ofstream& Out, const TArray<FVector>& Keys)
    {
        WriteUInt32LE(Out, static_cast<uint32>(Keys.size()));
        for (const FVector& Key : Keys)
        {
            WriteVectorBinary(Out, Key);
        }
    }

    bool ReadVectorKeyArrayBinary(std::ifstream& In, TArray<FVector>& OutKeys)
    {
        uint32 Count = 0;
        if (!ReadUInt32LE(In, Count) || Count > MAX_SEQUENCE_KEY_COUNT)
        {
            In.setstate(std::ios::failbit);
            return false;
        }

        OutKeys.clear();
        OutKeys.resize(Count);
        for (FVector& Key : OutKeys)
        {
            if (!ReadVectorBinary(In, Key))
            {
                OutKeys.clear();
                return false;
            }
        }

        return true;
    }

    void WriteQuatKeyArrayBinary(std::ofstream& Out, const TArray<FQuat>& Keys)
    {
        WriteUInt32LE(Out, static_cast<uint32>(Keys.size()));
        for (const FQuat& Key : Keys)
        {
            WriteQuatBinary(Out, Key);
        }
    }

    bool ReadQuatKeyArrayBinary(std::ifstream& In, TArray<FQuat>& OutKeys)
    {
        uint32 Count = 0;
        if (!ReadUInt32LE(In, Count) || Count > MAX_SEQUENCE_KEY_COUNT)
        {
            In.setstate(std::ios::failbit);
            return false;
        }

        OutKeys.clear();
        OutKeys.resize(Count);
        for (FQuat& Key : OutKeys)
        {
            if (!ReadQuatBinary(In, Key))
            {
                OutKeys.clear();
                return false;
            }
        }

        return true;
    }

    bool ValidateSequenceForBinary(const UAnimSequence* Sequence)
    {
        if (!Sequence || !Sequence->DataModel)
        {
            return false;
        }

        const UAnimDataModel* DataModel = Sequence->DataModel;
        const FAclAnimSequenceRuntimeData* AclRuntimeData = Sequence->GetAclRuntimeData();
        if (!AclRuntimeData || !AclRuntimeData->IsValid())
        {
            return false;
        }

        const FAclCompressedClipInfo& AclInfo = AclRuntimeData->GetInfo();
        if (!IsValidSequenceString(Sequence->GetName()) ||
            !IsValidSequenceString(Sequence->GetSkeletonAssetPath()) ||
            !IsValidSequenceCount(DataModel->BoneAnimationTracks.size(), MAX_SEQUENCE_TRACK_COUNT) ||
            !IsValidSequenceCount(DataModel->CurveData.FloatCurves.size(), MAX_SEQUENCE_CURVE_COUNT) ||
            !IsValidSequenceCount(DataModel->NotifyTracks.size(), MAX_SEQUENCE_NOTIFY_TRACK_COUNT) ||
            AclInfo.TrackCount != static_cast<uint32>(DataModel->BoneAnimationTracks.size()) ||
            AclInfo.SizeBytes == 0 ||
            AclInfo.SizeBytes > MAX_SEQUENCE_ACL_BYTES)
        {
            return false;
        }

        for (const FBoneAnimationTrack& Track : DataModel->BoneAnimationTracks)
        {
            if (!IsValidSequenceString(Track.Name.ToString()))
            {
                return false;
            }
        }

        for (const FFloatCurve& Curve : DataModel->CurveData.FloatCurves)
        {
            if (!IsValidSequenceString(Curve.CurveName.ToString()) ||
                !IsValidSequenceCount(Curve.Keys.size(), MAX_SEQUENCE_KEY_COUNT))
            {
                return false;
            }
        }

        for (const FAnimNotifyTrack& Track : DataModel->NotifyTracks)
        {
            if (!IsValidSequenceString(Track.TrackName.ToString()) ||
                !IsValidSequenceCount(Track.Events.size(), MAX_SEQUENCE_NOTIFY_EVENT_COUNT))
            {
                return false;
            }

            for (const FAnimNotifyEvent& Event : Track.Events)
            {
                if (!IsValidSequenceString(Event.Name.ToString()) ||
                    !IsValidSequenceString(Event.GetResolvedNotifyClassName()) ||
                    !IsValidSequenceString(Event.Payload))
                {
                    return false;
                }
            }
        }

        return true;
    }

    void WriteTrackBinary(std::ofstream& Out, const FBoneAnimationTrack& Track)
    {
        WriteStringBinary(Out, Track.Name.ToString());
        WriteInt32LE(Out, Track.BoneTreeIndex);
    }

    bool ReadTrackBinary(std::ifstream& In, FBoneAnimationTrack& OutTrack)
    {
        FString TrackName;
        if (!ReadStringBinary(In, TrackName) ||
            !ReadInt32LE(In, OutTrack.BoneTreeIndex))
        {
            return false;
        }

        OutTrack.Name = FName(TrackName);
        OutTrack.InternalTrackData.PosKeys.clear();
        OutTrack.InternalTrackData.RotKeys.clear();
        OutTrack.InternalTrackData.ScaleKeys.clear();
        return true;
    }

    void WriteFloatCurveBinary(std::ofstream& Out, const FFloatCurve& Curve)
    {
        WriteStringBinary(Out, Curve.CurveName.ToString());
        WriteUInt32LE(Out, static_cast<uint32>(Curve.CurveType));
        WriteUInt32LE(Out, static_cast<uint32>(Curve.SourceKind));
        WriteUInt32LE(Out, static_cast<uint32>(Curve.Keys.size()));

        for (const FCurveKey& Key : Curve.Keys)
        {
            WriteFloatLE(Out, Key.Time);
            WriteFloatLE(Out, Key.Value);
            WriteUInt32LE(Out, static_cast<uint32>(Key.InterpMode));
            WriteUInt32LE(Out, static_cast<uint32>(Key.TangentMode));
            WriteFloatLE(Out, Key.ArriveTangent);
            WriteFloatLE(Out, Key.LeaveTangent);
        }
    }

    bool ReadFloatCurveBinary(std::ifstream& In, FFloatCurve& OutCurve)
    {
        FString CurveName;
        uint32 CurveType = 0;
        uint32 SourceKind = 0;
        uint32 KeyCount = 0;

        if (!ReadStringBinary(In, CurveName) ||
            !ReadUInt32LE(In, CurveType) ||
            !ReadUInt32LE(In, SourceKind) ||
            !ReadUInt32LE(In, KeyCount) ||
            KeyCount > MAX_SEQUENCE_KEY_COUNT)
        {
            In.setstate(std::ios::failbit);
            return false;
        }

        OutCurve.CurveName = FName(CurveName);
        OutCurve.CurveType = static_cast<EAnimCurveType>(CurveType);
        OutCurve.SourceKind = static_cast<EAnimCurveSourceKind>(SourceKind);
        OutCurve.Keys.clear();
        OutCurve.Keys.reserve(KeyCount);

        for (uint32 KeyIndex = 0; KeyIndex < KeyCount; ++KeyIndex)
        {
            uint32 InterpMode = 0;
            uint32 TangentMode = 0;
            FCurveKey Key;
            if (!ReadFloatLE(In, Key.Time) ||
                !ReadFloatLE(In, Key.Value) ||
                !ReadUInt32LE(In, InterpMode) ||
                !ReadUInt32LE(In, TangentMode) ||
                !ReadFloatLE(In, Key.ArriveTangent) ||
                !ReadFloatLE(In, Key.LeaveTangent))
            {
                OutCurve.Keys.clear();
                return false;
            }

            Key.InterpMode = static_cast<ECurveInterpMode>(InterpMode);
            Key.TangentMode = static_cast<ECurveTangentMode>(TangentMode);
            OutCurve.Keys.push_back(Key);
        }

        OutCurve.SortKeys();
        return true;
    }

    void WriteNotifyTrackBinary(std::ofstream& Out, const FAnimNotifyTrack& Track)
    {
        WriteStringBinary(Out, Track.TrackName.ToString());
        WriteUInt32LE(Out, static_cast<uint32>(Track.Events.size()));

        for (const FAnimNotifyEvent& Event : Track.Events)
        {
            WriteGuidBinary(Out, Event.StableId);
            WriteStringBinary(Out, Event.Name.ToString());
            WriteFloatLE(Out, Event.Time);
            WriteFloatLE(Out, Event.Duration);
            WriteColorBinary(Out, Event.Color);
            WriteUInt32LE(Out, static_cast<uint32>(Event.EventType));
            WriteStringBinary(Out, Event.GetResolvedNotifyClassName());
            WriteStringBinary(Out, Event.Payload);
        }
    }

    bool ReadNotifyTrackBinary(std::ifstream& In, FAnimNotifyTrack& OutTrack)
    {
        FString TrackName;
        uint32 EventCount = 0;
        if (!ReadStringBinary(In, TrackName) ||
            !ReadUInt32LE(In, EventCount) ||
            EventCount > MAX_SEQUENCE_NOTIFY_EVENT_COUNT)
        {
            In.setstate(std::ios::failbit);
            return false;
        }

        OutTrack.TrackName = FName(TrackName);
        OutTrack.Events.clear();
        OutTrack.Events.reserve(EventCount);

        for (uint32 EventIndex = 0; EventIndex < EventCount; ++EventIndex)
        {
            FString EventName;
            FString NotifyClassName;
            uint32 EventType = 0;
            FAnimNotifyEvent Event;

            if (!ReadGuidBinary(In, Event.StableId) ||
                !ReadStringBinary(In, EventName) ||
                !ReadFloatLE(In, Event.Time) ||
                !ReadFloatLE(In, Event.Duration) ||
                !ReadColorBinary(In, Event.Color) ||
                !ReadUInt32LE(In, EventType) ||
                !ReadStringBinary(In, NotifyClassName) ||
                !ReadStringBinary(In, Event.Payload))
            {
                OutTrack.Events.clear();
                return false;
            }

            Event.Name = FName(EventName);
            Event.EventType = static_cast<EAnimNotifyEventType>(EventType);
            Event.NotifyClassName = NotifyClassName.empty()
                ? GetDefaultAnimNotifyClassName(Event.EventType)
                : NotifyClassName;
            Event.EnsureStableId();
            OutTrack.Events.push_back(Event);
        }

        return true;
    }

    uint64 GetFileSizeBytes(const std::filesystem::path& FilePath)
    {
        std::error_code ErrorCode;
        const uint64 Size = static_cast<uint64>(std::filesystem::file_size(FilePath, ErrorCode));
        return ErrorCode ? 0 : Size;
    }

    void DestroyLoadedSequence(UAnimSequence* Sequence, UAnimDataModel* DataModel)
    {
        if (Sequence)
        {
            if (!Sequence->DataModel)
            {
                delete DataModel;
            }
            UObjectManager::Get().DestroyObject(Sequence);
            return;
        }

        delete DataModel;
    }

    UAnimSequence* LoadBinarySequence(const FString& NormalizedPath)
    {
        const auto LoadStart = std::chrono::steady_clock::now();
        const std::filesystem::path FilePath(FPaths::ToAbsolute(FPaths::ToWide(NormalizedPath)));

        std::ifstream InFile(FilePath, std::ios::binary);
        if (!InFile.is_open())
        {
            UE_LOG_ERROR("[AnimSequenceAssetLoader] Failed to open binary sequence asset: %s", NormalizedPath.c_str());
            return nullptr;
        }

        FAnimSequenceBinaryHeader Header;
        if (!ReadBinaryHeader(InFile, Header) || !IsSupportedBinaryHeader(Header))
        {
            UE_LOG_ERROR("[AnimSequenceAssetLoader] Invalid binary sequence asset header: %s", NormalizedPath.c_str());
            return nullptr;
        }

        FString SequenceName;
        FString SkeletonAssetPath;
        if (!ReadStringBinary(InFile, SequenceName) || !ReadStringBinary(InFile, SkeletonAssetPath))
        {
            UE_LOG_ERROR("[AnimSequenceAssetLoader] Invalid binary sequence asset metadata: %s", NormalizedPath.c_str());
            return nullptr;
        }

        UAnimSequence* Sequence = UObjectManager::Get().CreateObject<UAnimSequence>();
        UAnimDataModel* DataModel = new UAnimDataModel();

        Sequence->SetFName(FName(SequenceName.empty() ? "AnimSequence" : SequenceName));
        Sequence->SetSkeletonAssetPath(FPaths::ToProjectRelativePath(SkeletonAssetPath));

        DataModel->SetFName(FName(Sequence->GetName() + FString("_Data")));
        DataModel->FrameRate = FFrameRate(Header.FrameRateNumerator, Header.FrameRateDenominator);
        DataModel->NumberOfFrames = Header.NumberOfFrames;
        DataModel->NumberOfKeys = Header.NumberOfKeys;
        DataModel->BoneAnimationTracks.reserve(Header.TrackCount);
        DataModel->CurveData.FloatCurves.reserve(Header.FloatCurveCount);
        DataModel->NotifyTracks.reserve(Header.NotifyTrackCount);

        for (uint32 TrackIndex = 0; TrackIndex < Header.TrackCount; ++TrackIndex)
        {
            FBoneAnimationTrack Track;
            if (!ReadTrackBinary(InFile, Track))
            {
                UE_LOG_ERROR("[AnimSequenceAssetLoader] Failed to read binary sequence track: %s", NormalizedPath.c_str());
                DestroyLoadedSequence(Sequence, DataModel);
                return nullptr;
            }
            DataModel->BoneAnimationTracks.push_back(Track);
        }

        for (uint32 CurveIndex = 0; CurveIndex < Header.FloatCurveCount; ++CurveIndex)
        {
            FFloatCurve Curve;
            if (!ReadFloatCurveBinary(InFile, Curve))
            {
                UE_LOG_ERROR("[AnimSequenceAssetLoader] Failed to read binary sequence curve: %s", NormalizedPath.c_str());
                DestroyLoadedSequence(Sequence, DataModel);
                return nullptr;
            }
            DataModel->CurveData.FloatCurves.push_back(Curve);
        }

        for (uint32 NotifyTrackIndex = 0; NotifyTrackIndex < Header.NotifyTrackCount; ++NotifyTrackIndex)
        {
            FAnimNotifyTrack Track;
            if (!ReadNotifyTrackBinary(InFile, Track))
            {
                UE_LOG_ERROR("[AnimSequenceAssetLoader] Failed to read binary sequence notify track: %s", NormalizedPath.c_str());
                DestroyLoadedSequence(Sequence, DataModel);
                return nullptr;
            }
            DataModel->NotifyTracks.push_back(Track);
        }

        TArray<uint8> AclBytes;
        AclBytes.resize(Header.AclCompressedSizeBytes);
        InFile.read(reinterpret_cast<char*>(AclBytes.data()), Header.AclCompressedSizeBytes);
        if (!InFile.good())
        {
            UE_LOG_ERROR("[AnimSequenceAssetLoader] Failed to read ACL sequence data: %s", NormalizedPath.c_str());
            DestroyLoadedSequence(Sequence, DataModel);
            return nullptr;
        }

        std::unique_ptr<FAclAnimSequenceRuntimeData> AclRuntimeData = std::make_unique<FAclAnimSequenceRuntimeData>();
        if (!AclRuntimeData->CopyCompressedTracks(AclBytes.data(), Header.AclCompressedSizeBytes))
        {
            UE_LOG_ERROR("[AnimSequenceAssetLoader] Invalid ACL sequence data: %s", NormalizedPath.c_str());
            DestroyLoadedSequence(Sequence, DataModel);
            return nullptr;
        }

        const FAclCompressedClipInfo& AclInfo = AclRuntimeData->GetInfo();
        if (AclInfo.TrackCount != Header.TrackCount)
        {
            UE_LOG_ERROR("[AnimSequenceAssetLoader] ACL sequence track mismatch: %s", NormalizedPath.c_str());
            DestroyLoadedSequence(Sequence, DataModel);
            return nullptr;
        }

        if (!InFile.good() && !InFile.eof())
        {
            UE_LOG_ERROR("[AnimSequenceAssetLoader] Failed while reading binary sequence asset: %s", NormalizedPath.c_str());
            DestroyLoadedSequence(Sequence, DataModel);
            return nullptr;
        }

        Sequence->DataModel = DataModel;
        Sequence->SetAclRuntimeData(AclRuntimeData.release());

        const auto LoadEnd = std::chrono::steady_clock::now();
        const double LoadSec = std::chrono::duration<double>(LoadEnd - LoadStart).count();
        UE_LOG("[SkeletalMeshLoad] Animation Sequence Load | Format=AclBinary | Path=%s | Sequence=%s | Tracks=%zu | Frames=%d | Keys=%d | Curves=%zu | Bytes=%llu | AclBytes=%u | Sec=%.6f | Ms=%.3f",
            NormalizedPath.c_str(),
            Sequence->GetName().c_str(),
            DataModel->BoneAnimationTracks.size(),
            DataModel->NumberOfFrames,
            DataModel->NumberOfKeys,
            DataModel->CurveData.FloatCurves.size(),
            static_cast<unsigned long long>(GetFileSizeBytes(FilePath)),
            AclInfo.SizeBytes,
            LoadSec,
            LoadSec * 1000.0);

        return Sequence;
    }

    bool LoadBinaryMetadata(const FString& NormalizedPath, FAnimSequenceAssetMetadata& OutMetadata)
    {
        std::ifstream InFile(
            std::filesystem::path(FPaths::ToAbsolute(FPaths::ToWide(NormalizedPath))),
            std::ios::binary);
        if (!InFile.is_open())
        {
            return false;
        }

        FAnimSequenceBinaryHeader Header;
        if (!ReadBinaryHeader(InFile, Header) || !IsSupportedBinaryHeader(Header))
        {
            return false;
        }

        FString SequenceName;
        FString SkeletonAssetPath;
        if (!ReadStringBinary(InFile, SequenceName) || !ReadStringBinary(InFile, SkeletonAssetPath))
        {
            return false;
        }

        OutMetadata.ObjectName = SequenceName;
        OutMetadata.SkeletonAssetPath = FPaths::Normalize(SkeletonAssetPath);
        OutMetadata.FrameRateNumerator = Header.FrameRateNumerator;
        OutMetadata.FrameRateDenominator = Header.FrameRateDenominator;
        OutMetadata.NumberOfFrames = Header.NumberOfFrames;
        OutMetadata.NumberOfKeys = Header.NumberOfKeys;
        OutMetadata.BoneTrackCount = static_cast<int32>(Header.TrackCount);
        return OutMetadata.IsValid();
    }
}

UAnimSequence* FAnimSequenceAssetLoader::Load(const FString& Path) const
{
    const FString NormalizedPath = NormalizeSequencePath(Path);
    if (NormalizedPath.empty() || !IsSequenceAssetPath(NormalizedPath))
    {
        return nullptr;
    }

    uint32 Magic = 0;
    if (!TryReadBinaryMagic(NormalizedPath, Magic))
    {
        UE_LOG_ERROR("[AnimSequenceAssetLoader] Failed to read sequence asset header: %s", NormalizedPath.c_str());
        return nullptr;
    }

    if (Magic != ANIM_SEQUENCE_BINARY_MAGIC)
    {
        UE_LOG_ERROR("[AnimSequenceAssetLoader] Unsupported legacy JSON sequence asset. Reimport or resave as ACL binary: %s", NormalizedPath.c_str());
        return nullptr;
    }

    return LoadBinarySequence(NormalizedPath);
}

bool FAnimSequenceAssetLoader::LoadMetadata(const FString& Path, FAnimSequenceAssetMetadata& OutMetadata) const
{
    OutMetadata = {};

    const FString NormalizedPath = NormalizeSequencePath(Path);
    if (NormalizedPath.empty() || !IsSequenceAssetPath(NormalizedPath))
    {
        return false;
    }

    uint32 Magic = 0;
    if (!TryReadBinaryMagic(NormalizedPath, Magic) || Magic != ANIM_SEQUENCE_BINARY_MAGIC)
    {
        return false;
    }

    return LoadBinaryMetadata(NormalizedPath, OutMetadata);
}

bool FAnimSequenceAssetLoader::Save(const FString& Path, const UAnimSequence* Sequence) const
{
    if (!Sequence || !Sequence->DataModel)
    {
        return false;
    }

    const FString NormalizedPath = NormalizeSequencePath(Path);
    if (NormalizedPath.empty() || !IsSequenceAssetPath(NormalizedPath))
    {
        return false;
    }

    const UAnimDataModel* DataModel = Sequence->DataModel;
    if (!ValidateSequenceForBinary(Sequence))
    {
        UE_LOG_ERROR("[AnimSequenceAssetLoader] Animation sequence is not valid for ACL binary format: %s", NormalizedPath.c_str());
        return false;
    }

    const FAclAnimSequenceRuntimeData* AclRuntimeData = Sequence->GetAclRuntimeData();
    const FAclCompressedClipInfo& AclInfo = AclRuntimeData->GetInfo();

    const auto SaveStart = std::chrono::steady_clock::now();

    std::error_code ErrorCode;
    const std::filesystem::path FilePath(FPaths::ToAbsolute(FPaths::ToWide(NormalizedPath)));
    std::filesystem::create_directories(FilePath.parent_path(), ErrorCode);

    std::ofstream OutFile(FilePath, std::ios::binary);
    if (!OutFile.is_open())
    {
        UE_LOG_ERROR("[AnimSequenceAssetLoader] Failed to open sequence asset for writing: %s", NormalizedPath.c_str());
        return false;
    }

    FAnimSequenceBinaryHeader Header;
    Header.FrameRateNumerator = DataModel->FrameRate.Numerator;
    Header.FrameRateDenominator = DataModel->FrameRate.Denominator;
    Header.NumberOfFrames = DataModel->NumberOfFrames;
    Header.NumberOfKeys = DataModel->NumberOfKeys;
    Header.TrackCount = static_cast<uint32>(DataModel->BoneAnimationTracks.size());
    Header.FloatCurveCount = static_cast<uint32>(DataModel->CurveData.FloatCurves.size());
    Header.NotifyTrackCount = static_cast<uint32>(DataModel->NotifyTracks.size());
    Header.AclCompressedSizeBytes = AclInfo.SizeBytes;

    WriteBinaryHeader(OutFile, Header);
    WriteStringBinary(OutFile, Sequence->GetName());
    WriteStringBinary(OutFile, FPaths::ToProjectRelativePath(Sequence->GetSkeletonAssetPath()));

    for (const FBoneAnimationTrack& Track : DataModel->BoneAnimationTracks)
    {
        WriteTrackBinary(OutFile, Track);
    }

    for (const FFloatCurve& Curve : DataModel->CurveData.FloatCurves)
    {
        WriteFloatCurveBinary(OutFile, Curve);
    }

    for (const FAnimNotifyTrack& Track : DataModel->NotifyTracks)
    {
        WriteNotifyTrackBinary(OutFile, Track);
    }

    OutFile.write(reinterpret_cast<const char*>(AclRuntimeData->GetCompressedData()), AclInfo.SizeBytes);

    OutFile.flush();
    if (!OutFile.good())
    {
        UE_LOG_ERROR("[AnimSequenceAssetLoader] Failed while writing ACL binary sequence asset: %s", NormalizedPath.c_str());
        return false;
    }

    const auto SaveEnd = std::chrono::steady_clock::now();
    const double SaveElapsedSec = std::chrono::duration<double>(SaveEnd - SaveStart).count();
    UE_LOG("[SkeletalMeshLoad] Animation Sequence Save | Format=AclBinary | Path=%s | Sequence=%s | Tracks=%zu | Frames=%d | Keys=%d | Curves=%zu | Bytes=%llu | AclBytes=%u | Sec=%.6f | Ms=%.3f",
        NormalizedPath.c_str(),
        Sequence->GetName().c_str(),
        DataModel->BoneAnimationTracks.size(),
        DataModel->NumberOfFrames,
        DataModel->NumberOfKeys,
        DataModel->CurveData.FloatCurves.size(),
        static_cast<unsigned long long>(GetFileSizeBytes(FilePath)),
        AclInfo.SizeBytes,
        SaveElapsedSec,
        SaveElapsedSec * 1000.0);
    return true;
}

bool FAnimSequenceAssetLoader::SupportsExtension(const FString& Extension) const
{
    return Extension == ".sequence" || Extension == "sequence";
}

FString FAnimSequenceAssetLoader::GetLoaderName() const
{
    return "FAnimSequenceAssetLoader";
}
