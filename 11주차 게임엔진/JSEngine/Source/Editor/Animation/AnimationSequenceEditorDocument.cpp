#include "Editor/Animation/AnimationSequenceEditorDocument.h"

#include "Animation/AnimNotify.h"
#include "Animation/AnimNotifySemanticFieldNames.h"
#include "Animation/AnimData/AnimDataModel.h"
#include "Animation/AnimData/AnimSequence.h"
#include "Core/Paths.h"
#include "Core/ResourceManager.h"
#include "Editor/Animation/AnimationSequenceEditorWidget.h"
#include "Editor/Animation/AnimationSequenceNotifyValidation.h"
#include "Editor/Animation/AnimationSequencePreviewController.h"
#include "Editor/Animation/AnimationSequenceViewerUtils.h"
#include "Editor/EditorEngine.h"
#include "Editor/Viewport/SkeletalMeshViewportClient.h"
#include "Object/ObjectFactory.h"

#include "ImGui/imgui.h"

#include <algorithm>

namespace
{
    const UClass* FindRegisteredClassByName(const FString& ClassName)
    {
        if (ClassName.empty())
        {
            return nullptr;
        }

        TArray<const UClass*> RegisteredClasses;
        FObjectFactory::Get().GetRegisteredClasses(RegisteredClasses);
        for (const UClass* RegisteredClass : RegisteredClasses)
        {
            if (RegisteredClass && RegisteredClass->GetName() == ClassName)
            {
                return RegisteredClass;
            }
        }

        return nullptr;
    }

    FAnimNotifyTrack MakeDefaultNotifyTrack(int32 TrackIndex)
    {
        FAnimNotifyTrack Track;
        Track.TrackName = FName(("Track " + std::to_string(TrackIndex + 1)).c_str());
        return Track;
    }

    int32 FindNotifyEventIndexById(const FAnimNotifyTrack& Track, const FGuid& StableId)
    {
        if (!StableId.IsValid())
        {
            return -1;
        }

        for (int32 Index = 0; Index < static_cast<int32>(Track.Events.size()); ++Index)
        {
            if (Track.Events[Index].StableId == StableId)
            {
                return Index;
            }
        }

        return -1;
    }

    void SortNotifyTrackEvents(FAnimNotifyTrack& Track)
    {
        std::stable_sort(
            Track.Events.begin(),
            Track.Events.end(),
            [](const FAnimNotifyEvent& Left, const FAnimNotifyEvent& Right)
            {
                if (Left.Time == Right.Time)
                {
                    return Left.Name.ToString() < Right.Name.ToString();
                }

                return Left.Time < Right.Time;
            });
    }

    float GetNotifyDuplicateTimeOffset(const FAnimationSequenceEditorState& State)
    {
        const double FrameRate = State.DisplayFrameRate.AsDecimal();
        if (FrameRate > 0.0)
        {
            return static_cast<float>(1.0 / FrameRate);
        }

        return 1.0f / 30.0f;
    }

    void EnsureTrackExists(TArray<FAnimNotifyTrack>& Tracks, int32 TrackIndex)
    {
        while (TrackIndex >= static_cast<int32>(Tracks.size()))
        {
            Tracks.push_back(MakeDefaultNotifyTrack(static_cast<int32>(Tracks.size())));
        }

        if (TrackIndex >= 0 && !Tracks[TrackIndex].TrackName.IsValid())
        {
            Tracks[TrackIndex].TrackName = MakeDefaultNotifyTrack(TrackIndex).TrackName;
        }
    }

    int32 InsertNotifyEventSorted(FAnimNotifyTrack& Track, const FAnimNotifyEvent& Event)
    {
        Track.Events.push_back(Event);
        SortNotifyTrackEvents(Track);
        return FindNotifyEventIndexById(Track, Event.StableId);
    }

    void RemapTrackIndexAfterMove(int32& TrackIndex, int32 FromIndex, int32 ToIndex)
    {
        if (TrackIndex < 0 || FromIndex == ToIndex)
        {
            return;
        }

        if (TrackIndex == FromIndex)
        {
            TrackIndex = ToIndex;
            return;
        }

        if (FromIndex < ToIndex)
        {
            if (TrackIndex > FromIndex && TrackIndex <= ToIndex)
            {
                --TrackIndex;
            }
            return;
        }

        if (TrackIndex >= ToIndex && TrackIndex < FromIndex)
        {
            ++TrackIndex;
        }
    }

    void RemapSequencerRowIdAfterMove(FAnimationSequenceSequencerRowId& RowId, int32 FromIndex, int32 ToIndex)
    {
        if (!RowId.IsValid() || RowId.Type != EAnimationSequenceSequencerVisibleRowType::NotifyTrack)
        {
            return;
        }

        RemapTrackIndexAfterMove(RowId.SourceIndex, FromIndex, ToIndex);
    }

    void RemapSequencerRowIdAfterDelete(FAnimationSequenceSequencerRowId& RowId, int32 DeletedIndex)
    {
        if (!RowId.IsValid() || RowId.Type != EAnimationSequenceSequencerVisibleRowType::NotifyTrack)
        {
            return;
        }

        if (RowId.SourceIndex == DeletedIndex)
        {
            RowId = FAnimationSequenceSequencerRowId();
        }
        else if (RowId.SourceIndex > DeletedIndex)
        {
            --RowId.SourceIndex;
        }
    }
}

