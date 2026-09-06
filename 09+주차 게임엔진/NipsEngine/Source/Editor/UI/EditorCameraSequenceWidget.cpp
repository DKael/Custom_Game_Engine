#include "Editor/UI/EditorCameraSequenceWidget.h"

#include "ImGui/imgui.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace
{
    constexpr float MinDomainSize = 0.01f;
    constexpr float KeyRadius = 6.0f;
    constexpr float HandleRadius = 5.0f;
    constexpr float KeyTimeEpsilon = 0.001f;

    int32 FindSequenceIndex(const TArray<FString>& Paths, const FString& TargetPath)
    {
        for (int32 Index = 0; Index < static_cast<int32>(Paths.size()); ++Index)
        {
            if (Paths[Index] == TargetPath)
            {
                return Index;
            }
        }
        return -1;
    }

    const char* GetInterpLabel(ECameraSequenceInterpMode Mode)
    {
        return Mode == ECameraSequenceInterpMode::Bezier ? "Bezier" : "Linear";
    }

    float DistanceSquared(const ImVec2& A, const ImVec2& B)
    {
        const float DX = A.x - B.x;
        const float DY = A.y - B.y;
        return DX * DX + DY * DY;
    }
}

void FEditorCameraSequenceWidget::Initialize(UEditorEngine* InEditorEngine)
{
    FEditorWidget::Initialize(InEditorEngine);
    RefreshSequenceList();
    CreateNewSequence();
}

void FEditorCameraSequenceWidget::Render(float DeltaTime)
{
    (void)DeltaTime;

    if (!ImGui::Begin("Camera Sequence Editor"))
    {
        ImGui::End();
        return;
    }

    RenderToolbar();
    ImGui::Separator();

    ImGui::BeginChild("##SequenceList", ImVec2(240.0f, 0.0f), true);
    RenderSequenceList();
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("##SequenceEditorBody", ImVec2(0.0f, 0.0f), false);
    RenderEditorBody();
    ImGui::EndChild();

    ImGui::End();
}

void FEditorCameraSequenceWidget::RenderToolbar()
{
    if (ImGui::Button("New"))
    {
        CreateNewSequence();
    }
    ImGui::SameLine();
    if (ImGui::Button("Save"))
    {
        SaveCurrentSequence();
    }
    ImGui::SameLine();
    if (ImGui::Button("Reload"))
    {
        ReloadCurrentSequence();
    }
    ImGui::SameLine();
    if (ImGui::Button("Refresh List"))
    {
        RefreshSequenceList();
    }

    if (!StatusMessage.empty())
    {
        ImGui::SameLine();
        ImGui::TextDisabled("%s", StatusMessage.c_str());
    }
}

void FEditorCameraSequenceWidget::RenderSequenceList()
{
    ImGui::TextUnformatted("Sequences");
    ImGui::Separator();

    for (int32 Index = 0; Index < static_cast<int32>(SequencePaths.size()); ++Index)
    {
        const bool bSelected = SelectedSequenceIndex == Index;
        if (ImGui::Selectable(SequencePaths[Index].c_str(), bSelected))
        {
            LoadSequence(SequencePaths[Index]);
        }
    }
}

