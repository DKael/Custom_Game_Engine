#include "Editor/UI/AnimInstanceStateSequenceContext.h"

#include "Animation/AnimData/AnimNotifyTypes.h"
#include "Animation/AnimData/AnimSequence.h"
#include "Animation/AnimInstanceAsset.h"
#include "Animation/AnimNotifyPayloadParser.h"
#include "Animation/AnimNotifySemanticFieldNames.h"
#include "Asset/AnimSequenceAssetLoader.h"
#include "Core/Paths.h"
#include "Core/ResourceManager.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>

namespace
{
    FString NormalizeSequenceAssetPath(const FString& Path)
    {
        return FPaths::Normalize(Path);
    }

    bool IsSequenceAssetPath(const FString& Path)
    {
        if (Path.empty())
        {
            return false;
        }

        return std::filesystem::path(FPaths::ToWide(Path)).extension() == L".sequence";
    }

    bool SequenceAssetExistsOnDisk(const FString& Path)
    {
        if (Path.empty())
        {
            return false;
        }

        std::error_code ErrorCode;
        return std::filesystem::exists(
            std::filesystem::path(FPaths::ToAbsolute(FPaths::ToWide(Path))),
            ErrorCode);
    }

    float ComputeFramesPerSecond(int32 Numerator, int32 Denominator)
    {
        if (Numerator <= 0 || Denominator <= 0)
        {
            return 0.0f;
        }

        return static_cast<float>(Numerator) / static_cast<float>(Denominator);
    }

    float ComputeDurationSeconds(int32 NumberOfFrames, int32 Numerator, int32 Denominator)
    {
        const float FramesPerSecond = ComputeFramesPerSecond(Numerator, Denominator);
        return FramesPerSecond > 0.0f
            ? static_cast<float>(NumberOfFrames) / FramesPerSecond
            : 0.0f;
    }

    void AddHint(
        FAnimInstanceStateSequenceContextReport& Report,
        EAnimInstanceStateContextSeverity Severity,
        const FString& Message)
    {
        FAnimInstanceStateAuthoringHint Hint;
        Hint.Severity = Severity;
        Hint.Message = Message;
        Report.Hints.push_back(Hint);
    }

    int32 CountOutgoingStateFinishedTransitions(const UAnimInstanceAsset* Asset, const FAnimInstanceStateAssetData& State)
    {
        if (!Asset)
        {
            return 0;
        }

        int32 Count = 0;
        for (const FAnimInstanceTransitionAssetData& Transition : Asset->Transitions)
        {
            if (Transition.FromState == State.Name &&
                Transition.ConditionType == EAnimTransitionConditionType::StateFinished)
            {
                ++Count;
            }
        }
        return Count;
    }

    FString MakeTrackLabel(const FAnimNotifyTrack& Track, int32 TrackIndex)
    {
        return Track.TrackName.IsValid()
            ? Track.TrackName.ToString()
            : FString("Track ") + std::to_string(TrackIndex + 1);
    }

    FString MakeBuiltInUsageSample(const FAnimNotifyEvent& Event, const FString& Primary, const FString& Secondary)
    {
        char TimeBuffer[32] = {};
        sprintf_s(TimeBuffer, "%.3fs", Event.Time);

        FString Summary = TimeBuffer;
        Summary += " - ";
        Summary += Primary.empty() ? "<missing>" : Primary;
        if (!Secondary.empty())
        {
            Summary += " @ ";
            Summary += Secondary;
        }
        return Summary;
    }

    const char* GetBuiltInUsageDisplayName(const FString& NotifyClassName)
    {
        if (NotifyClassName == "UAnimNotify_PlaySFX")
        {
            return "Play SFX";
        }
        if (NotifyClassName == "UAnimNotifyState_PlayLoopingSFX")
        {
            return "Play Looping SFX";
        }
        if (NotifyClassName == "UAnimNotifyState_AttackWindow")
        {
            return "Attack Window";
        }
        return nullptr;
    }

    FAnimInstanceStateBuiltInNotifyUsage* FindBuiltInUsage(
        TArray<FAnimInstanceStateBuiltInNotifyUsage>& BuiltInUsage,
        const FString& NotifyClassName)
    {
        for (FAnimInstanceStateBuiltInNotifyUsage& Usage : BuiltInUsage)
        {
            if (Usage.NotifyClassName == NotifyClassName)
            {
                return &Usage;
            }
        }
        return nullptr;
    }