FAnimationSequenceEditorDocument::FAnimationSequenceEditorDocument() = default;

FAnimationSequenceEditorDocument::~FAnimationSequenceEditorDocument()
{
    Widget.reset();
    PreviewController.reset();
    Sequence = nullptr;
}

bool FAnimationSequenceEditorDocument::Initialize(UEditorEngine* InEditorEngine, const FString& InSequencePath, UAnimSequence* InSequence)
{
    if (!InEditorEngine || !InSequence)
    {
        return false;
    }

    EditorEngine = InEditorEngine;
    SequencePath = FPaths::Normalize(InSequencePath);
    Sequence = InSequence;
    TabId = MakeAnimationSequenceTabId(SequencePath);
    TabLabel = MakeAnimationSequenceTabLabel(SequencePath);
    bDirty = false;
    LastNotifyValidationStatusText.clear();

    PreviewController = std::make_unique<FAnimationSequencePreviewController>();
    PreviewController->Initialize(InEditorEngine, SequencePath, Sequence);
    EditorState.InitializeFromSequence(
        Sequence,
        PreviewController->GetLength(),
        PreviewController->IsLooping(),
        PreviewController->GetPlayRate());
    SyncEditorState();

    Widget = std::make_unique<FAnimationSequenceEditorWidget>();
    Widget->Initialize(InEditorEngine);
    Widget->BindDocumentContext(this, SequencePath, Sequence, PreviewController.get(), &EditorState);
    return true;
}

void FAnimationSequenceEditorDocument::Tick(float DeltaTime)
{
    if (PreviewController)
    {
        PreviewController->Tick(DeltaTime);
    }

    SyncEditorState();
}

void FAnimationSequenceEditorDocument::RenderEmbedded(float DeltaTime)
{
    // The render pipeline ticks the active document before rendering its preview
    // viewport. We synchronize once more here so the ImGui panels reflect the
    // latest playback time even when the document has no active preview scene.
    SyncEditorState();

    if (Widget)
    {
        Widget->Render(DeltaTime);
    }
    else
    {
        ImGui::TextDisabled("Animation Sequence editor widget is unavailable.");
    }
}

void FAnimationSequenceEditorDocument::RenderToolbar()
{
    ImGui::TextDisabled("Animation Sequence");
    ImGui::SameLine();
    ImGui::Text("%s%s", TabLabel.c_str(), bDirty ? " *" : "");
}

void FAnimationSequenceEditorDocument::BuildCommandList(FEditorCommandList& OutCommands)
{
    OutCommands.Clear();

    OutCommands.MapAction(
        EEditorCommandId::Save,
        { static_cast<int32>(ImGuiKey_S), true, false, false },
        [this]()
        {
            Save();
        },
        [this]()
        {
            return CanSave();
        });

    OutCommands.MapAction(
        EEditorCommandId::DeleteSelection,
        { static_cast<int32>(ImGuiKey_Delete), false, false, false },
        [this]()
        {
            DeleteSelectedNotify();
        },
        [this]()
        {
            return GetSelectedNotify() != nullptr;
        });

    OutCommands.MapAction(
        EEditorCommandId::DuplicateSelection,
        { static_cast<int32>(ImGuiKey_D), true, false, false },
        [this]()
        {
            DuplicateSelectedNotify();
        },
        [this]()
        {
            return GetSelectedNotify() != nullptr;
        });

    OutCommands.MapAction(
        EEditorCommandId::CopySelection,
        { static_cast<int32>(ImGuiKey_C), true, false, false },
        [this]()
        {
            CopySelectedNotify();
        },
        [this]()
        {
            return GetSelectedNotify() != nullptr;
        });

    OutCommands.MapAction(
        EEditorCommandId::PasteSelection,
        { static_cast<int32>(ImGuiKey_V), true, false, false },
        [this]()
        {
            const int32 TargetTrackIndex = EditorState.SelectedNotifyTrackIndex >= 0 ? EditorState.SelectedNotifyTrackIndex : 0;
            PasteNotifyToTrackAtTime(TargetTrackIndex, EditorState.CurrentTime);
        },
        [this]()
        {
            return CanPasteNotify();
        });
}

FSceneViewport* FAnimationSequenceEditorDocument::GetSceneViewport()
{
    return PreviewController ? PreviewController->GetSceneViewport() : nullptr;
}

const FSceneViewport* FAnimationSequenceEditorDocument::GetSceneViewport() const
{
    return PreviewController ? PreviewController->GetSceneViewport() : nullptr;
}