void FEditorCameraSequenceWidget::RenderEditorBody()
{
    ImGui::InputText("Path", SequencePathBuffer, sizeof(SequencePathBuffer));
    CurrentSequencePath = SequencePathBuffer;

    ImGui::InputText("Name", SequenceNameBuffer, sizeof(SequenceNameBuffer));
    CurrentAsset.Name = SequenceNameBuffer;

    const float Duration = CurrentAsset.GetLastKeyTime();
    ImGui::Text("Duration: %.3f (auto from last key)", Duration);
    ImGui::Text("X Domain: %.3f ~ %.3f (all channel keys)", TimeDomainMin, TimeDomainMax);

    float ValueDomain[2] = { ValueDomainMin, ValueDomainMax };
    if (ImGui::DragFloat2("Y Domain", ValueDomain, 0.01f, -1000.0f, 1000.0f, "%.3f"))
    {
        ValueDomainMin = ValueDomain[0];
        ValueDomainMax = ValueDomain[1];
        ClampDomains();
    }
    ImGui::SameLine();
    if (ImGui::Button("Frame Y"))
    {
        FrameValueDomain();
    }

    const TArray<FString>& ChannelNames = FCameraSequenceManager::Get().GetBuiltInChannelNames();
    const char* CurrentChannelLabel =
        (SelectedChannelIndex >= 0 && SelectedChannelIndex < static_cast<int32>(ChannelNames.size()))
        ? ChannelNames[SelectedChannelIndex].c_str()
        : "<None>";
    if (ImGui::BeginCombo("Channel", CurrentChannelLabel))
    {
        for (int32 Index = 0; Index < static_cast<int32>(ChannelNames.size()); ++Index)
        {
            const bool bSelected = SelectedChannelIndex == Index;
            if (ImGui::Selectable(ChannelNames[Index].c_str(), bSelected))
            {
                SelectedChannelIndex = Index;
                SelectedKeyIndex = -1;
                FrameValueDomain();
            }
            if (bSelected)
            {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    FCameraSequenceChannel* Channel = GetSelectedChannel();
    if (Channel)
    {
        if (ImGui::Button("Add Key"))
        {
            FCameraSequenceKey NewKey;
            if (SelectedKeyIndex >= 0 && SelectedKeyIndex < static_cast<int32>(Channel->Keys.size()))
            {
                const FCameraSequenceKey& SelectedKey = Channel->Keys[SelectedKeyIndex];
                float NextTime = SelectedKey.Time + 0.1f;
                if (SelectedKeyIndex + 1 < static_cast<int32>(Channel->Keys.size()))
                {
                    const FCameraSequenceKey& NextKey = Channel->Keys[SelectedKeyIndex + 1];
                    NextTime = SelectedKey.Time + (NextKey.Time - SelectedKey.Time) * 0.5f;
                    NewKey.Value = SelectedKey.Value + (NextKey.Value - SelectedKey.Value) * 0.5f;
                }
                else
                {
                    NewKey.Value = SelectedKey.Value;
                }
                NewKey.Time = NextTime;
            }
            else
            {
                NewKey.Time = Channel->Keys.empty() ? 0.0f : Channel->Keys.back().Time + 0.1f;
            }
                Channel->Keys.push_back(NewKey);
            Channel->SortKeys();
            CurrentAsset.RecalculateDuration();
            UpdateTimeDomainFromSequence();
            for (int32 Index = 0; Index < static_cast<int32>(Channel->Keys.size()); ++Index)
            {
                if (std::abs(Channel->Keys[Index].Time - NewKey.Time) < KeyTimeEpsilon &&
                    std::abs(Channel->Keys[Index].Value - NewKey.Value) < KeyTimeEpsilon)
                {
                    SelectedKeyIndex = Index;
                    break;
                }
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Delete Key"))
        {
            if (SelectedKeyIndex >= 0 && SelectedKeyIndex < static_cast<int32>(Channel->Keys.size()))
            {
                Channel->Keys.erase(Channel->Keys.begin() + SelectedKeyIndex);
                CurrentAsset.RecalculateDuration();
                UpdateTimeDomainFromSequence();
                SelectedKeyIndex = std::min(SelectedKeyIndex, static_cast<int32>(Channel->Keys.size()) - 1);
            }
        }
    }

    ImGui::Separator();
    RenderChannelCanvas();
    ImGui::Separator();
    RenderKeyInspector();
}

void FEditorCameraSequenceWidget::RenderChannelCanvas()
{
    FCameraSequenceChannel* Channel = GetSelectedChannel();
    if (!Channel)
    {
        ImGui::TextDisabled("No channel selected.");
        return;
    }

    ClampDomains();

    const ImVec2 CanvasSize(ImGui::GetContentRegionAvail().x, 360.0f);
    const ImVec2 CanvasPos = ImGui::GetCursorScreenPos();
    ImDrawList* DrawList = ImGui::GetWindowDrawList();

    ImGui::InvisibleButton("##SequenceCanvas", CanvasSize);
    const bool bCanvasHovered = ImGui::IsItemHovered();
    const ImGuiIO& IO = ImGui::GetIO();

    DrawList->AddRectFilled(CanvasPos, ImVec2(CanvasPos.x + CanvasSize.x, CanvasPos.y + CanvasSize.y), IM_COL32(21, 24, 30, 255), 4.0f);
    DrawList->AddRect(CanvasPos, ImVec2(CanvasPos.x + CanvasSize.x, CanvasPos.y + CanvasSize.y), IM_COL32(92, 102, 122, 255), 4.0f);

    const ImVec2 PlotMin(CanvasPos.x + 56.0f, CanvasPos.y + 20.0f);
    const ImVec2 PlotMax(CanvasPos.x + CanvasSize.x - 16.0f, CanvasPos.y + CanvasSize.y - 34.0f);
    const float PlotWidth = std::max(PlotMax.x - PlotMin.x, 1.0f);
    const float PlotHeight = std::max(PlotMax.y - PlotMin.y, 1.0f);

    DrawList->AddRectFilled(PlotMin, PlotMax, IM_COL32(16, 18, 24, 255), 2.0f);
    DrawList->AddRect(PlotMin, PlotMax, IM_COL32(76, 84, 98, 180), 2.0f);

    auto TimeToScreenX = [&](float Time)
    {
        const float Alpha = (Time - TimeDomainMin) / std::max(TimeDomainMax - TimeDomainMin, MinDomainSize);
        return PlotMin.x + MathUtil::Clamp(Alpha, 0.0f, 1.0f) * PlotWidth;
    };
    auto ValueToScreenY = [&](float Value)
    {
        const float Alpha = (Value - ValueDomainMin) / std::max(ValueDomainMax - ValueDomainMin, MinDomainSize);
        return PlotMin.y + (1.0f - MathUtil::Clamp(Alpha, 0.0f, 1.0f)) * PlotHeight;
    };
    auto ScreenToTime = [&](float X)
    {
        const float Alpha = MathUtil::Clamp((X - PlotMin.x) / PlotWidth, 0.0f, 1.0f);
        return TimeDomainMin + (TimeDomainMax - TimeDomainMin) * Alpha;
    };
    auto ScreenToValue = [&](float Y)
    {
        const float Alpha = MathUtil::Clamp((Y - PlotMin.y) / PlotHeight, 0.0f, 1.0f);
        return ValueDomainMax + (ValueDomainMin - ValueDomainMax) * Alpha;
    };

    for (int32 GridIndex = 0; GridIndex <= 8; ++GridIndex)
    {
        const float Alpha = static_cast<float>(GridIndex) / 8.0f;
        const float X = PlotMin.x + PlotWidth * Alpha;
        const float Y = PlotMin.y + PlotHeight * Alpha;
        const ImU32 GridColor = GridIndex == 0 || GridIndex == 8 ? IM_COL32(76, 84, 98, 255) : IM_COL32(46, 52, 64, 255);
        DrawList->AddLine(ImVec2(X, PlotMin.y), ImVec2(X, PlotMax.y), GridColor);
        DrawList->AddLine(ImVec2(PlotMin.x, Y), ImVec2(PlotMax.x, Y), GridColor);
    }

    char LabelBuffer[64];
    std::snprintf(LabelBuffer, sizeof(LabelBuffer), "X %.2f", TimeDomainMin);
    DrawList->AddText(ImVec2(PlotMin.x, PlotMax.y + 8.0f), IM_COL32(180, 190, 210, 255), LabelBuffer);
    std::snprintf(LabelBuffer, sizeof(LabelBuffer), "X %.2f", TimeDomainMax);
    DrawList->AddText(ImVec2(PlotMax.x - 52.0f, PlotMax.y + 8.0f), IM_COL32(180, 190, 210, 255), LabelBuffer);
    std::snprintf(LabelBuffer, sizeof(LabelBuffer), "Y %.2f", ValueDomainMax);
    DrawList->AddText(ImVec2(CanvasPos.x + 8.0f, PlotMin.y - 8.0f), IM_COL32(180, 190, 210, 255), LabelBuffer);
    std::snprintf(LabelBuffer, sizeof(LabelBuffer), "Y %.2f", ValueDomainMin);
    DrawList->AddText(ImVec2(CanvasPos.x + 8.0f, PlotMax.y - 8.0f), IM_COL32(180, 190, 210, 255), LabelBuffer);

    const float Duration = CurrentAsset.GetLastKeyTime();
    if (Duration >= TimeDomainMin && Duration <= TimeDomainMax)
    {
        const float DurationX = TimeToScreenX(Duration);
        DrawList->AddLine(ImVec2(DurationX, PlotMin.y), ImVec2(DurationX, PlotMax.y), IM_COL32(250, 210, 80, 180), 1.5f);
        std::snprintf(LabelBuffer, sizeof(LabelBuffer), "Duration %.2f", Duration);
        DrawList->AddText(ImVec2(DurationX + 4.0f, PlotMin.y + 4.0f), IM_COL32(250, 210, 80, 255), LabelBuffer);
    }

    if (bCanvasHovered)
    {
        const float MouseX = MathUtil::Clamp(IO.MousePos.x, PlotMin.x, PlotMax.x);
        const float MouseY = MathUtil::Clamp(IO.MousePos.y, PlotMin.y, PlotMax.y);
        DrawList->AddLine(ImVec2(MouseX, PlotMin.y), ImVec2(MouseX, PlotMax.y), IM_COL32(255, 255, 255, 36));
        DrawList->AddLine(ImVec2(PlotMin.x, MouseY), ImVec2(PlotMax.x, MouseY), IM_COL32(255, 255, 255, 36));
    }

    const auto GetLeaveHandlePoint = [&](int32 KeyIndex)
    {
        const FCameraSequenceKey& Key = Channel->Keys[KeyIndex];
        const FCameraSequenceKey& NextKey = Channel->Keys[KeyIndex + 1];
        const float DeltaTime = std::max(NextKey.Time - Key.Time, KeyTimeEpsilon);
        return ImVec2(
            TimeToScreenX(Key.Time + MathUtil::Clamp(Key.LeaveTime, KeyTimeEpsilon, DeltaTime - KeyTimeEpsilon)),
            ValueToScreenY(Key.Value + Key.LeaveTangent));
    };
    const auto GetArriveHandlePoint = [&](int32 KeyIndex)
    {
        const FCameraSequenceKey& PrevKey = Channel->Keys[KeyIndex - 1];
        const FCameraSequenceKey& Key = Channel->Keys[KeyIndex];
        const float DeltaTime = std::max(Key.Time - PrevKey.Time, KeyTimeEpsilon);
        return ImVec2(
            TimeToScreenX(Key.Time - MathUtil::Clamp(Key.ArriveTime, KeyTimeEpsilon, DeltaTime - KeyTimeEpsilon)),
            ValueToScreenY(Key.Value - Key.ArriveTangent));
    };

    for (int32 SegmentIndex = 0; SegmentIndex < static_cast<int32>(Channel->Keys.size()) - 1; ++SegmentIndex)
    {
        const FCameraSequenceKey& StartKey = Channel->Keys[SegmentIndex];
        const FCameraSequenceKey& EndKey = Channel->Keys[SegmentIndex + 1];
        const ImVec2 P0(TimeToScreenX(StartKey.Time), ValueToScreenY(StartKey.Value));
        const ImVec2 P3(TimeToScreenX(EndKey.Time), ValueToScreenY(EndKey.Value));

        if (StartKey.InterpMode == ECameraSequenceInterpMode::Bezier)
        {
            const ImVec2 P1 = GetLeaveHandlePoint(SegmentIndex);
            const ImVec2 P2 = GetArriveHandlePoint(SegmentIndex + 1);
            const bool bSelectedSegment = SelectedKeyIndex == SegmentIndex || SelectedKeyIndex == SegmentIndex + 1;
            DrawList->AddBezierCubic(P0, P1, P2, P3, IM_COL32(110, 200, 255, 255), 2.5f, 32);
            DrawList->AddLine(P0, P1, IM_COL32(255, 130, 190, 120), 1.0f);
            DrawList->AddLine(P2, P3, IM_COL32(110, 255, 220, 120), 1.0f);
            DrawList->AddCircleFilled(P1, HandleRadius, bSelectedSegment ? IM_COL32(255, 130, 190, 255) : IM_COL32(255, 130, 190, 180));
            DrawList->AddCircleFilled(P2, HandleRadius, bSelectedSegment ? IM_COL32(110, 255, 220, 255) : IM_COL32(110, 255, 220, 180));
        }
        else
        {
            DrawList->AddLine(P0, P3, IM_COL32(110, 200, 255, 255), 2.0f);
        }
    }

    int32 HoveredKeyIndex = -1;
    int32 HoveredLeaveHandleKeyIndex = -1;
    int32 HoveredArriveHandleKeyIndex = -1;
    float BestDistanceSq = KeyRadius * KeyRadius * 2.5f;

    for (int32 KeyIndex = 0; KeyIndex < static_cast<int32>(Channel->Keys.size()); ++KeyIndex)
    {
        const FCameraSequenceKey& Key = Channel->Keys[KeyIndex];
        const ImVec2 KeyPos(TimeToScreenX(Key.Time), ValueToScreenY(Key.Value));
        const float DistanceSq = DistanceSquared(IO.MousePos, KeyPos);
        if (DistanceSq <= BestDistanceSq)
        {
            HoveredKeyIndex = KeyIndex;
            BestDistanceSq = DistanceSq;
        }
    }

    for (int32 KeyIndex = 0; KeyIndex < static_cast<int32>(Channel->Keys.size()) - 1; ++KeyIndex)
    {
        if (Channel->Keys[KeyIndex].InterpMode != ECameraSequenceInterpMode::Bezier)
        {
            continue;
        }

        const ImVec2 LeaveHandle = GetLeaveHandlePoint(KeyIndex);
        const float LeaveDistanceSq = DistanceSquared(IO.MousePos, LeaveHandle);
        if (LeaveDistanceSq <= BestDistanceSq)
        {
            HoveredKeyIndex = -1;
            HoveredLeaveHandleKeyIndex = KeyIndex;
            HoveredArriveHandleKeyIndex = -1;
            BestDistanceSq = LeaveDistanceSq;
        }

        const ImVec2 ArriveHandle = GetArriveHandlePoint(KeyIndex + 1);
        const float ArriveDistanceSq = DistanceSquared(IO.MousePos, ArriveHandle);
        if (ArriveDistanceSq <= BestDistanceSq)
        {
            HoveredKeyIndex = -1;
            HoveredLeaveHandleKeyIndex = -1;
            HoveredArriveHandleKeyIndex = KeyIndex + 1;
            BestDistanceSq = ArriveDistanceSq;
        }
    }

    if (bCanvasHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        if (HoveredLeaveHandleKeyIndex >= 0)
        {
            ActiveLeaveHandleKeyIndex = HoveredLeaveHandleKeyIndex;
            SelectedKeyIndex = HoveredLeaveHandleKeyIndex;
        }
        else if (HoveredArriveHandleKeyIndex >= 0)
        {
            ActiveArriveHandleKeyIndex = HoveredArriveHandleKeyIndex;
            SelectedKeyIndex = HoveredArriveHandleKeyIndex;
        }
        else if (HoveredKeyIndex >= 0)
        {
            ActiveKeyDragIndex = HoveredKeyIndex;
            SelectedKeyIndex = HoveredKeyIndex;
        }
        else
        {
            SelectedKeyIndex = -1;
        }
    }

    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        ResetInteractionState();
    }

    if (ActiveKeyDragIndex >= 0 && ActiveKeyDragIndex < static_cast<int32>(Channel->Keys.size()) && bCanvasHovered)
    {
        FCameraSequenceKey& Key = Channel->Keys[ActiveKeyDragIndex];
        float MinTime = TimeDomainMin;
        float MaxTime = TimeDomainMax;
        if (ActiveKeyDragIndex > 0)
        {
            MinTime = Channel->Keys[ActiveKeyDragIndex - 1].Time + KeyTimeEpsilon;
        }
        if (ActiveKeyDragIndex + 1 < static_cast<int32>(Channel->Keys.size()))
        {
            MaxTime = Channel->Keys[ActiveKeyDragIndex + 1].Time - KeyTimeEpsilon;
        }

        Key.Time = MathUtil::Clamp(ScreenToTime(IO.MousePos.x), MinTime, MaxTime);
        Key.Value = MathUtil::Clamp(ScreenToValue(IO.MousePos.y), ValueDomainMin, ValueDomainMax);
        CurrentAsset.RecalculateDuration();
    }

    if (ActiveLeaveHandleKeyIndex >= 0 &&
        ActiveLeaveHandleKeyIndex + 1 < static_cast<int32>(Channel->Keys.size()) &&
        bCanvasHovered)
    {
        FCameraSequenceKey& Key = Channel->Keys[ActiveLeaveHandleKeyIndex];
        const FCameraSequenceKey& NextKey = Channel->Keys[ActiveLeaveHandleKeyIndex + 1];
        const float DeltaTime = std::max(NextKey.Time - Key.Time, KeyTimeEpsilon);
        const float HandleTime = MathUtil::Clamp(ScreenToTime(IO.MousePos.x), Key.Time + KeyTimeEpsilon, NextKey.Time - KeyTimeEpsilon);
        const float HandleValue = MathUtil::Clamp(ScreenToValue(IO.MousePos.y), ValueDomainMin, ValueDomainMax);
        Key.LeaveTime = HandleTime - Key.Time;
        Key.LeaveTangent = HandleValue - Key.Value;
        Key.InterpMode = ECameraSequenceInterpMode::Bezier;
    }

    if (ActiveArriveHandleKeyIndex > 0 &&
        ActiveArriveHandleKeyIndex < static_cast<int32>(Channel->Keys.size()) &&
        bCanvasHovered)
    {
        FCameraSequenceKey& Key = Channel->Keys[ActiveArriveHandleKeyIndex];
        const FCameraSequenceKey& PrevKey = Channel->Keys[ActiveArriveHandleKeyIndex - 1];
        const float DeltaTime = std::max(Key.Time - PrevKey.Time, KeyTimeEpsilon);
        const float HandleTime = MathUtil::Clamp(ScreenToTime(IO.MousePos.x), PrevKey.Time + KeyTimeEpsilon, Key.Time - KeyTimeEpsilon);
        const float HandleValue = MathUtil::Clamp(ScreenToValue(IO.MousePos.y), ValueDomainMin, ValueDomainMax);
        Key.ArriveTime = Key.Time - HandleTime;
        Key.ArriveTangent = Key.Value - HandleValue;
        Channel->Keys[ActiveArriveHandleKeyIndex - 1].InterpMode = ECameraSequenceInterpMode::Bezier;
    }

    for (int32 KeyIndex = 0; KeyIndex < static_cast<int32>(Channel->Keys.size()); ++KeyIndex)
    {
        const FCameraSequenceKey& Key = Channel->Keys[KeyIndex];
        const ImVec2 KeyPos(TimeToScreenX(Key.Time), ValueToScreenY(Key.Value));
        const bool bSelected = SelectedKeyIndex == KeyIndex;
        const bool bHovered = HoveredKeyIndex == KeyIndex;
        DrawList->AddCircleFilled(
            KeyPos,
            bSelected ? KeyRadius + 1.5f : KeyRadius,
            bSelected ? IM_COL32(255, 215, 95, 255) : (bHovered ? IM_COL32(220, 230, 255, 255) : IM_COL32(240, 245, 255, 235)));
        DrawList->AddCircle(KeyPos, bSelected ? KeyRadius + 1.5f : KeyRadius, IM_COL32(24, 28, 36, 255), 0, 1.5f);
    }

    if (bCanvasHovered)
    {
        const float HoverTime = ScreenToTime(IO.MousePos.x);
        const float HoverValue = ScreenToValue(IO.MousePos.y);
        std::snprintf(LabelBuffer, sizeof(LabelBuffer), "t=%.3f, v=%.3f", HoverTime, HoverValue);
        DrawList->AddText(ImVec2(PlotMax.x - 118.0f, CanvasPos.y + 4.0f), IM_COL32(220, 225, 235, 255), LabelBuffer);
    }

    if (HoveredKeyIndex >= 0)
    {
        const FCameraSequenceKey& HoveredKey = Channel->Keys[HoveredKeyIndex];
        ImGui::SetTooltip("Key %d\nTime: %.3f\nValue: %.3f", HoveredKeyIndex, HoveredKey.Time, HoveredKey.Value);
    }
    else if (HoveredLeaveHandleKeyIndex >= 0)
    {
        ImGui::SetTooltip("Leave Tangent: %.3f", Channel->Keys[HoveredLeaveHandleKeyIndex].LeaveTangent);
    }
    else if (HoveredArriveHandleKeyIndex >= 0)
    {
        ImGui::SetTooltip("Arrive Tangent: %.3f", Channel->Keys[HoveredArriveHandleKeyIndex].ArriveTangent);
    }

    // ImGui::TextDisabled("Click a key to select. Drag keys to move. Drag Bezier handles for tangents. Duration follows the last key.");
}

void FEditorCameraSequenceWidget::RenderKeyInspector()
{
    FCameraSequenceChannel* Channel = GetSelectedChannel();
    if (!Channel)
    {
        return;
    }

    ClampSelection();
    if (SelectedKeyIndex < 0 || SelectedKeyIndex >= static_cast<int32>(Channel->Keys.size()))
    {
        ImGui::TextDisabled("No key selected.");
        return;
    }

    FCameraSequenceKey& Key = Channel->Keys[SelectedKeyIndex];
    ImGui::Text("Selected Key %d", SelectedKeyIndex);
    if (IsSelectedChannelFOV())
    {
        ImGui::TextDisabled("FOV channel values are in radians.");
    }

    float MinTime = -1000.0f;
    float MaxTime = 1000.0f;
    if (SelectedKeyIndex > 0)
    {
        MinTime = Channel->Keys[SelectedKeyIndex - 1].Time + KeyTimeEpsilon;
    }
    if (SelectedKeyIndex + 1 < static_cast<int32>(Channel->Keys.size()))
    {
        MaxTime = Channel->Keys[SelectedKeyIndex + 1].Time - KeyTimeEpsilon;
    }

    if (ImGui::DragFloat("Time", &Key.Time, 0.01f, MinTime, MaxTime, "%.3f"))
    {
        Key.Time = MathUtil::Clamp(Key.Time, MinTime, MaxTime);
        CurrentAsset.RecalculateDuration();
        UpdateTimeDomainFromSequence();
    }

    const char* ValueLabel = IsSelectedChannelFOV() ? "Value (rad)" : "Value";
    if (ImGui::DragFloat(ValueLabel, &Key.Value, 0.01f, -1000.0f, 1000.0f, "%.3f"))
    {
    }

    const char* CurrentInterp = GetInterpLabel(Key.InterpMode);
    if (ImGui::BeginCombo("Interp", CurrentInterp))
    {
        const bool bLinearSelected = Key.InterpMode == ECameraSequenceInterpMode::Linear;
        if (ImGui::Selectable("Linear", bLinearSelected))
        {
            Key.InterpMode = ECameraSequenceInterpMode::Linear;
        }
        if (bLinearSelected)
        {
            ImGui::SetItemDefaultFocus();
        }

        const bool bBezierSelected = Key.InterpMode == ECameraSequenceInterpMode::Bezier;
        if (ImGui::Selectable("Bezier", bBezierSelected))
        {
            Key.InterpMode = ECameraSequenceInterpMode::Bezier;
        }
        if (bBezierSelected)
        {
            ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    if (Key.InterpMode == ECameraSequenceInterpMode::Bezier)
    {
        ImGui::DragFloat("Arrive Time", &Key.ArriveTime, 0.01f, 0.001f, 1000.0f, "%.3f");
        ImGui::DragFloat("Leave Time", &Key.LeaveTime, 0.01f, 0.001f, 1000.0f, "%.3f");
        const char* ArriveTangentLabel = IsSelectedChannelFOV() ? "Arrive Offset (rad)" : "Arrive Offset";
        const char* LeaveTangentLabel = IsSelectedChannelFOV() ? "Leave Offset (rad)" : "Leave Offset";
        ImGui::DragFloat(ArriveTangentLabel, &Key.ArriveTangent, 0.01f, -1000.0f, 1000.0f, "%.3f");
        ImGui::DragFloat(LeaveTangentLabel, &Key.LeaveTangent, 0.01f, -1000.0f, 1000.0f, "%.3f");
    }
}

void FEditorCameraSequenceWidget::RefreshSequenceList()
{
    FCameraSequenceManager::Get().RefreshSequenceList();
    SequencePaths = FCameraSequenceManager::Get().GetAvailableSequencePaths();
    if (!CurrentSequencePath.empty())
    {
        SelectedSequenceIndex = FindSequenceIndex(SequencePaths, CurrentSequencePath);
    }
    else
    {
        SelectedSequenceIndex = -1;
    }
}

void FEditorCameraSequenceWidget::CreateNewSequence()
{
    CurrentAsset = {};
    CurrentAsset.Name = "NewSequence";
    CurrentSequencePath = FString(FCameraSequenceManager::SequenceRoot) + "/NewSequence.json";
    bHasLoadedSequence = true;
    SelectedChannelIndex = 0;
    SelectedKeyIndex = -1;
    EnsureAllBuiltInChannelsExist();
    ResetInteractionState();

    strncpy_s(SequencePathBuffer, sizeof(SequencePathBuffer), CurrentSequencePath.c_str(), _TRUNCATE);
    strncpy_s(SequenceNameBuffer, sizeof(SequenceNameBuffer), CurrentAsset.Name.c_str(), _TRUNCATE);
    SelectedSequenceIndex = FindSequenceIndex(SequencePaths, CurrentSequencePath);
    UpdateTimeDomainFromSequence();
    FrameValueDomain();
    StatusMessage = "Editing new sequence";
}

bool FEditorCameraSequenceWidget::LoadSequence(const FString& Identifier)
{
    FString ResolvedPath;
    FCameraSequenceAsset LoadedAsset;
    if (!FCameraSequenceManager::Get().TryLoadEditableSequence(Identifier, ResolvedPath, LoadedAsset))
    {
        StatusMessage = "Failed to load sequence";
        return false;
    }

    CurrentSequencePath = ResolvedPath;
    CurrentAsset = LoadedAsset;
    EnsureAllBuiltInChannelsExist();
    bHasLoadedSequence = true;
    SelectedSequenceIndex = FindSequenceIndex(SequencePaths, CurrentSequencePath);
    SelectedKeyIndex = -1;
    ResetInteractionState();

    strncpy_s(SequencePathBuffer, sizeof(SequencePathBuffer), CurrentSequencePath.c_str(), _TRUNCATE);
    strncpy_s(SequenceNameBuffer, sizeof(SequenceNameBuffer), CurrentAsset.Name.c_str(), _TRUNCATE);
    UpdateTimeDomainFromSequence();
    FrameValueDomain();
    StatusMessage = "Loaded";
    return true;
}

void FEditorCameraSequenceWidget::SaveCurrentSequence()
{
    if (!bHasLoadedSequence)
    {
        return;
    }

    CurrentSequencePath = SequencePathBuffer;
    CurrentAsset.Name = SequenceNameBuffer;
    EnsureAllBuiltInChannelsExist();
    CurrentAsset.SortAllChannels();
    CurrentAsset.RecalculateDuration();

    if (CurrentSequencePath.empty())
    {
        StatusMessage = "Path is required";
        return;
    }

    if (CurrentSequencePath.find(".json") == FString::npos)
    {
        CurrentSequencePath += ".json";
        strncpy_s(SequencePathBuffer, sizeof(SequencePathBuffer), CurrentSequencePath.c_str(), _TRUNCATE);
    }

    if (FCameraSequenceManager::Get().SaveSequence(CurrentSequencePath, CurrentAsset))
    {
        RefreshSequenceList();
        SelectedSequenceIndex = FindSequenceIndex(SequencePaths, CurrentSequencePath);
        UpdateTimeDomainFromSequence();
        StatusMessage = "Saved";
    }
    else
    {
        StatusMessage = "Save failed";
    }
}

void FEditorCameraSequenceWidget::ReloadCurrentSequence()
{
    if (CurrentSequencePath.empty())
    {
        return;
    }

    if (LoadSequence(CurrentSequencePath))
    {
        StatusMessage = "Reloaded";
    }
}

void FEditorCameraSequenceWidget::EnsureAllBuiltInChannelsExist()
{
    const TArray<FString>& ChannelNames = FCameraSequenceManager::Get().GetBuiltInChannelNames();
    for (const FString& ChannelName : ChannelNames)
    {
        if (CurrentAsset.Channels.find(ChannelName) == CurrentAsset.Channels.end())
        {
            CurrentAsset.Channels[ChannelName] = {};
        }
    }

    CurrentAsset.RecalculateDuration();
}

void FEditorCameraSequenceWidget::UpdateTimeDomainFromSequence()
{
    bool bHasAnyKey = false;
    float MinTime = 0.0f;
    float MaxTime = 0.0f;
    for (const auto& Pair : CurrentAsset.Channels)
    {
        for (const FCameraSequenceKey& Key : Pair.second.Keys)
        {
            if (!bHasAnyKey)
            {
                MinTime = Key.Time;
                MaxTime = Key.Time;
                bHasAnyKey = true;
            }
            else
            {
                MinTime = std::min(MinTime, Key.Time);
                MaxTime = std::max(MaxTime, Key.Time);
            }
        }
    }

    if (!bHasAnyKey)
    {
        TimeDomainMin = 0.0f;
        TimeDomainMax = 1.0f;
    }
    else
    {
        TimeDomainMin = MinTime;
        TimeDomainMax = std::max(MaxTime, MinTime + 1.0f);
    }
    ClampDomains();
}

void FEditorCameraSequenceWidget::FrameValueDomain()
{
    const FCameraSequenceChannel* Channel = GetSelectedChannel();
    if (!Channel || Channel->Keys.empty())
    {
        ValueDomainMin = -1.0f;
        ValueDomainMax = 1.0f;
        return;
    }

    ValueDomainMin = Channel->Keys.front().Value;
    ValueDomainMax = Channel->Keys.front().Value;
    for (const FCameraSequenceKey& Key : Channel->Keys)
    {
        ValueDomainMin = std::min(ValueDomainMin, Key.Value);
        ValueDomainMax = std::max(ValueDomainMax, Key.Value);
        ValueDomainMin = std::min(ValueDomainMin, Key.Value + Key.ArriveTangent * -0.25f);
        ValueDomainMax = std::max(ValueDomainMax, Key.Value + Key.LeaveTangent * 0.25f);
    }

    if (std::abs(ValueDomainMax - ValueDomainMin) < MinDomainSize)
    {
        ValueDomainMax += 1.0f;
        ValueDomainMin -= 1.0f;
    }
    else
    {
        const float Padding = (ValueDomainMax - ValueDomainMin) * 0.15f;
        ValueDomainMin -= Padding;
        ValueDomainMax += Padding;
    }
}

void FEditorCameraSequenceWidget::ClampDomains()
{
    if (TimeDomainMax - TimeDomainMin < MinDomainSize)
    {
        TimeDomainMax = TimeDomainMin + MinDomainSize;
    }
    if (ValueDomainMax - ValueDomainMin < MinDomainSize)
    {
        ValueDomainMax = ValueDomainMin + MinDomainSize;
    }
}

FString FEditorCameraSequenceWidget::GetSelectedChannelName() const
{
    const TArray<FString>& ChannelNames = FCameraSequenceManager::Get().GetBuiltInChannelNames();
    if (SelectedChannelIndex < 0 || SelectedChannelIndex >= static_cast<int32>(ChannelNames.size()))
    {
        return FString();
    }

    return ChannelNames[SelectedChannelIndex];
}

bool FEditorCameraSequenceWidget::IsSelectedChannelFOV() const
{
    return GetSelectedChannelName() == "FOV";
}

FCameraSequenceChannel* FEditorCameraSequenceWidget::GetSelectedChannel()
{
    const TArray<FString>& ChannelNames = FCameraSequenceManager::Get().GetBuiltInChannelNames();
    if (SelectedChannelIndex < 0 || SelectedChannelIndex >= static_cast<int32>(ChannelNames.size()))
    {
        return nullptr;
    }

    auto It = CurrentAsset.Channels.find(ChannelNames[SelectedChannelIndex]);
    return It != CurrentAsset.Channels.end() ? &It->second : nullptr;
}

const FCameraSequenceChannel* FEditorCameraSequenceWidget::GetSelectedChannel() const
{
    const TArray<FString>& ChannelNames = FCameraSequenceManager::Get().GetBuiltInChannelNames();
    if (SelectedChannelIndex < 0 || SelectedChannelIndex >= static_cast<int32>(ChannelNames.size()))
    {
        return nullptr;
    }

    auto It = CurrentAsset.Channels.find(ChannelNames[SelectedChannelIndex]);
    return It != CurrentAsset.Channels.end() ? &It->second : nullptr;
}

void FEditorCameraSequenceWidget::ClampSelection()
{
    FCameraSequenceChannel* Channel = GetSelectedChannel();
    if (!Channel)
    {
        SelectedKeyIndex = -1;
        return;
    }

    if (Channel->Keys.empty())
    {
        SelectedKeyIndex = -1;
        return;
    }

    SelectedKeyIndex = std::clamp(SelectedKeyIndex, 0, static_cast<int32>(Channel->Keys.size()) - 1);
}

void FEditorCameraSequenceWidget::ResetInteractionState()
{
    ActiveKeyDragIndex = -1;
    ActiveLeaveHandleKeyIndex = -1;
    ActiveArriveHandleKeyIndex = -1;
}