    void AccumulateBuiltInUsage(
        TArray<FAnimInstanceStateBuiltInNotifyUsage>& BuiltInUsage,
        const FString& NotifyClassName,
        const FString& Sample,
        int32 MaxBuiltInUsageSamples)
    {
        FAnimInstanceStateBuiltInNotifyUsage* Usage = FindBuiltInUsage(BuiltInUsage, NotifyClassName);
        if (!Usage)
        {
            FAnimInstanceStateBuiltInNotifyUsage NewUsage;
            NewUsage.NotifyClassName = NotifyClassName;
            const char* DisplayName = GetBuiltInUsageDisplayName(NotifyClassName);
            NewUsage.DisplayName = DisplayName ? DisplayName : NotifyClassName;
            BuiltInUsage.push_back(NewUsage);
            Usage = &BuiltInUsage.back();
        }

        ++Usage->Count;
        if (!Sample.empty() && static_cast<int32>(Usage->Samples.size()) < MaxBuiltInUsageSamples)
        {
            Usage->Samples.push_back(Sample);
        }
    }
}

FAnimInstanceStateSequenceContextReport BuildAnimInstanceStateSequenceContext(
    const UAnimInstanceAsset* Asset,
    int32 StateIndex,
    int32 MaxNotifyHighlights,
    int32 MaxBuiltInUsageSamples)
{
    FAnimInstanceStateSequenceContextReport Report;
    if (!Asset || StateIndex < 0 || StateIndex >= static_cast<int32>(Asset->States.size()))
    {
        return Report;
    }

    const FAnimInstanceStateAssetData& State = Asset->States[StateIndex];
    Report.bHasSequencePath = !State.AnimSequencePath.empty();
    if (!Report.bHasSequencePath)
    {
        AddHint(Report, EAnimInstanceStateContextSeverity::Info, "Assign a .sequence asset path for this state.");
        return Report;
    }

    Report.Overview.NormalizedPath = NormalizeSequenceAssetPath(State.AnimSequencePath);
    Report.Overview.ProjectRelativePath = FPaths::ToProjectRelativePath(Report.Overview.NormalizedPath);
    Report.bLooksLikeSequencePath = IsSequenceAssetPath(Report.Overview.ProjectRelativePath);
    if (!Report.bLooksLikeSequencePath)
    {
        AddHint(Report, EAnimInstanceStateContextSeverity::Warning, "Selected path does not point to a .sequence asset.");
        return Report;
    }

    Report.bSequenceExistsOnDisk = SequenceAssetExistsOnDisk(Report.Overview.ProjectRelativePath);
    if (!Report.bSequenceExistsOnDisk)
    {
        AddHint(Report, EAnimInstanceStateContextSeverity::Warning, "Sequence asset file was not found on disk.");
        return Report;
    }

    FAnimSequenceAssetMetadata SequenceMetadata;
    Report.Overview.bMetadataAvailable =
        FAnimSequenceAssetLoader().LoadMetadata(Report.Overview.ProjectRelativePath, SequenceMetadata);
    if (Report.Overview.bMetadataAvailable)
    {
        Report.Overview.DisplayName = SequenceMetadata.ObjectName;
        Report.Overview.SkeletonAssetPath = SequenceMetadata.SkeletonAssetPath;
        Report.Overview.NumberOfFrames = SequenceMetadata.NumberOfFrames;
        Report.Overview.FrameRateNumerator = SequenceMetadata.FrameRateNumerator;
        Report.Overview.FrameRateDenominator = SequenceMetadata.FrameRateDenominator;
        Report.Overview.DurationSeconds = ComputeDurationSeconds(
            SequenceMetadata.NumberOfFrames,
            SequenceMetadata.FrameRateNumerator,
            SequenceMetadata.FrameRateDenominator);
    }

    UAnimSequence* Sequence = FResourceManager::Get().FindAnimSequence(Report.Overview.ProjectRelativePath);
    if (!Sequence)
    {
        Sequence = FResourceManager::Get().LoadAnimSequence(Report.Overview.ProjectRelativePath);
    }

    Report.bSequenceContentLoaded = Sequence && Sequence->DataModel;
    Report.Overview.bSequenceLoaded = Report.bSequenceContentLoaded;
    if (!Report.bSequenceContentLoaded)
    {
        if (Report.Overview.bMetadataAvailable)
        {
            AddHint(
                Report,
                EAnimInstanceStateContextSeverity::Warning,
                "Sequence metadata is available, but full sequence content could not be loaded.");
        }
    }
    else
    {
        UAnimDataModel* DataModel = Sequence->DataModel;
        if (!Sequence->GetName().empty())
        {
            Report.Overview.DisplayName = Sequence->GetName();
        }
        if (!Sequence->GetSkeletonAssetPath().empty())
        {
            Report.Overview.SkeletonAssetPath = Sequence->GetSkeletonAssetPath();
        }
        if (DataModel->NumberOfFrames > 0)
        {
            Report.Overview.NumberOfFrames = DataModel->NumberOfFrames;
        }
        if (DataModel->FrameRate.Numerator > 0)
        {
            Report.Overview.FrameRateNumerator = DataModel->FrameRate.Numerator;
        }
        if (DataModel->FrameRate.Denominator > 0)
        {
            Report.Overview.FrameRateDenominator = DataModel->FrameRate.Denominator;
        }

        Report.Overview.CurveCount = static_cast<int32>(DataModel->CurveData.FloatCurves.size());
        Report.Overview.DurationSeconds = ComputeDurationSeconds(
            Report.Overview.NumberOfFrames,
            Report.Overview.FrameRateNumerator,
            Report.Overview.FrameRateDenominator);

        const TArray<FAnimNotifyTrack>& NotifyTracks = Sequence->GetNotifyTracks();
        Report.NotifySummary.TrackCount = static_cast<int32>(NotifyTracks.size());

        for (int32 TrackIndex = 0; TrackIndex < static_cast<int32>(NotifyTracks.size()); ++TrackIndex)
        {
            const FAnimNotifyTrack& Track = NotifyTracks[TrackIndex];
            const FString TrackLabel = MakeTrackLabel(Track, TrackIndex);
            for (const FAnimNotifyEvent& Event : Track.Events)
            {
                ++Report.NotifySummary.TotalNotifyCount;
                if (Event.IsState())
                {
                    ++Report.NotifySummary.NotifyStateCount;
                }
                else
                {
                    ++Report.NotifySummary.OneShotNotifyCount;
                }

                FAnimInstanceStateNotifyHighlight Highlight;
                Highlight.Time = Event.Time;
                Highlight.TrackName = TrackLabel;
                Highlight.NotifyName = Event.Name.IsValid() ? Event.Name.ToString() : "<Unnamed>";
                Highlight.NotifyType = Event.IsState() ? "NotifyState" : "Notify";
                Highlight.NotifyClassName = Event.GetResolvedNotifyClassName();
                Report.NotifyHighlights.push_back(Highlight);

                const FString NotifyClassName = Event.GetResolvedNotifyClassName();
                if (GetBuiltInUsageDisplayName(NotifyClassName))
                {
                    const FAnimNotifyPayloadParser Parser(Event.Payload);
                    if (NotifyClassName == "UAnimNotify_PlaySFX" ||
                        NotifyClassName == "UAnimNotifyState_PlayLoopingSFX")
                    {
                        const FString Sound = Parser.GetStringAny(AnimNotifySemanticFieldNames::GetLookupKeys(AnimNotifySemanticFieldNames::SoundCueKey()));
                        const FString Socket = Parser.GetStringAny(AnimNotifySemanticFieldNames::GetLookupKeys(AnimNotifySemanticFieldNames::SocketNameKey()));
                        AccumulateBuiltInUsage(
                            Report.BuiltInUsage,
                            NotifyClassName,
                            MakeBuiltInUsageSample(Event, Sound, Socket),
                            MaxBuiltInUsageSamples);
                    }
                    else if (NotifyClassName == "UAnimNotifyState_AttackWindow")
                    {
                        const FString Component = Parser.GetStringAny(AnimNotifySemanticFieldNames::GetLookupKeys(AnimNotifySemanticFieldNames::ComponentNameKey()));
                        const FString AttackId = Parser.GetStringAny(AnimNotifySemanticFieldNames::GetLookupKeys(AnimNotifySemanticFieldNames::AttackIdKey()));
                        FString Summary = MakeBuiltInUsageSample(Event, Component, "");
                        if (!AttackId.empty())
                        {
                            Summary += " | AttackId=";
                            Summary += AttackId;
                        }
                        AccumulateBuiltInUsage(
                            Report.BuiltInUsage,
                            NotifyClassName,
                            Summary,
                            MaxBuiltInUsageSamples);
                    }
                }
            }
        }

        std::sort(
            Report.NotifyHighlights.begin(),
            Report.NotifyHighlights.end(),
            [](const FAnimInstanceStateNotifyHighlight& Left, const FAnimInstanceStateNotifyHighlight& Right)
            {
                if (Left.Time != Right.Time)
                {
                    return Left.Time < Right.Time;
                }
                if (Left.TrackName != Right.TrackName)
                {
                    return Left.TrackName < Right.TrackName;
                }
                return Left.NotifyName < Right.NotifyName;
            });

        if (static_cast<int32>(Report.NotifyHighlights.size()) > MaxNotifyHighlights)
        {
            Report.NotifyHighlights.resize(static_cast<size_t>(MaxNotifyHighlights));
        }
    }

    if (State.bLoop && CountOutgoingStateFinishedTransitions(Asset, State) > 0)
    {
        AddHint(
            Report,
            EAnimInstanceStateContextSeverity::Warning,
            "This state loops, so outgoing StateFinished transitions may never fire.");
    }

    if (Report.bSequenceContentLoaded && Report.NotifySummary.TotalNotifyCount == 0)
    {
        AddHint(Report, EAnimInstanceStateContextSeverity::Info, "Linked sequence contains no notify events.");
    }

    if (!Report.BuiltInUsage.empty())
    {
        AddHint(
            Report,
            EAnimInstanceStateContextSeverity::Info,
            "This state uses built-in sequence notifies for gameplay or audio timing.");
    }

    return Report;
}