bool FAnimationSequenceEditorDocument::GetSkeletalPreviewDebugSettings(FEditorSkeletalPreviewDebugSettings& OutSettings) const
{
    FSceneViewport* SceneViewport = PreviewController ? PreviewController->GetSceneViewport() : nullptr;
    FSkeletalMeshViewportClient* Client = SceneViewport ? static_cast<FSkeletalMeshViewportClient*>(SceneViewport->GetClient()) : nullptr;
    USkeletalMeshComponent* PreviewComponent = PreviewController ? PreviewController->GetPreviewComponent() : nullptr;
    if (!(Client && PreviewComponent && Widget))
    {
        return false;
    }

    const FSkeletalViewerShowFlags& ShowFlags = Client->GetShowFlags();
    OutSettings.SkeletalMeshComponent = PreviewComponent;
    OutSettings.bShowBones = ShowFlags.bShowBones;
    OutSettings.bShowOnlySelectedBone = ShowFlags.bShowOnlySelectedBone;
    OutSettings.bShowSelectedBoneWeight = ShowFlags.bShowSelectedBoneWeight;
    OutSettings.SelectedBoneIndex = Widget->GetSelectedPreviewBoneIndex();
    return true;
}

bool FAnimationSequenceEditorDocument::CanSave() const
{
    return AnimationSequenceViewer::IsLiveObject(Sequence) && bDirty;
}

bool FAnimationSequenceEditorDocument::Save()
{
    if (!AnimationSequenceViewer::IsLiveObject(Sequence))
    {
        LastNotifyValidationStatusText = "Save failed: animation sequence is unavailable.";
        return false;
    }

    const FAnimNotifyValidationReport ValidationReport = BuildDocumentNotifyValidationReport();
    if (!FResourceManager::Get().SaveAnimSequence(SequencePath, Sequence))
    {
        LastNotifyValidationStatusText = "Save failed: resource manager rejected the sequence write.";
        return false;
    }

    bDirty = false;
    if (ValidationReport.HasAnyIssues())
    {
        LastNotifyValidationStatusText = "Saved with " + FormatAnimNotifyValidationSummary(ValidationReport) + ".";
    }
    else
    {
        LastNotifyValidationStatusText = "Saved with no notify validation issues.";
    }
    return true;
}

FAnimNotifyValidationReport FAnimationSequenceEditorDocument::BuildSelectedNotifyValidationReport() const
{
    const FAnimNotifyEvent* SelectedNotify = GetSelectedNotify();
    if (!SelectedNotify)
    {
        return FAnimNotifyValidationReport();
    }

    FAnimNotifyValidationContext Context;
    Context.PreviewController = PreviewController.get();
    FAnimNotifyValidationReport Report = ValidateAnimNotifyEvent(
        *SelectedNotify,
        EditorState.SelectedNotifyTrackIndex,
        EditorState.SelectedNotifyEventIndex,
        Context);

    const FAnimNotifyValidationReport DocumentReport = ValidateAnimNotifyDocument(Sequence, Context);
    for (const FAnimNotifyValidationIssue& Issue : DocumentReport.Issues)
    {
        if (Issue.Field == EAnimNotifyValidationField::General &&
            Issue.StableId == SelectedNotify->StableId &&
            Issue.TrackIndex == EditorState.SelectedNotifyTrackIndex &&
            Issue.EventIndex == EditorState.SelectedNotifyEventIndex)
        {
            Report.AddIssue(Issue);
        }
    }

    return Report;
}

FAnimNotifyValidationReport FAnimationSequenceEditorDocument::BuildDocumentNotifyValidationReport() const
{
    FAnimNotifyValidationContext Context;
    Context.PreviewController = PreviewController.get();
    return ValidateAnimNotifyDocument(Sequence, Context);
}

int32 FAnimationSequenceEditorDocument::GetNotifyTrackCount() const
{
    const TArray<FAnimNotifyTrack>* Tracks = AnimationSequenceViewer::GetSequenceNotifyTracks(Sequence);
    return Tracks ? static_cast<int32>(Tracks->size()) : 0;
}

const FAnimNotifyTrack* FAnimationSequenceEditorDocument::GetNotifyTrack(int32 TrackIndex) const
{
    const TArray<FAnimNotifyTrack>* Tracks = AnimationSequenceViewer::GetSequenceNotifyTracks(Sequence);
    if (!Tracks || TrackIndex < 0 || TrackIndex >= static_cast<int32>(Tracks->size()))
    {
        return nullptr;
    }

    return &(*Tracks)[TrackIndex];
}

FAnimNotifyTrack* FAnimationSequenceEditorDocument::GetNotifyTrack(int32 TrackIndex)
{
    TArray<FAnimNotifyTrack>* Tracks = AnimationSequenceViewer::GetSequenceNotifyTracks(Sequence);
    if (!Tracks || TrackIndex < 0 || TrackIndex >= static_cast<int32>(Tracks->size()))
    {
        return nullptr;
    }

    return &(*Tracks)[TrackIndex];
}

bool FAnimationSequenceEditorDocument::HasSelectedNotify() const
{
    return GetSelectedNotify() != nullptr;
}

int32 FAnimationSequenceEditorDocument::AddNotifyTrack()
{
    TArray<FAnimNotifyTrack>* Tracks = AnimationSequenceViewer::GetSequenceNotifyTracks(Sequence);
    if (!Tracks)
    {
        return -1;
    }

    const int32 NewTrackIndex = static_cast<int32>(Tracks->size());
    Tracks->push_back(MakeDefaultNotifyTrack(NewTrackIndex));
    MarkDirty();
    return NewTrackIndex;
}

