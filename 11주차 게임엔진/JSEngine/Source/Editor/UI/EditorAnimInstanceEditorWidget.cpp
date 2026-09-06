#include "Editor/UI/EditorAnimInstanceEditorWidget.h"

#include "Animation/AnimInstanceAsset.h"
#include "Asset/AnimSequenceAssetLoader.h"
#include "Core/Paths.h"
#include "Core/ResourceManager.h"
#include "Editor/EditorEngine.h"
#include "ImGui/imgui.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace
{
    FString MakeStateName(int32 Index)
    {
        return FString("State") + std::to_string(Index);
    }

    int32 FindStateIndex(const UAnimInstanceAsset* Asset, const FName& Name)
    {
        if (!Asset)
        {
            return -1;
        }

        for (int32 Index = 0; Index < static_cast<int32>(Asset->States.size()); ++Index)
        {
            if (Asset->States[Index].Name == Name)
            {
                return Index;
            }
        }
        return -1;
    }

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

    const char* ToConditionLabel(EAnimTransitionConditionType Type)
    {
        switch (Type)
        {
        case EAnimTransitionConditionType::BoolEquals:
            return "Bool Equals";
        case EAnimTransitionConditionType::FloatGreater:
            return "Float Greater";
        case EAnimTransitionConditionType::FloatLessEqual:
            return "Float Less Equal";
        case EAnimTransitionConditionType::IntEquals:
            return "Int Equals";
        case EAnimTransitionConditionType::Trigger:
            return "Trigger";
        case EAnimTransitionConditionType::StateFinished:
            return "State Finished";
        case EAnimTransitionConditionType::Native:
        default:
            return "Native";
        }
    }

    void InputFString(const char* Label, FString& Value, bool& bChanged)
    {
        char Buffer[512] = {};
        strncpy_s(Buffer, sizeof(Buffer), Value.c_str(), _TRUNCATE);
        if (ImGui::InputText(Label, Buffer, sizeof(Buffer)))
        {
            Value = Buffer;
            bChanged = true;
        }
    }

    void InputAnimSequencePath(UEditorEngine* EditorEngine, const char* Label, FString& Value, bool& bChanged)
    {
        const TArray<FString>* SequencePaths =
            EditorEngine ? &EditorEngine->GetAssetService().GetAnimSequenceAssetPaths() : nullptr;
        if (!SequencePaths || SequencePaths->empty())
        {
            InputFString(Label, Value, bChanged);
            return;
        }

        const FString Current = Value;
        if (ImGui::BeginCombo(Label, Current.empty() ? "<None>" : Current.c_str()))
        {
            if (ImGui::Selectable("<None>", Current.empty()))
            {
                Value.clear();
                bChanged = true;
            }

            for (const FString& Path : *SequencePaths)
            {
                const bool bSelected = Current == Path;
                if (ImGui::Selectable(Path.c_str(), bSelected))
                {
                    Value = Path;
                    bChanged = true;
                }
                if (bSelected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }

            ImGui::EndCombo();
        }
    }

    void InputFName(const char* Label, FName& Value, bool& bChanged)
    {
        FString Text = Value.ToString();
        InputFString(Label, Text, bChanged);
        if (bChanged)
        {
            Value = FName(Text);
        }
    }

    void DrawSectionHeader(const char* Label)
    {
        const ImVec2 Min = ImGui::GetCursorScreenPos();
        const float Height = ImGui::GetFrameHeight();
        const ImVec2 Max(Min.x + ImGui::GetContentRegionAvail().x, Min.y + Height);
        ImDrawList* DrawList = ImGui::GetWindowDrawList();
        DrawList->AddRectFilled(Min, Max, ImGui::GetColorU32(ImVec4(0.20f, 0.24f, 0.29f, 1.0f)), 4.0f);
        DrawList->AddRect(Min, Max, ImGui::GetColorU32(ImVec4(0.34f, 0.39f, 0.46f, 1.0f)), 4.0f);
        ImGui::SetCursorScreenPos(ImVec2(Min.x + 8.0f, Min.y + 3.0f));
        ImGui::TextUnformatted(Label);
        ImGui::SetCursorScreenPos(ImVec2(Min.x, Max.y + 4.0f));
    }

    ImVec2 CubicBezierPoint(const ImVec2& P0, const ImVec2& P1, const ImVec2& P2, const ImVec2& P3, float T)
    {
        const float U = 1.0f - T;
        const float TT = T * T;
        const float UU = U * U;
        const float UUU = UU * U;
        const float TTT = TT * T;
        return ImVec2(
            UUU * P0.x + 3.0f * UU * T * P1.x + 3.0f * U * TT * P2.x + TTT * P3.x,
            UUU * P0.y + 3.0f * UU * T * P1.y + 3.0f * U * TT * P2.y + TTT * P3.y);
    }

    float DistancePointToSegment(const ImVec2& P, const ImVec2& A, const ImVec2& B)
    {
        const float ABX = B.x - A.x;
        const float ABY = B.y - A.y;
        const float APX = P.x - A.x;
        const float APY = P.y - A.y;
        const float LengthSq = ABX * ABX + ABY * ABY;
        if (LengthSq <= 0.0001f)
        {
            const float DX = P.x - A.x;
            const float DY = P.y - A.y;
            return std::sqrt(DX * DX + DY * DY);
        }

        const float T = std::clamp((APX * ABX + APY * ABY) / LengthSq, 0.0f, 1.0f);
        const float ClosestX = A.x + ABX * T;
        const float ClosestY = A.y + ABY * T;
        const float DX = P.x - ClosestX;
        const float DY = P.y - ClosestY;
        return std::sqrt(DX * DX + DY * DY);
    }

    float DistancePointToBezier(const ImVec2& P, const ImVec2& P0, const ImVec2& P1, const ImVec2& P2, const ImVec2& P3)
    {
        float Best = FLT_MAX;
        ImVec2 Prev = P0;
        for (int32 Step = 1; Step <= 24; ++Step)
        {
            const float T = static_cast<float>(Step) / 24.0f;
            const ImVec2 Next = CubicBezierPoint(P0, P1, P2, P3, T);
            Best = std::min(Best, DistancePointToSegment(P, Prev, Next));
            Prev = Next;
        }
        return Best;
    }

    ImVec2 Add(const ImVec2& A, const ImVec2& B)
    {
        return ImVec2(A.x + B.x, A.y + B.y);
    }

    ImVec2 Sub(const ImVec2& A, const ImVec2& B)
    {
        return ImVec2(A.x - B.x, A.y - B.y);
    }

    ImVec2 Mul(const ImVec2& V, float Scalar)
    {
        return ImVec2(V.x * Scalar, V.y * Scalar);
    }

    float Length(const ImVec2& V)
    {
        return std::sqrt(V.x * V.x + V.y * V.y);
    }

    ImVec2 Normalize(const ImVec2& V)
    {
        const float L = Length(V);
        return L > 0.0001f ? ImVec2(V.x / L, V.y / L) : ImVec2(1.0f, 0.0f);
    }

    ImVec2 Perp(const ImVec2& V)
    {
        return ImVec2(-V.y, V.x);
    }

    ImVec2 RectCenter(const ImVec2& Min, const ImVec2& Max)
    {
        return ImVec2((Min.x + Max.x) * 0.5f, (Min.y + Max.y) * 0.5f);
    }

    ImVec2 RectEdgePointToward(const ImVec2& RectMin, const ImVec2& RectMax, const ImVec2& Target)
    {
        const ImVec2 Center = RectCenter(RectMin, RectMax);
        const ImVec2 Direction = Sub(Target, Center);
        if (std::abs(Direction.x) <= 0.0001f && std::abs(Direction.y) <= 0.0001f)
        {
            return ImVec2(RectMax.x, Center.y);
        }

        float BestT = FLT_MAX;
        if (std::abs(Direction.x) > 0.0001f)
        {
            const float TargetX = Direction.x > 0.0f ? RectMax.x : RectMin.x;
            const float T = (TargetX - Center.x) / Direction.x;
            if (T > 0.0f)
            {
                BestT = std::min(BestT, T);
            }
        }
        if (std::abs(Direction.y) > 0.0001f)
        {
            const float TargetY = Direction.y > 0.0f ? RectMax.y : RectMin.y;
            const float T = (TargetY - Center.y) / Direction.y;
            if (T > 0.0f)
            {
                BestT = std::min(BestT, T);
            }
        }

        if (BestT == FLT_MAX)
        {
            BestT = 0.0f;
        }

        return Add(Center, Mul(Direction, BestT));
    }

    int32 CountParallelTransitions(const UAnimInstanceAsset* Asset, const FName& A, const FName& B)
    {
        int32 Count = 0;
        for (const FAnimInstanceTransitionAssetData& Transition : Asset->Transitions)
        {
            if ((Transition.FromState == A && Transition.ToState == B) ||
                (Transition.FromState == B && Transition.ToState == A))
            {
                ++Count;
            }
        }
        return Count;
    }

    int32 GetParallelTransitionOrdinal(const UAnimInstanceAsset* Asset, int32 TransitionIndex)
    {
        const FAnimInstanceTransitionAssetData& Target = Asset->Transitions[TransitionIndex];
        int32 Ordinal = 0;
        for (int32 Index = 0; Index < TransitionIndex; ++Index)
        {
            const FAnimInstanceTransitionAssetData& Transition = Asset->Transitions[Index];
            if ((Transition.FromState == Target.FromState && Transition.ToState == Target.ToState) ||
                (Transition.FromState == Target.ToState && Transition.ToState == Target.FromState))
            {
                ++Ordinal;
            }
        }
        return Ordinal;
    }

    void DrawArrowHead(ImDrawList* DrawList, const ImVec2& Tip, const ImVec2& Direction, ImU32 Color, float Size)
    {
        const ImVec2 Dir = Normalize(Direction);
        const ImVec2 Side = Perp(Dir);
        const ImVec2 Base = Sub(Tip, Mul(Dir, Size));
        DrawList->AddTriangleFilled(
            Tip,
            Add(Base, Mul(Side, Size * 0.55f)),
            Sub(Base, Mul(Side, Size * 0.55f)),
            Color);
    }

    float ComputeFramesPerSecond(int32 Numerator, int32 Denominator)
    {
        if (Numerator <= 0 || Denominator <= 0)
        {
            return 0.0f;
        }

        return static_cast<float>(Numerator) / static_cast<float>(Denominator);
    }

    ImVec4 ToHintColor(EAnimInstanceStateContextSeverity Severity)
    {
        return Severity == EAnimInstanceStateContextSeverity::Warning
            ? ImVec4(0.92f, 0.63f, 0.32f, 1.0f)
            : ImVec4(0.65f, 0.74f, 0.88f, 1.0f);
    }

    const char* ToHintPrefix(EAnimInstanceStateContextSeverity Severity)
    {
        return Severity == EAnimInstanceStateContextSeverity::Warning ? "Warning" : "Info";
    }
}

void FEditorAnimInstanceEditorWidget::OpenAnimInstanceAsset(const FString& AnimInstancePath)
{
    CurrentPath = AnimInstancePath;
    CurrentAsset = FResourceManager::Get().LoadAnimInstanceAsset(CurrentPath);
    SelectedStateIndex = CurrentAsset && !CurrentAsset->States.empty() ? 0 : -1;
    SelectedTransitionIndex = -1;
    InvalidateSelectedStateSequenceContext();
    bDirty = false;
    bVisible = true;
}

void FEditorAnimInstanceEditorWidget::Render(float DeltaTime)
{
    (void)DeltaTime;

    if (!bVisible)
    {
        return;
    }

    FString Title = FString("Anim Instance Editor");
    if (!CurrentPath.empty())
    {
        Title += FString(" - ") + CurrentPath;
    }
    if (bDirty)
    {
        Title += " *";
    }

    bool bOpen = true;
    ImGui::SetNextWindowSize(ImVec2(920.0f, 620.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(Title.c_str(), &bOpen))
    {
        ImGui::End();
        bVisible = bOpen;
        return;
    }

    if (!CurrentAsset)
    {
        ImGui::TextDisabled("AnimInstance asset could not be loaded.");
        if (ImGui::Button("Reload"))
        {
            ReloadAsset();
        }
        ImGui::End();
        bVisible = bOpen;
        return;
    }

    DrawToolbar();
    ImGui::Separator();

    if (ImGui::BeginTable("##AnimInstanceEditorLayout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV))
    {
        ImGui::TableSetupColumn("Graph", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Details", ImGuiTableColumnFlags_WidthFixed, 300.0f);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        DrawGraph();
        ImGui::TableSetColumnIndex(1);
        DrawDetails();
        ImGui::EndTable();
    }

    ImGui::End();
    bVisible = bOpen;
}

void FEditorAnimInstanceEditorWidget::DrawToolbar()
{
    if (ImGui::Button("Save"))
    {
        SaveAsset();
    }
    ImGui::SameLine();
    if (ImGui::Button("Reload"))
    {
        ReloadAsset();
    }
    ImGui::SameLine();
    if (ImGui::Button("Add State"))
    {
        AddState();
    }
    ImGui::SameLine();
    if (ImGui::Button("Add Transition"))
    {
        AddTransition();
    }
    ImGui::SameLine();
    const bool bCanDeleteSelection = HasSelectedState() || HasSelectedTransition();
    if (!bCanDeleteSelection)
    {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button("Delete Selected"))
    {
        if (HasSelectedState())
        {
            DeleteSelectedState();
        }
        else
        {
            DeleteSelectedTransition();
        }
    }
    if (!bCanDeleteSelection)
    {
        ImGui::EndDisabled();
    }

    ImGui::SameLine();
    ImGui::TextDisabled("Entry: %s", CurrentAsset && CurrentAsset->EntryState.IsValid() ? CurrentAsset->EntryState.ToString().c_str() : "<None>");
}

void FEditorAnimInstanceEditorWidget::DrawGraph()
{
    if (!ImGui::BeginChild("##AnimInstanceGraphChild", ImVec2(0.0f, 0.0f), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
    {
        ImGui::EndChild();
        return;
    }

    const ImVec2 CanvasSize(
        std::max(1.0f, ImGui::GetContentRegionAvail().x),
        std::max(1.0f, ImGui::GetContentRegionAvail().y));
    const ImVec2 CanvasMin = ImGui::GetCursorScreenPos();
    const ImVec2 CanvasMax(CanvasMin.x + CanvasSize.x, CanvasMin.y + CanvasSize.y);
    const bool bCanvasHovered =
        ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) &&
        ImGui::IsMouseHoveringRect(CanvasMin, CanvasMax, true);
    ImDrawList* DrawList = ImGui::GetWindowDrawList();

    DrawList->AddRectFilled(CanvasMin, CanvasMax, ImGui::GetColorU32(ImVec4(0.12f, 0.13f, 0.15f, 1.0f)));
    DrawList->PushClipRect(CanvasMin, CanvasMax, true);

    const float GridStep = 32.0f;
    const ImU32 GridColor = ImGui::GetColorU32(ImVec4(0.23f, 0.24f, 0.27f, 0.45f));
    for (float X = CanvasMin.x; X < CanvasMax.x; X += GridStep)
    {
        DrawList->AddLine(ImVec2(X, CanvasMin.y), ImVec2(X, CanvasMax.y), GridColor);
    }
    for (float Y = CanvasMin.y; Y < CanvasMax.y; Y += GridStep)
    {
        DrawList->AddLine(ImVec2(CanvasMin.x, Y), ImVec2(CanvasMax.x, Y), GridColor);
    }

    const ImVec2 NodeSize(150.0f, 62.0f);
    for (int32 TransitionIndex = 0; TransitionIndex < static_cast<int32>(CurrentAsset->Transitions.size()); ++TransitionIndex)
    {
        const FAnimInstanceTransitionAssetData& Transition = CurrentAsset->Transitions[TransitionIndex];
        const int32 FromIndex = FindStateIndex(CurrentAsset, Transition.FromState);
        const int32 ToIndex = FindStateIndex(CurrentAsset, Transition.ToState);
        if (FromIndex < 0 || ToIndex < 0)
        {
            continue;
        }

        const FVector2& FromPos = CurrentAsset->States[FromIndex].EditorNodePosition;
        const FVector2& ToPos = CurrentAsset->States[ToIndex].EditorNodePosition;
        const ImVec2 FromMin(CanvasMin.x + FromPos.X, CanvasMin.y + FromPos.Y);
        const ImVec2 FromMax(FromMin.x + NodeSize.x, FromMin.y + NodeSize.y);
        const ImVec2 ToMin(CanvasMin.x + ToPos.X, CanvasMin.y + ToPos.Y);
        const ImVec2 ToMax(ToMin.x + NodeSize.x, ToMin.y + NodeSize.y);
        const bool bSelected = SelectedTransitionIndex == TransitionIndex;
        const ImU32 LineColor = ImGui::GetColorU32(bSelected ? ImVec4(1.0f, 0.86f, 0.42f, 1.0f) : ImVec4(0.78f, 0.78f, 0.76f, 0.92f));
        const float LineThickness = bSelected ? 3.4f : 2.0f;

        ImVec2 Start;
        ImVec2 End;
        ImVec2 ControlA;
        ImVec2 ControlB;

        if (FromIndex == ToIndex)
        {
            Start = ImVec2(FromMin.x, FromMin.y + NodeSize.y * 0.45f);
            End = ImVec2(FromMin.x, FromMin.y + NodeSize.y * 0.72f);
            const float LoopWidth = 40.0f + 8.0f * GetParallelTransitionOrdinal(CurrentAsset, TransitionIndex);
            ControlA = ImVec2(Start.x - LoopWidth, Start.y - 28.0f);
            ControlB = ImVec2(End.x - LoopWidth, End.y + 28.0f);
        }
        else
        {
            const ImVec2 FromCenter = RectCenter(FromMin, FromMax);
            const ImVec2 ToCenter = RectCenter(ToMin, ToMax);
            const ImVec2 Direction = Normalize(Sub(ToCenter, FromCenter));
            const bool bCanonicalOrder = FromIndex < ToIndex;
            const ImVec2 CanonicalDirection = bCanonicalOrder ? Direction : Mul(Direction, -1.0f);
            const ImVec2 Normal = Perp(CanonicalDirection);
            const int32 ParallelCount = CountParallelTransitions(CurrentAsset, Transition.FromState, Transition.ToState);
            const int32 ParallelOrdinal = GetParallelTransitionOrdinal(CurrentAsset, TransitionIndex);
            const float Offset = (static_cast<float>(ParallelOrdinal) - (static_cast<float>(ParallelCount) - 1.0f) * 0.5f) * 24.0f;
            const ImVec2 OffsetVector = Mul(Normal, Offset);
            Start = Add(RectEdgePointToward(FromMin, FromMax, ToCenter), OffsetVector);
            End = Add(RectEdgePointToward(ToMin, ToMax, FromCenter), OffsetVector);
            const float Distance = Length(Sub(End, Start));
            const float ControlLength = std::clamp(Distance * 0.22f, 36.0f, 110.0f);
            ControlA = Add(Start, Mul(Direction, ControlLength));
            ControlB = Sub(End, Mul(Direction, ControlLength));
        }

        DrawList->AddBezierCubic(Start, ControlA, ControlB, End, LineColor, LineThickness);

        const ImVec2 ArrowTip = CubicBezierPoint(Start, ControlA, ControlB, End, 0.86f);
        const ImVec2 ArrowPrev = CubicBezierPoint(Start, ControlA, ControlB, End, 0.80f);
        DrawArrowHead(DrawList, ArrowTip, Sub(ArrowTip, ArrowPrev), LineColor, bSelected ? 10.0f : 8.0f);

        if (bCanvasHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            const float Distance = DistancePointToBezier(ImGui::GetIO().MousePos, Start, ControlA, ControlB, End);
            if (Distance <= 8.0f)
            {
                SelectedTransitionIndex = TransitionIndex;
                SelectedStateIndex = -1;
            }
        }
    }

    for (int32 Index = 0; Index < static_cast<int32>(CurrentAsset->States.size()); ++Index)
    {
        FAnimInstanceStateAssetData& State = CurrentAsset->States[Index];
        ImVec2 NodeMin(CanvasMin.x + State.EditorNodePosition.X, CanvasMin.y + State.EditorNodePosition.Y);
        ImVec2 NodeMax(NodeMin.x + NodeSize.x, NodeMin.y + NodeSize.y);
        const bool bSelected = SelectedStateIndex == Index && SelectedTransitionIndex < 0;
        const bool bEntry = State.Name == CurrentAsset->EntryState;
        const ImU32 NodeColor = ImGui::GetColorU32(bSelected ? ImVec4(0.25f, 0.34f, 0.42f, 1.0f) : ImVec4(0.18f, 0.21f, 0.25f, 1.0f));
        const ImU32 BorderColor = ImGui::GetColorU32(bEntry ? ImVec4(0.85f, 0.74f, 0.44f, 1.0f) : ImVec4(0.36f, 0.40f, 0.46f, 1.0f));

        DrawList->AddRectFilled(NodeMin, NodeMax, NodeColor, 6.0f);
        DrawList->AddRect(NodeMin, NodeMax, BorderColor, 6.0f, 0, bSelected ? 2.5f : 1.5f);
        DrawList->AddText(ImVec2(NodeMin.x + 10.0f, NodeMin.y + 10.0f), ImGui::GetColorU32(ImGuiCol_Text), State.Name.ToString().c_str());
        DrawList->AddText(ImVec2(NodeMin.x + 10.0f, NodeMin.y + 34.0f), ImGui::GetColorU32(ImGuiCol_TextDisabled), bEntry ? "Entry" : "State");

        ImGui::SetCursorScreenPos(NodeMin);
        ImGui::PushID(Index);
        ImGui::InvisibleButton("##AnimStateNode", NodeSize);
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
        {
            SelectedStateIndex = Index;
            SelectedTransitionIndex = -1;
        }
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
        {
            const ImVec2 Delta = ImGui::GetIO().MouseDelta;
            State.EditorNodePosition.X += Delta.x;
            State.EditorNodePosition.Y += Delta.y;
            MarkDirty();
        }
        ImGui::PopID();
    }

    DrawList->PopClipRect();
    ImGui::EndChild();
}

void FEditorAnimInstanceEditorWidget::DrawDetails()
{
    DrawSectionHeader("States");
    for (int32 Index = 0; Index < static_cast<int32>(CurrentAsset->States.size()); ++Index)
    {
        const FString Label = CurrentAsset->States[Index].Name.ToString();
        if (ImGui::Selectable(Label.c_str(), SelectedStateIndex == Index && SelectedTransitionIndex < 0))
        {
            SelectedStateIndex = Index;
            SelectedTransitionIndex = -1;
            InvalidateSelectedStateSequenceContext();
        }
    }

    ImGui::Spacing();
    DrawSectionHeader("Transitions");
    for (int32 Index = 0; Index < static_cast<int32>(CurrentAsset->Transitions.size()); ++Index)
    {
        const FAnimInstanceTransitionAssetData& Transition = CurrentAsset->Transitions[Index];
        FString Label = Transition.FromState.ToString() + FString(" -> ") + Transition.ToState.ToString();
        if (ImGui::Selectable(Label.c_str(), SelectedTransitionIndex == Index))
        {
            SelectedTransitionIndex = Index;
            SelectedStateIndex = -1;
            InvalidateSelectedStateSequenceContext();
        }
    }

    ImGui::Separator();
    if (SelectedStateIndex >= 0 && SelectedStateIndex < static_cast<int32>(CurrentAsset->States.size()))
    {
        FAnimInstanceStateAssetData& State = CurrentAsset->States[SelectedStateIndex];
        bool bChanged = false;
        bool bContextChanged = false;
        InputFName("Name", State.Name, bChanged);
        bContextChanged |= bChanged;
        InputAnimSequencePath(EditorEngine, "Anim Sequence", State.AnimSequencePath, bChanged);
        bContextChanged |= bChanged;
        const FString NormalizedSequencePath = NormalizeSequenceAssetPath(State.AnimSequencePath);
        const FString ProjectRelativeSequencePath = FPaths::ToProjectRelativePath(NormalizedSequencePath);
        const bool bLooksLikeSequence = IsSequenceAssetPath(ProjectRelativeSequencePath);
        const bool bSequenceExistsOnDisk = bLooksLikeSequence && SequenceAssetExistsOnDisk(ProjectRelativeSequencePath);
        FAnimSequenceAssetMetadata SequenceMetadata;
        const bool bHasSequenceMetadata =
            bSequenceExistsOnDisk && FAnimSequenceAssetLoader().LoadMetadata(ProjectRelativeSequencePath, SequenceMetadata);

        if (NormalizedSequencePath.empty())
        {
            ImGui::TextDisabled("Assign a .sequence asset path for this state.");
        }
        else if (!bLooksLikeSequence)
        {
            ImGui::TextColored(ImVec4(0.92f, 0.63f, 0.32f, 1.0f), "Expected a .sequence asset path.");
        }
        else if (!bSequenceExistsOnDisk)
        {
            ImGui::TextColored(ImVec4(0.89f, 0.38f, 0.34f, 1.0f), "Sequence asset file was not found on disk.");
        }
        else if (!bHasSequenceMetadata)
        {
            ImGui::TextColored(ImVec4(0.92f, 0.63f, 0.32f, 1.0f), "Sequence metadata could not be read.");
        }
        else
        {
            ImGui::TextDisabled(
                "Sequence: %s | Frames: %d | Skeleton: %s",
                SequenceMetadata.ObjectName.c_str(),
                SequenceMetadata.NumberOfFrames,
                SequenceMetadata.SkeletonAssetPath.empty()
                    ? "<None>"
                    : SequenceMetadata.SkeletonAssetPath.c_str());
        }

        if (!bSequenceExistsOnDisk || !EditorEngine)
        {
            ImGui::BeginDisabled();
        }
        if (ImGui::Button("Open Sequence") && EditorEngine)
        {
            EditorEngine->GetMainPanel().OpenAnimationSequenceAsset(ProjectRelativeSequencePath);
        }
        if (!bSequenceExistsOnDisk || !EditorEngine)
        {
            ImGui::EndDisabled();
        }

        if (ImGui::Checkbox("Loop", &State.bLoop))
        {
            bChanged = true;
            bContextChanged = true;
        }
        bChanged |= ImGui::DragFloat("Play Rate", &State.PlayRate, 0.01f, 0.0f, 10.0f);

        const bool bIsEntry = State.Name == CurrentAsset->EntryState;
        if (!bIsEntry && ImGui::Button("Set Entry"))
        {
            CurrentAsset->EntryState = State.Name;
            bChanged = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Delete State"))
        {
            DeleteSelectedState();
            return;
        }

        if (bContextChanged)
        {
            InvalidateSelectedStateSequenceContext();
        }
        RefreshSelectedStateSequenceContext();
        DrawSelectedStateSequenceContext(CachedSelectedStateContext);
        DrawSelectedStateNotifyContext(CachedSelectedStateContext);
        DrawSelectedStateAuthoringHints(CachedSelectedStateContext);

        if (bChanged)
        {
            MarkDirty();
        }
        return;
    }

    if (SelectedTransitionIndex >= 0 && SelectedTransitionIndex < static_cast<int32>(CurrentAsset->Transitions.size()))
    {
        FAnimInstanceTransitionAssetData& Transition = CurrentAsset->Transitions[SelectedTransitionIndex];
        bool bChanged = false;
        InputFName("From", Transition.FromState, bChanged);
        InputFName("To", Transition.ToState, bChanged);
        InputFName("Parameter", Transition.ParameterName, bChanged);

        const EAnimTransitionConditionType Types[] =
        {
            EAnimTransitionConditionType::BoolEquals,
            EAnimTransitionConditionType::FloatGreater,
            EAnimTransitionConditionType::FloatLessEqual,
            EAnimTransitionConditionType::IntEquals,
            EAnimTransitionConditionType::Trigger,
            EAnimTransitionConditionType::StateFinished,
        };
        if (ImGui::BeginCombo("Condition", ToConditionLabel(Transition.ConditionType)))
        {
            for (EAnimTransitionConditionType Type : Types)
            {
                const bool bSelected = Transition.ConditionType == Type;
                if (ImGui::Selectable(ToConditionLabel(Type), bSelected))
                {
                    Transition.ConditionType = Type;
                    bChanged = true;
                }
                if (bSelected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        if (Transition.ConditionType == EAnimTransitionConditionType::BoolEquals)
        {
            bChanged |= ImGui::Checkbox("Expected Bool", &Transition.ExpectedBool);
        }
        else if (Transition.ConditionType == EAnimTransitionConditionType::FloatGreater
            || Transition.ConditionType == EAnimTransitionConditionType::FloatLessEqual)
        {
            bChanged |= ImGui::DragFloat("Compare Float", &Transition.CompareFloat, 0.01f);
        }
        else if (Transition.ConditionType == EAnimTransitionConditionType::IntEquals)
        {
            bChanged |= ImGui::DragInt("Expected Int", &Transition.ExpectedInt, 1);
        }
        else if (Transition.ConditionType == EAnimTransitionConditionType::StateFinished)
        {
            ImGui::TextDisabled("Fires when the source state's non-looping animation finishes.");
        }

        bChanged |= ImGui::DragFloat("Blend Speed", &Transition.BlendSpeed, 0.01f, 0.0f, 100.0f);
        bChanged |= ImGui::DragInt("Priority", &Transition.Priority, 1);
        if (ImGui::Button("Delete Transition"))
        {
            DeleteSelectedTransition();
            return;
        }
        if (bChanged)
        {
            InvalidateSelectedStateSequenceContext();
            MarkDirty();
        }
        return;
    }

    ImGui::TextDisabled("Select a state or transition.");
}

void FEditorAnimInstanceEditorWidget::AddState()
{
    if (!CurrentAsset)
    {
        return;
    }

    FAnimInstanceStateAssetData State;
    State.Name = FName(MakeStateName(static_cast<int32>(CurrentAsset->States.size())));
    State.EditorNodePosition = FVector2(120.0f + CurrentAsset->States.size() * 40.0f, 120.0f + CurrentAsset->States.size() * 24.0f);
    CurrentAsset->States.push_back(State);

    if (!CurrentAsset->EntryState.IsValid())
    {
        CurrentAsset->EntryState = State.Name;
    }

    SelectedStateIndex = static_cast<int32>(CurrentAsset->States.size()) - 1;
    SelectedTransitionIndex = -1;
    InvalidateSelectedStateSequenceContext();
    MarkDirty();
}

void FEditorAnimInstanceEditorWidget::AddTransition()
{
    if (!CurrentAsset || CurrentAsset->States.size() < 2)
    {
        return;
    }

    FAnimInstanceTransitionAssetData Transition;
    Transition.FromState = CurrentAsset->States[0].Name;
    Transition.ToState = CurrentAsset->States[1].Name;
    Transition.ParameterName = FName("Direction");
    Transition.ConditionType = EAnimTransitionConditionType::IntEquals;
    Transition.ExpectedInt = 1;
    CurrentAsset->Transitions.push_back(Transition);
    SelectedTransitionIndex = static_cast<int32>(CurrentAsset->Transitions.size()) - 1;
    SelectedStateIndex = -1;
    InvalidateSelectedStateSequenceContext();
    MarkDirty();
}

bool FEditorAnimInstanceEditorWidget::HasSelectedState() const
{
    return CurrentAsset &&
        SelectedStateIndex >= 0 &&
        SelectedStateIndex < static_cast<int32>(CurrentAsset->States.size()) &&
        SelectedTransitionIndex < 0;
}

bool FEditorAnimInstanceEditorWidget::HasSelectedTransition() const
{
    return CurrentAsset &&
        SelectedTransitionIndex >= 0 &&
        SelectedTransitionIndex < static_cast<int32>(CurrentAsset->Transitions.size());
}

void FEditorAnimInstanceEditorWidget::DeleteSelectedState()
{
    if (!HasSelectedState())
    {
        return;
    }

    const FName RemovedStateName = CurrentAsset->States[SelectedStateIndex].Name;
    CurrentAsset->States.erase(CurrentAsset->States.begin() + SelectedStateIndex);

    CurrentAsset->Transitions.erase(
        std::remove_if(
            CurrentAsset->Transitions.begin(),
            CurrentAsset->Transitions.end(),
            [&RemovedStateName](const FAnimInstanceTransitionAssetData& Transition)
            {
                return Transition.FromState == RemovedStateName || Transition.ToState == RemovedStateName;
            }),
        CurrentAsset->Transitions.end());

    if (CurrentAsset->EntryState == RemovedStateName)
    {
        CurrentAsset->EntryState = CurrentAsset->States.empty() ? FName::None : CurrentAsset->States.front().Name;
    }

    if (CurrentAsset->States.empty())
    {
        SelectedStateIndex = -1;
    }
    else
    {
        SelectedStateIndex = std::min(SelectedStateIndex, static_cast<int32>(CurrentAsset->States.size()) - 1);
    }

    SelectedTransitionIndex = -1;
    InvalidateSelectedStateSequenceContext();
    MarkDirty();
}

void FEditorAnimInstanceEditorWidget::DeleteSelectedTransition()
{
    if (!HasSelectedTransition())
    {
        return;
    }

    CurrentAsset->Transitions.erase(CurrentAsset->Transitions.begin() + SelectedTransitionIndex);
    if (CurrentAsset->Transitions.empty())
    {
        SelectedTransitionIndex = -1;
    }
    else
    {
        SelectedTransitionIndex = std::min(SelectedTransitionIndex, static_cast<int32>(CurrentAsset->Transitions.size()) - 1);
    }

    SelectedStateIndex = -1;
    InvalidateSelectedStateSequenceContext();
    MarkDirty();
}

bool FEditorAnimInstanceEditorWidget::SaveAsset()
{
    if (!CurrentAsset || CurrentPath.empty())
    {
        return false;
    }

    if (!FResourceManager::Get().SaveAnimInstanceAsset(CurrentPath, CurrentAsset))
    {
        return false;
    }

    bDirty = false;
    if (EditorEngine)
    {
        EditorEngine->GetAssetService().RefreshAssetDatabase();
    }
    return true;
}

bool FEditorAnimInstanceEditorWidget::ReloadAsset()
{
    if (CurrentPath.empty())
    {
        return false;
    }

    CurrentAsset = FResourceManager::Get().LoadAnimInstanceAsset(CurrentPath);
    SelectedStateIndex = CurrentAsset && !CurrentAsset->States.empty() ? 0 : -1;
    SelectedTransitionIndex = -1;
    InvalidateSelectedStateSequenceContext();
    bDirty = false;
    return CurrentAsset != nullptr;
}

void FEditorAnimInstanceEditorWidget::InvalidateSelectedStateSequenceContext()
{
    bCachedSelectedStateContextValid = false;
    CachedSelectedStateContextStateIndex = -1;
    CachedSelectedStateContextStateName.clear();
    CachedSelectedStateContextSequencePath.clear();
    bCachedSelectedStateContextLoop = false;
    CachedSelectedStateFinishedTransitionCount = -1;
    CachedSelectedStateContext = {};
}

int32 FEditorAnimInstanceEditorWidget::GetSelectedStateFinishedTransitionCount() const
{
    if (!CurrentAsset ||
        SelectedStateIndex < 0 ||
        SelectedStateIndex >= static_cast<int32>(CurrentAsset->States.size()))
    {
        return 0;
    }

    const FAnimInstanceStateAssetData& State = CurrentAsset->States[SelectedStateIndex];
    int32 Count = 0;
    for (const FAnimInstanceTransitionAssetData& Transition : CurrentAsset->Transitions)
    {
        if (Transition.FromState == State.Name &&
            Transition.ConditionType == EAnimTransitionConditionType::StateFinished)
        {
            ++Count;
        }
    }

    return Count;
}

void FEditorAnimInstanceEditorWidget::RefreshSelectedStateSequenceContext()
{
    if (!CurrentAsset ||
        SelectedStateIndex < 0 ||
        SelectedStateIndex >= static_cast<int32>(CurrentAsset->States.size()))
    {
        InvalidateSelectedStateSequenceContext();
        return;
    }

    const FAnimInstanceStateAssetData& State = CurrentAsset->States[SelectedStateIndex];
    const FString StateName = State.Name.ToString();
    const int32 StateFinishedTransitionCount = GetSelectedStateFinishedTransitionCount();
    const bool bNeedsRefresh =
        !bCachedSelectedStateContextValid ||
        CachedSelectedStateContextStateIndex != SelectedStateIndex ||
        CachedSelectedStateContextStateName != StateName ||
        CachedSelectedStateContextSequencePath != State.AnimSequencePath ||
        bCachedSelectedStateContextLoop != State.bLoop ||
        CachedSelectedStateFinishedTransitionCount != StateFinishedTransitionCount;

    if (!bNeedsRefresh)
    {
        return;
    }

    CachedSelectedStateContext = BuildAnimInstanceStateSequenceContext(CurrentAsset, SelectedStateIndex);
    CachedSelectedStateContextStateIndex = SelectedStateIndex;
    CachedSelectedStateContextStateName = StateName;
    CachedSelectedStateContextSequencePath = State.AnimSequencePath;
    bCachedSelectedStateContextLoop = State.bLoop;
    CachedSelectedStateFinishedTransitionCount = StateFinishedTransitionCount;
    bCachedSelectedStateContextValid = true;
}

void FEditorAnimInstanceEditorWidget::DrawSelectedStateSequenceContext(const FAnimInstanceStateSequenceContextReport& Context)
{
    if (!ImGui::CollapsingHeader("Sequence Context", ImGuiTreeNodeFlags_DefaultOpen))
    {
        return;
    }

    if (!Context.bHasSequencePath)
    {
        ImGui::TextDisabled("No linked sequence is assigned.");
        return;
    }

    ImGui::TextWrapped(
        "Path: %s",
        Context.Overview.ProjectRelativePath.empty()
            ? Context.Overview.NormalizedPath.c_str()
            : Context.Overview.ProjectRelativePath.c_str());

    if (!Context.bLooksLikeSequencePath)
    {
        ImGui::TextDisabled("Sequence context is unavailable until the path points to a .sequence asset.");
        return;
    }

    ImGui::TextDisabled(
        "Content: %s",
        Context.bSequenceContentLoaded
            ? "Loaded"
            : (Context.Overview.bMetadataAvailable ? "Metadata Only" : "Unavailable"));
    ImGui::TextWrapped(
        "Sequence: %s",
        Context.Overview.DisplayName.empty() ? "(unnamed)" : Context.Overview.DisplayName.c_str());

    if (Context.Overview.NumberOfFrames > 0)
    {
        ImGui::Text("Frames: %d", Context.Overview.NumberOfFrames);
    }

    if (Context.Overview.FrameRateNumerator > 0 && Context.Overview.FrameRateDenominator > 0)
    {
        ImGui::Text(
            "Frame Rate: %d / %d (%.2f fps)",
            Context.Overview.FrameRateNumerator,
            Context.Overview.FrameRateDenominator,
            ComputeFramesPerSecond(
                Context.Overview.FrameRateNumerator,
                Context.Overview.FrameRateDenominator));
    }

    if (Context.Overview.DurationSeconds > 0.0f)
    {
        ImGui::Text("Length: %.3fs", Context.Overview.DurationSeconds);
    }

    ImGui::TextWrapped(
        "Skeleton: %s",
        Context.Overview.SkeletonAssetPath.empty()
            ? "(none)"
            : Context.Overview.SkeletonAssetPath.c_str());
    if (Context.bSequenceContentLoaded)
    {
        ImGui::Text("Curves: %d", Context.Overview.CurveCount);
    }
}

void FEditorAnimInstanceEditorWidget::DrawSelectedStateNotifyContext(const FAnimInstanceStateSequenceContextReport& Context)
{
    if (!ImGui::CollapsingHeader("Notify Context", ImGuiTreeNodeFlags_DefaultOpen))
    {
        return;
    }

    if (!Context.bSequenceContentLoaded)
    {
        ImGui::TextDisabled("Notify context becomes available when the linked sequence content can be loaded.");
        return;
    }

    ImGui::Text("Tracks: %d", Context.NotifySummary.TrackCount);
    ImGui::Text("Total Notifies: %d", Context.NotifySummary.TotalNotifyCount);
    ImGui::Text("One-shot: %d", Context.NotifySummary.OneShotNotifyCount);
    ImGui::Text("Notify States: %d", Context.NotifySummary.NotifyStateCount);

    if (!Context.BuiltInUsage.empty())
    {
        ImGui::Spacing();
        ImGui::TextDisabled("Built-in Usage");
        for (const FAnimInstanceStateBuiltInNotifyUsage& Usage : Context.BuiltInUsage)
        {
            ImGui::BulletText("%s x%d", Usage.DisplayName.c_str(), Usage.Count);
            if (!Usage.NotifyClassName.empty())
            {
                ImGui::Indent();
                ImGui::TextDisabled("%s", Usage.NotifyClassName.c_str());
                for (const FString& Sample : Usage.Samples)
                {
                    ImGui::TextDisabled("%s", Sample.c_str());
                }
                ImGui::Unindent();
            }
        }
    }

    if (!Context.NotifyHighlights.empty())
    {
        ImGui::Spacing();
        ImGui::TextDisabled("Notify Highlights");
        for (const FAnimInstanceStateNotifyHighlight& Highlight : Context.NotifyHighlights)
        {
            ImGui::BulletText("%.3fs  %s", Highlight.Time, Highlight.NotifyName.c_str());
            ImGui::Indent();
            ImGui::TextDisabled(
                "%s | %s | %s",
                Highlight.TrackName.c_str(),
                Highlight.NotifyType.c_str(),
                Highlight.NotifyClassName.c_str());
            ImGui::Unindent();
        }
    }
}

void FEditorAnimInstanceEditorWidget::DrawSelectedStateAuthoringHints(const FAnimInstanceStateSequenceContextReport& Context)
{
    if (!ImGui::CollapsingHeader("Authoring Hints", ImGuiTreeNodeFlags_DefaultOpen))
    {
        return;
    }

    if (Context.Hints.empty())
    {
        ImGui::TextDisabled("No current authoring hints.");
        return;
    }

    for (const FAnimInstanceStateAuthoringHint& Hint : Context.Hints)
    {
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextColored(
            ToHintColor(Hint.Severity),
            "%s: %s",
            ToHintPrefix(Hint.Severity),
            Hint.Message.c_str());
        ImGui::PopTextWrapPos();
    }
}