bool FAnimationSequenceEditorDocument::RenameNotifyTrack(int32 TrackIndex, const FName& NewName)
{
    FAnimNotifyTrack* Track = GetNotifyTrack(TrackIndex);
    if (!Track)
    {
        return false;
    }

    const FString NormalizedName = NewName.ToString();
    if (NormalizedName.empty())
    {
        return false;
    }

    if (Track->TrackName == NewName)
    {
        return true;
    }

    Track->TrackName = NewName;
    MarkDirty();
    return true;
}

bool FAnimationSequenceEditorDocument::DeleteNotifyTrack(int32 TrackIndex)
{
    TArray<FAnimNotifyTrack>* Tracks = AnimationSequenceViewer::GetSequenceNotifyTracks(Sequence);
    if (!Tracks || TrackIndex < 0 || TrackIndex >= static_cast<int32>(Tracks->size()))
    {
        return false;
    }

    FAnimNotifyTrack& Track = (*Tracks)[TrackIndex];
    if (!Track.Events.empty())
    {
        return false;
    }

    Tracks->erase(Tracks->begin() + TrackIndex);

    auto RemapOrClearTrackIndex = [TrackIndex](int32& Index)
    {
        if (Index == TrackIndex)
        {
            Index = -1;
        }
        else if (Index > TrackIndex)
        {
            --Index;
        }
    };

    RemapOrClearTrackIndex(EditorState.SelectedNotifyTrackIndex);
    RemapOrClearTrackIndex(EditorState.HoveredNotifyTrackIndex);
    RemapOrClearTrackIndex(EditorState.DraggedNotifyTrackIndex);

    if (EditorState.SelectedNotifyTrackIndex < 0)
    {
        EditorState.SelectedNotifyEventIndex = -1;
    }
    if (EditorState.HoveredNotifyTrackIndex < 0)
    {
        EditorState.HoveredNotifyEventIndex = -1;
    }
    if (EditorState.DraggedNotifyTrackIndex < 0)
    {
        EditorState.DraggedNotifyEventIndex = -1;
        EditorState.bDraggingNotify = false;
        EditorState.ActiveNotifyDragMode = EAnimNotifyDragMode::None;
        EditorState.DraggedNotifyGrabOffsetTime = 0.0f;
    }

    RemapSequencerRowIdAfterDelete(EditorState.HoveredSequencerRowId, TrackIndex);
    RemapSequencerRowIdAfterDelete(EditorState.FocusedSequencerRowId, TrackIndex);

    ValidateNotifySelection();
    MarkDirty();
    return true;
}

bool FAnimationSequenceEditorDocument::MoveNotifyTrack(int32 FromIndex, int32 ToIndex)
{
    TArray<FAnimNotifyTrack>* Tracks = AnimationSequenceViewer::GetSequenceNotifyTracks(Sequence);
    if (!Tracks)
    {
        return false;
    }

    const int32 TrackCount = static_cast<int32>(Tracks->size());
    if (FromIndex < 0 || ToIndex < 0 || FromIndex >= TrackCount || ToIndex >= TrackCount || FromIndex == ToIndex)
    {
        return false;
    }

    if (FromIndex < ToIndex)
    {
        std::rotate(Tracks->begin() + FromIndex, Tracks->begin() + FromIndex + 1, Tracks->begin() + ToIndex + 1);
    }
    else
    {
        std::rotate(Tracks->begin() + ToIndex, Tracks->begin() + FromIndex, Tracks->begin() + FromIndex + 1);
    }

    RemapTrackIndexAfterMove(EditorState.SelectedNotifyTrackIndex, FromIndex, ToIndex);
    RemapTrackIndexAfterMove(EditorState.HoveredNotifyTrackIndex, FromIndex, ToIndex);
    RemapTrackIndexAfterMove(EditorState.DraggedNotifyTrackIndex, FromIndex, ToIndex);
    RemapSequencerRowIdAfterMove(EditorState.HoveredSequencerRowId, FromIndex, ToIndex);
    RemapSequencerRowIdAfterMove(EditorState.FocusedSequencerRowId, FromIndex, ToIndex);

    ValidateNotifySelection();
    MarkDirty();
    return true;
}

const FAnimNotifyEvent* FAnimationSequenceEditorDocument::GetSelectedNotify() const
{
    const_cast<FAnimationSequenceEditorDocument*>(this)->ValidateNotifySelection();

    const FAnimNotifyTrack* Track = GetNotifyTrack(EditorState.SelectedNotifyTrackIndex);
    if (!Track)
    {
        return nullptr;
    }

    if (EditorState.SelectedNotifyEventIndex < 0 ||
        EditorState.SelectedNotifyEventIndex >= static_cast<int32>(Track->Events.size()))
    {
        return nullptr;
    }

    return &Track->Events[EditorState.SelectedNotifyEventIndex];
}

FAnimNotifyEvent* FAnimationSequenceEditorDocument::GetSelectedNotify()
{
    ValidateNotifySelection();

    FAnimNotifyTrack* Track = GetNotifyTrack(EditorState.SelectedNotifyTrackIndex);
    if (!Track)
    {
        return nullptr;
    }

    if (EditorState.SelectedNotifyEventIndex < 0 ||
        EditorState.SelectedNotifyEventIndex >= static_cast<int32>(Track->Events.size()))
    {
        return nullptr;
    }

    return &Track->Events[EditorState.SelectedNotifyEventIndex];
}

bool FAnimationSequenceEditorDocument::TryGetSelectedNotifySnapshot(FAnimNotifyEvent& OutNotify) const
{
    const FAnimNotifyEvent* SelectedNotify = GetSelectedNotify();
    if (!SelectedNotify)
    {
        return false;
    }

    OutNotify = *SelectedNotify;
    return true;
}

FString FAnimationSequenceEditorDocument::GetSelectedNotifyResolvedClassName() const
{
    const FAnimNotifyEvent* SelectedNotify = GetSelectedNotify();
    return SelectedNotify ? SelectedNotify->GetResolvedNotifyClassName() : FString();
}

bool FAnimationSequenceEditorDocument::AddNotifyAtTime(int32 TrackIndex, float TimeSeconds)
{
    TArray<FAnimNotifyTrack>* Tracks = AnimationSequenceViewer::GetSequenceNotifyTracks(Sequence);
    if (!Tracks || TrackIndex < 0)
    {
        return false;
    }

    EnsureTrackExists(*Tracks, TrackIndex);

    FAnimNotifyTrack& Track = (*Tracks)[TrackIndex];

    FAnimNotifyEvent NotifyEvent;
    NotifyEvent.StableId = FGuid::NewGuid();
    NotifyEvent.Name = FName("Notify");
    NotifyEvent.Time = EditorState.ClampOrSnapTime(TimeSeconds);
    NotifyEvent.Duration = 0.0f;
    NotifyEvent.Color = FColor(0.9490196f, 0.61960787f, 0.23921569f, 1.0f);
    NotifyEvent.EventType = EAnimNotifyEventType::Notify;
    NotifyEvent.NotifyClassName = GetDefaultAnimNotifyClassName(NotifyEvent.EventType);

    Track.Events.push_back(NotifyEvent);
    SortNotifyTrackEvents(Track);

    const int32 NewEventIndex = FindNotifyEventIndexById(Track, NotifyEvent.StableId);
    SelectNotify(TrackIndex, NewEventIndex);
    MarkDirty();
    return NewEventIndex >= 0;
}

bool FAnimationSequenceEditorDocument::DuplicateSelectedNotify()
{
    FAnimNotifyTrack* Track = GetNotifyTrack(EditorState.SelectedNotifyTrackIndex);
    const FAnimNotifyEvent* SelectedNotify = GetSelectedNotify();
    if (!Track || !SelectedNotify)
    {
        return false;
    }

    FAnimNotifyEvent DuplicatedNotify = *SelectedNotify;
    DuplicatedNotify.StableId = FGuid::NewGuid();
    DuplicatedNotify.Time =
        EditorState.ClampOrSnapTime(SelectedNotify->Time + GetNotifyDuplicateTimeOffset(EditorState));

    const int32 NewEventIndex = InsertNotifyEventSorted(*Track, DuplicatedNotify);
    SelectNotify(EditorState.SelectedNotifyTrackIndex, NewEventIndex);
    MarkDirty();
    return NewEventIndex >= 0;
}

bool FAnimationSequenceEditorDocument::CopySelectedNotify()
{
    const FAnimNotifyEvent* SelectedNotify = GetSelectedNotify();
    if (!SelectedNotify)
    {
        NotifyClipboard = FNotifyClipboard();
        return false;
    }

    NotifyClipboard.bValid = true;
    NotifyClipboard.EventType = SelectedNotify->EventType;
    NotifyClipboard.Name = SelectedNotify->Name;
    NotifyClipboard.Time = SelectedNotify->Time;
    NotifyClipboard.Duration = SelectedNotify->Duration;
    NotifyClipboard.Color = SelectedNotify->Color;
    NotifyClipboard.NotifyClassName = SelectedNotify->NotifyClassName;
    NotifyClipboard.Payload = SelectedNotify->Payload;
    NotifyClipboard.SourceTrackIndex = EditorState.SelectedNotifyTrackIndex;
    return true;
}

bool FAnimationSequenceEditorDocument::CanPasteNotify() const
{
    return NotifyClipboard.bValid;
}

bool FAnimationSequenceEditorDocument::PasteNotifyToTrackAtTime(int32 TrackIndex, float TimeSeconds)
{
    TArray<FAnimNotifyTrack>* Tracks = AnimationSequenceViewer::GetSequenceNotifyTracks(Sequence);
    if (!Tracks || TrackIndex < 0 || !NotifyClipboard.bValid)
    {
        return false;
    }

    EnsureTrackExists(*Tracks, TrackIndex);
    FAnimNotifyTrack& Track = (*Tracks)[TrackIndex];

    FAnimNotifyEvent PastedNotify;
    PastedNotify.StableId = FGuid::NewGuid();
    PastedNotify.Name = NotifyClipboard.Name;
    PastedNotify.Time = EditorState.ClampOrSnapTime(TimeSeconds);
    PastedNotify.Duration = std::max(0.0f, NotifyClipboard.Duration);
    PastedNotify.Color = NotifyClipboard.Color;
    PastedNotify.EventType = NotifyClipboard.EventType;
    PastedNotify.NotifyClassName = NotifyClipboard.NotifyClassName.empty()
        ? GetDefaultAnimNotifyClassName(NotifyClipboard.EventType)
        : NotifyClipboard.NotifyClassName;
    PastedNotify.Payload = NotifyClipboard.Payload;

    const int32 NewEventIndex = InsertNotifyEventSorted(Track, PastedNotify);
    SelectNotify(TrackIndex, NewEventIndex);
    MarkDirty();
    return NewEventIndex >= 0;
}

bool FAnimationSequenceEditorDocument::MoveSelectedNotifyToTrack(int32 TargetTrackIndex)
{
    TArray<FAnimNotifyTrack>* Tracks = AnimationSequenceViewer::GetSequenceNotifyTracks(Sequence);
    if (!Tracks || TargetTrackIndex < 0)
    {
        return false;
    }

    const int32 SourceTrackIndex = EditorState.SelectedNotifyTrackIndex;
    EnsureTrackExists(*Tracks, TargetTrackIndex);
    FAnimNotifyTrack* SourceTrack = GetNotifyTrack(SourceTrackIndex);
    const FAnimNotifyEvent* SelectedNotify = GetSelectedNotify();
    if (!SourceTrack || !SelectedNotify)
    {
        return false;
    }

    if (TargetTrackIndex == SourceTrackIndex)
    {
        return true;
    }

    const int32 SourceEventIndex = EditorState.SelectedNotifyEventIndex;
    if (SourceEventIndex < 0 || SourceEventIndex >= static_cast<int32>(SourceTrack->Events.size()))
    {
        return false;
    }

    const FAnimNotifyEvent MovedNotify = *SelectedNotify;
    SourceTrack->Events.erase(SourceTrack->Events.begin() + SourceEventIndex);
    FAnimNotifyTrack& TargetTrack = (*Tracks)[TargetTrackIndex];
    const int32 NewEventIndex = InsertNotifyEventSorted(TargetTrack, MovedNotify);
    SelectNotify(TargetTrackIndex, NewEventIndex);
    MarkDirty();
    return NewEventIndex >= 0;
}

bool FAnimationSequenceEditorDocument::DeleteSelectedNotify()
{
    ValidateNotifySelection();

    FAnimNotifyTrack* Track = GetNotifyTrack(EditorState.SelectedNotifyTrackIndex);
    if (!Track)
    {
        return false;
    }

    const int32 EventIndex = EditorState.SelectedNotifyEventIndex;
    if (EventIndex < 0 || EventIndex >= static_cast<int32>(Track->Events.size()))
    {
        return false;
    }

    Track->Events.erase(Track->Events.begin() + EventIndex);
    if (Track->Events.empty())
    {
        ClearNotifySelection();
    }
    else
    {
        SelectNotify(EditorState.SelectedNotifyTrackIndex, std::min(EventIndex, static_cast<int32>(Track->Events.size()) - 1));
    }

    MarkDirty();
    return true;
}

bool FAnimationSequenceEditorDocument::SetSelectedNotifyTime(float TimeSeconds, bool bApplySnap)
{
    FAnimNotifyTrack* Track = GetNotifyTrack(EditorState.SelectedNotifyTrackIndex);
    FAnimNotifyEvent* NotifyEvent = GetSelectedNotify();
    if (!Track || !NotifyEvent)
    {
        return false;
    }

    const FGuid StableId = NotifyEvent->StableId;
    NotifyEvent->Time = bApplySnap ? EditorState.ClampOrSnapTime(TimeSeconds) : EditorState.ClampTime(TimeSeconds);
    SortNotifyTrackEvents(*Track);
    const int32 NewEventIndex = FindNotifyEventIndexById(*Track, StableId);
    SelectNotify(EditorState.SelectedNotifyTrackIndex, NewEventIndex);
    MarkDirty();
    return NewEventIndex >= 0;
}

bool FAnimationSequenceEditorDocument::SetSelectedNotifyEndTime(float EndTimeSeconds, bool bApplySnap)
{
    FAnimNotifyEvent* NotifyEvent = GetSelectedNotify();
    if (!NotifyEvent)
    {
        return false;
    }

    const float ClampedEndTime = bApplySnap
        ? EditorState.ClampOrSnapTime(EndTimeSeconds)
        : EditorState.ClampTime(EndTimeSeconds);
    NotifyEvent->Duration = std::max(0.0f, ClampedEndTime - NotifyEvent->Time);
    MarkDirty();
    return true;
}

bool FAnimationSequenceEditorDocument::SetSelectedNotifyDuration(float DurationSeconds)
{
    FAnimNotifyEvent* NotifyEvent = GetSelectedNotify();
    if (!NotifyEvent)
    {
        return false;
    }

    NotifyEvent->Duration = std::max(0.0f, DurationSeconds);
    MarkDirty();
    return true;
}

bool FAnimationSequenceEditorDocument::SetSelectedNotifyName(const FName& Name)
{
    FAnimNotifyEvent* NotifyEvent = GetSelectedNotify();
    if (!NotifyEvent)
    {
        return false;
    }

    NotifyEvent->Name = Name;
    MarkDirty();
    return true;
}

bool FAnimationSequenceEditorDocument::SetSelectedNotifyColor(const FColor& Color)
{
    FAnimNotifyEvent* NotifyEvent = GetSelectedNotify();
    if (!NotifyEvent)
    {
        return false;
    }

    NotifyEvent->Color = Color;
    MarkDirty();
    return true;
}

bool FAnimationSequenceEditorDocument::SetSelectedNotifyType(EAnimNotifyEventType EventType)
{
    FAnimNotifyEvent* NotifyEvent = GetSelectedNotify();
    if (!NotifyEvent)
    {
        return false;
    }

    NotifyEvent->EventType = EventType;
    const FString DefaultClassName = GetDefaultAnimNotifyClassName(EventType);
    const UClass* SelectedClass = FindRegisteredClassByName(NotifyEvent->NotifyClassName);
    const bool bRequiresStateClass = EventType == EAnimNotifyEventType::NotifyState;
    const bool bSelectedClassMatchesType =
        SelectedClass &&
        (bRequiresStateClass
            ? SelectedClass->IsA(UAnimNotifyState::StaticClass())
            : (SelectedClass->IsA(UAnimNotify::StaticClass()) && !SelectedClass->IsA(UAnimNotifyState::StaticClass())));

    if (NotifyEvent->NotifyClassName.empty() ||
        NotifyEvent->NotifyClassName == "UAnimNotify" ||
        NotifyEvent->NotifyClassName == "UAnimNotifyState" ||
        !bSelectedClassMatchesType)
    {
        NotifyEvent->NotifyClassName = DefaultClassName;
    }

    MarkDirty();
    return true;
}

bool FAnimationSequenceEditorDocument::SetSelectedNotifyClassName(const FString& NotifyClassName)
{
    FAnimNotifyEvent* NotifyEvent = GetSelectedNotify();
    if (!NotifyEvent)
    {
        return false;
    }

    NotifyEvent->NotifyClassName = NotifyClassName.empty()
        ? GetDefaultAnimNotifyClassName(NotifyEvent->EventType)
        : NotifyClassName;
    MarkDirty();
    return true;
}

bool FAnimationSequenceEditorDocument::SetSelectedNotifyPayload(const FString& Payload)
{
    FAnimNotifyEvent* NotifyEvent = GetSelectedNotify();
    if (!NotifyEvent)
    {
        return false;
    }

    NotifyEvent->Payload = Payload;
    MarkDirty();
    return true;
}

FAnimNotifyPayloadParser FAnimationSequenceEditorDocument::GetSelectedNotifyPayloadParser() const
{
    const FAnimNotifyEvent* NotifyEvent = GetSelectedNotify();
    return NotifyEvent ? FAnimNotifyPayloadParser(NotifyEvent->Payload) : FAnimNotifyPayloadParser();
}

bool FAnimationSequenceEditorDocument::SetSelectedNotifyPayloadStringValue(const FString& Key, const FString& Value)
{
    FAnimNotifyPayloadParser Payload = GetSelectedNotifyPayloadParser();
    const TArray<FString>& LookupKeys = AnimNotifySemanticFieldNames::GetLookupKeys(Key);
    if (!LookupKeys.empty())
    {
        Payload.RemoveAny(LookupKeys);
    }
    else
    {
        Payload.RemoveValue(Key);
    }
    Payload.SetString(Key, Value);
    return SetSelectedNotifyPayload(Payload.SerializeCanonical());
}

bool FAnimationSequenceEditorDocument::SetSelectedNotifyPayloadNameValue(const FString& Key, const FName& Value)
{
    FAnimNotifyPayloadParser Payload = GetSelectedNotifyPayloadParser();
    const TArray<FString>& LookupKeys = AnimNotifySemanticFieldNames::GetLookupKeys(Key);
    if (!LookupKeys.empty())
    {
        Payload.RemoveAny(LookupKeys);
    }
    else
    {
        Payload.RemoveValue(Key);
    }
    Payload.SetName(Key, Value);
    return SetSelectedNotifyPayload(Payload.SerializeCanonical());
}

bool FAnimationSequenceEditorDocument::SetSelectedNotifyPayloadFloatValue(const FString& Key, float Value)
{
    FAnimNotifyPayloadParser Payload = GetSelectedNotifyPayloadParser();
    const TArray<FString>& LookupKeys = AnimNotifySemanticFieldNames::GetLookupKeys(Key);
    if (!LookupKeys.empty())
    {
        Payload.RemoveAny(LookupKeys);
    }
    else
    {
        Payload.RemoveValue(Key);
    }
    Payload.SetFloat(Key, Value);
    return SetSelectedNotifyPayload(Payload.SerializeCanonical());
}

bool FAnimationSequenceEditorDocument::SetSelectedNotifyPayloadBoolValue(const FString& Key, bool Value)
{
    FAnimNotifyPayloadParser Payload = GetSelectedNotifyPayloadParser();
    const TArray<FString>& LookupKeys = AnimNotifySemanticFieldNames::GetLookupKeys(Key);
    if (!LookupKeys.empty())
    {
        Payload.RemoveAny(LookupKeys);
    }
    else
    {
        Payload.RemoveValue(Key);
    }
    Payload.SetBool(Key, Value);
    return SetSelectedNotifyPayload(Payload.SerializeCanonical());
}

bool FAnimationSequenceEditorDocument::ClearSelectedNotifyPayloadValue(const FString& Key)
{
    FAnimNotifyPayloadParser Payload = GetSelectedNotifyPayloadParser();
    const TArray<FString>& LookupKeys = AnimNotifySemanticFieldNames::GetLookupKeys(Key);
    if (!LookupKeys.empty())
    {
        Payload.RemoveAny(LookupKeys);
    }
    else
    {
        Payload.RemoveValue(Key);
    }
    return SetSelectedNotifyPayload(Payload.SerializeCanonical());
}

bool FAnimationSequenceEditorDocument::SelectNotifyByStableId(int32 TrackIndex, const FGuid& StableId)
{
    const FAnimNotifyTrack* Track = GetNotifyTrack(TrackIndex);
    if (!Track)
    {
        return false;
    }

    const int32 EventIndex = FindNotifyEventIndexById(*Track, StableId);
    if (EventIndex < 0)
    {
        return false;
    }

    SelectNotify(TrackIndex, EventIndex);
    return true;
}

void FAnimationSequenceEditorDocument::SelectNotify(int32 TrackIndex, int32 EventIndex)
{
    EditorState.SelectedCurveIndex = -1;
    EditorState.HoveredCurveIndex = -1;
    EditorState.SelectedNotifyTrackIndex = TrackIndex;
    EditorState.SelectedNotifyEventIndex = EventIndex;
    EditorState.HoveredNotifyTrackIndex = TrackIndex;
    EditorState.HoveredNotifyEventIndex = EventIndex;
}

void FAnimationSequenceEditorDocument::ClearNotifySelection()
{
    EditorState.SelectedNotifyTrackIndex = -1;
    EditorState.SelectedNotifyEventIndex = -1;
    EditorState.HoveredNotifyTrackIndex = -1;
    EditorState.HoveredNotifyEventIndex = -1;
    EditorState.DraggedNotifyTrackIndex = -1;
    EditorState.DraggedNotifyEventIndex = -1;
    EditorState.bDraggingNotify = false;
    EditorState.ActiveNotifyDragMode = EAnimNotifyDragMode::None;
    EditorState.DraggedNotifyGrabOffsetTime = 0.0f;
}

void FAnimationSequenceEditorDocument::MarkDirty()
{
    bDirty = true;
}

void FAnimationSequenceEditorDocument::SyncEditorState()
{
    if (!PreviewController)
    {
        return;
    }

    // PreviewController owns runtime playback authority. EditorState mirrors
    // that data for timeline rendering and UI-only interactions.
    EditorState.SequenceLength = PreviewController->GetLength();
    EditorState.DisplayFrameRate = PreviewController->GetFrameRate();
    EditorState.TotalFrames = PreviewController->GetFrameCount();
    EditorState.bLoop = PreviewController->IsLooping();
    EditorState.PlayRate = PreviewController->GetPlayRate();
    EditorState.SetPlaybackRange(
        PreviewController->GetPlaybackRangeStart(),
        PreviewController->GetPlaybackRangeEnd());
    EditorState.SetCurrentTime(PreviewController->GetCurrentTime(), false);
    EditorState.SetVisibleRange(EditorState.VisibleTimeStart, EditorState.VisibleTimeEnd);

    const UAnimDataModel* DataModel = AnimationSequenceViewer::GetValidAnimDataModel(Sequence);
    const int32 CurveCount = DataModel ? static_cast<int32>(DataModel->CurveData.FloatCurves.size()) : 0;
    if (EditorState.SelectedCurveIndex >= CurveCount)
    {
        EditorState.SelectedCurveIndex = -1;
    }
    if (EditorState.HoveredCurveIndex >= CurveCount)
    {
        EditorState.HoveredCurveIndex = -1;
    }

    ValidateNotifySelection();
}

void FAnimationSequenceEditorDocument::ValidateNotifySelection()
{
    const TArray<FAnimNotifyTrack>* Tracks = AnimationSequenceViewer::GetSequenceNotifyTracks(Sequence);
    if (!Tracks || Tracks->empty())
    {
        ClearNotifySelection();
        return;
    }

    if (EditorState.SelectedNotifyTrackIndex < 0 ||
        EditorState.SelectedNotifyTrackIndex >= static_cast<int32>(Tracks->size()))
    {
        ClearNotifySelection();
        return;
    }

    const FAnimNotifyTrack& Track = (*Tracks)[EditorState.SelectedNotifyTrackIndex];
    if (EditorState.SelectedNotifyEventIndex < 0 ||
        EditorState.SelectedNotifyEventIndex >= static_cast<int32>(Track.Events.size()))
    {
        EditorState.SelectedNotifyEventIndex = -1;
    }

    if (EditorState.HoveredNotifyTrackIndex >= static_cast<int32>(Tracks->size()))
    {
        EditorState.HoveredNotifyTrackIndex = -1;
        EditorState.HoveredNotifyEventIndex = -1;
    }

    if (EditorState.DraggedNotifyTrackIndex >= static_cast<int32>(Tracks->size()))
    {
        EditorState.DraggedNotifyTrackIndex = -1;
        EditorState.DraggedNotifyEventIndex = -1;
        EditorState.bDraggingNotify = false;
        EditorState.ActiveNotifyDragMode = EAnimNotifyDragMode::None;
        EditorState.DraggedNotifyGrabOffsetTime = 0.0f;
    }
}
