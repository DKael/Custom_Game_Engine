#include "Editor/UI/EditorMainPanel.h"

#include "Editor/Document/EditorDocument.h"
#include "Editor/EditorEngine.h"
#include "Editor/UI/EditorViewerWindowWidget.h"
#include "Editor/Viewer/EditorViewer.h"
#include "Editor/Viewport/FSceneViewport.h"
#include "Editor/Viewport/ViewportLayout.h"
#include "Component/GizmoComponent.h"
#include "Engine/Input/InputSystem.h"
#include "Engine/Runtime/WindowsWindow.h"

#include "ImGui/imgui.h"
#include "ImGui/imgui_internal.h"

#include <cstring>
#include <imm.h>

namespace
{
	bool IsWindowInHierarchy(ImGuiWindow* Candidate, ImGuiWindow* Root)
	{
		for (ImGuiWindow* Window = Candidate; Window; Window = Window->ParentWindow)
		{
			if (Window == Root)
			{
				return true;
			}
			if (Window->RootWindow == Root || Window->RootWindowDockTree == Root)
			{
				return true;
			}
		}
		return false;
	}

	bool IsViewportContentWindow(ImGuiWindow* Window)
	{
		if (!Window)
		{
			return false;
		}

		const char* WindowName = Window->Name ? Window->Name : "";
		if (std::strcmp(WindowName, "Viewport") == 0 ||
			std::strncmp(WindowName, "Viewport###", 11) == 0 ||
			std::strncmp(WindowName, "Viewer##", 8) == 0 ||
			std::strcmp(WindowName, "ViewportPanel") == 0)
		{
			return true;
		}

		if (ImGuiWindow* ViewportRoot = ImGui::FindWindowByName("Viewport"))
		{
			return IsWindowInHierarchy(Window, ViewportRoot);
		}
		return false;
	}

	bool CanExecuteLevelEditorSceneCommand(UEditorEngine* EditorEngine)
	{
		return EditorEngine && EditorEngine->GetEditorState() == EViewportPlayState::Editing;
	}

	bool HasActiveViewerViewportOperation(UEditorEngine* EditorEngine)
	{
		if (!EditorEngine)
		{
			return false;
		}

		for (const std::unique_ptr<FEditorViewer>& Viewer : EditorEngine->GetViewers())
		{
			if (!Viewer)
			{
				continue;
			}
			if (!EditorEngine->GetMainPanel().ShouldRouteViewerViewportInput(Viewer.get()))
			{
				continue;
			}

			FEditorViewportClient& Client = Viewer->GetClient();
			UGizmoComponent* Gizmo = Client.GetGizmo();
			if (Gizmo && (Gizmo->IsHolding() || Gizmo->IsPressedOnHandle()))
			{
				return true;
			}
		}
		return false;
	}
}

void FEditorMainPanel::BuildActiveEditorCommandList(FEditorCommandList& OutCommands)
{
	OutCommands.Clear();

	const FEditorTabId RoutingTabId = GetInputRoutingTabId();
	if (FEditorDocument* Document = FindDocumentByTabId(RoutingTabId))
	{
		Document->BuildCommandList(OutCommands);
		return;
	}

	if (RoutingTabId.Kind == EEditorTabKind::SkeletalMeshViewer ||
		RoutingTabId.Kind == EEditorTabKind::StaticMeshViewer)
	{
		FEditorViewerWindowWidget* ViewerWidget = FindViewerWidgetForTab(RoutingTabId);
		if (!ViewerWidget)
		{
			return;
		}

		OutCommands.MapAction(
			EEditorCommandId::Save,
			{ static_cast<int32>(ImGuiKey_S), true, false, false },
			[ViewerWidget]()
			{
				ViewerWidget->RequestSaveMesh();
			},
			[ViewerWidget]()
			{
				return ViewerWidget->CanSaveMesh();
			});
		return;
	}

	if (RoutingTabId.Kind != EEditorTabKind::LevelEditor || !RoutingTabId.PayloadId.empty())
	{
		return;
	}

	// 현재 Level Editor 문맥에서만 Scene 저장 명령을 받는다.
	OutCommands.MapAction(
		EEditorCommandId::Save,
		{ static_cast<int32>(ImGuiKey_S), true, false, false },
		[this]()
		{
			RequestSaveScene();
		},
		[this]()
		{
			return CanExecuteLevelEditorSceneCommand(EditorEngine);
		});

	OutCommands.MapAction(
		EEditorCommandId::SaveAs,
		{ static_cast<int32>(ImGuiKey_S), true, true, false },
		[this]()
		{
			RequestSaveSceneAsWithDialog();
		},
		[this]()
		{
			return CanExecuteLevelEditorSceneCommand(EditorEngine);
		});
}

bool FEditorMainPanel::ExecuteActiveEditorShortcut(const FEditorShortcut& Shortcut)
{
	FEditorCommandList Commands;
	BuildActiveEditorCommandList(Commands);
	return Commands.TryExecuteShortcut(Shortcut);
}

bool FEditorMainPanel::ExecuteActiveEditorCommand(EEditorCommandId CommandId)
{
	FEditorCommandList Commands;
	BuildActiveEditorCommandList(Commands);
	return Commands.TryExecuteCommand(CommandId);
}

void FEditorMainPanel::Update()
{
    ImGuiIO& IO = ImGui::GetIO();

    FEditorViewportLayout& Layout = EditorEngine->GetViewportLayout();
    const bool bMouseOverContentBrowser = Widgets.ContentBrowserWidget.IsMouseOverBrowser();
    bool bViewportOperationActive =
        (Layout.HasActiveOperationViewport() || HasActiveViewerViewportOperation(EditorEngine)) &&
        !bMouseOverContentBrowser;

    if (bViewportOperationActive)
    {
        IO.ConfigFlags |= ImGuiConfigFlags_NoMouse;
    }
    else
    {
        IO.ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
    }

    bool bWantMouse = bViewportOperationActive ? false : IO.WantCaptureMouse;
    bool bWantKeyboard = IO.WantCaptureKeyboard;
    const bool bWantTextInput = IO.WantTextInput;
    const bool bAnyUIItemActive = ImGui::IsAnyItemActive();
    const bool bAnyUIItemHovered = ImGui::IsAnyItemHovered();
    const bool bAnyPopupOpen = ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId);
    const bool bAnyDragDropActive = ImGui::GetDragDropPayload() != nullptr;
    const bool bAnyWindowHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow);
    const bool bAnyWindowFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow);

    bool bMouseOverViewportRect = false;
    if (Window)
    {
        POINT MouseClientPos = Window->ScreenToClientPoint(InputSystem::Get().GetMousePos());
		if (ShouldRouteLevelViewportInput())
		{
			for (int32 i = 0; i < FEditorViewportLayout::MaxViewports; ++i)
			{
				const FViewportRect& ViewportRect = Layout.GetSceneViewport(i).GetRect();
				if (ViewportRect.Width > 0 && ViewportRect.Height > 0 && ViewportRect.Contains(MouseClientPos.x, MouseClientPos.y))
				{
					bMouseOverViewportRect = true;
					break;
				}
			}
		}

		TArray<std::unique_ptr<FEditorViewer>>& Viewers = EditorEngine->GetViewers();
		for (size_t i = 0; i < Viewers.size(); i++)
		{
			if (!ShouldRouteViewerViewportInput(Viewers[i].get()))
			{
				continue;
			}

            const FViewportRect& ViewportRect = Viewers[i]->GetViewport().GetRect();
            if (ViewportRect.Width > 0 && ViewportRect.Height > 0 && ViewportRect.Contains(MouseClientPos.x, MouseClientPos.y))
            {
                bMouseOverViewportRect = true;
                break;
            }
		}

        if (!bMouseOverViewportRect)
        {
            if (FEditorDocument* ActiveDocument = GetActiveEditorDocument())
            {
                if (const FSceneViewport* DocumentViewport = ActiveDocument->GetSceneViewport())
                {
                    const FViewportRect& ViewportRect = DocumentViewport->GetRect();
                    if (ViewportRect.Width > 0 && ViewportRect.Height > 0 && ViewportRect.Contains(MouseClientPos.x, MouseClientPos.y))
                    {
                        bMouseOverViewportRect = true;
                    }
                }
            }
        }
    }

    bool bHoveredViewportContentWindow = false;
    bool bHoveredNonViewportWindow = false;
    if (ImGuiContext* Context = ImGui::GetCurrentContext())
    {
        if (ImGuiWindow* HoveredWindow = Context->HoveredWindow)
        {
            bHoveredViewportContentWindow = IsViewportContentWindow(HoveredWindow);
            bHoveredNonViewportWindow = !bHoveredViewportContentWindow;
        }
    }

    // Viewport input ownership is decided by the routed viewport rect first.
    // ImGui child window names differ between embedded and detached documents, so
    // name-based hover checks are only a hint.
    if (bMouseOverViewportRect
        && !bMouseOverContentBrowser
        && (!bAnyWindowHovered || bHoveredViewportContentWindow)
        && !bAnyUIItemActive
        && !bAnyPopupOpen
        && !bAnyDragDropActive)
    {
        bHoveredViewportContentWindow = true;
        bHoveredNonViewportWindow = false;
    }

    if (bMouseOverContentBrowser)
    {
        bHoveredViewportContentWindow = false;
        bHoveredNonViewportWindow = true;
    }

    const bool bForcePIEViewportInputFocus =
        PIEViewportState.PendingInputFocusFrames > 0
        && EditorEngine
        && EditorEngine->GetEditorState() == EViewportPlayState::Playing;

    const bool bReleaseMouseToViewport =
        bMouseOverViewportRect
        && !bHoveredNonViewportWindow
        && !bAnyUIItemActive
        && !bAnyDragDropActive
        && !bAnyPopupOpen;
    const bool bNonViewportImGuiInteraction =
        bMouseOverContentBrowser
        ||
        (bHoveredNonViewportWindow && (bAnyWindowHovered || bAnyWindowFocused || bAnyUIItemActive || bAnyUIItemHovered || bAnyPopupOpen || bWantTextInput || bWantKeyboard))
        || bAnyUIItemActive
        || bAnyDragDropActive
        || bAnyPopupOpen;

    if (bNonViewportImGuiInteraction)
    {
        bWantMouse = true;
    }
    else if (EditorEngine && bReleaseMouseToViewport)
    {
        bWantMouse = false;
        bWantKeyboard = false;
    }

    if (bForcePIEViewportInputFocus)
    {
        bWantMouse = false;
        bWantKeyboard = false;
    }

    InputSystem::Get().SetGuiMouseCapture(bWantMouse);
    InputSystem::Get().SetGuiKeyboardCapture(bWantKeyboard);
    InputSystem::Get().SetGuiTextInputCapture(bForcePIEViewportInputFocus ? false : bWantTextInput);
    const bool bAllowViewportMouseFocus =
        (bForcePIEViewportInputFocus || bMouseOverViewportRect) &&
        !bHoveredNonViewportWindow &&
        !bAnyPopupOpen &&
        !bAnyDragDropActive &&
        !bWantTextInput;
    InputSystem::Get().SetGuiViewportMouseBlock(
        bForcePIEViewportInputFocus
            ? false
            : (bAnyDragDropActive ||
               bAnyPopupOpen ||
               bMouseOverContentBrowser ||
               bHoveredNonViewportWindow));
    InputSystem::Get().SetGuiViewportMouseFocusAllowed(bAllowViewportMouseFocus);

    if (bForcePIEViewportInputFocus)
    {
        --PIEViewportState.PendingInputFocusFrames;
    }

    if (EditorEngine && InputSystem::Get().GetKeyUp('F') && !IO.WantTextInput)
    {
        FEditorViewportLayout& FocusLayout = EditorEngine->GetViewportLayout();
        const int32 FocusedIdx = FocusLayout.GetLastFocusedViewportIndex();
        FocusLayout.GetViewportClient(FocusedIdx)->FocusSelection();
    }

    bool bHandledActiveCtrlShortcut = false;
    if (!IO.WantTextInput &&
        !bAnyUIItemActive &&
        !bAnyPopupOpen &&
        !bAnyDragDropActive &&
        !bMouseOverContentBrowser &&
        IO.KeyCtrl)
    {
        auto TryExecuteCtrlShortcut = [&](ImGuiKey Key)
        {
            if (!ImGui::IsKeyPressed(Key, false))
            {
                return false;
            }

            return ExecuteActiveEditorShortcut(
                {
                    static_cast<int32>(Key),
                    true,
                    IO.KeyShift,
                    IO.KeyAlt
                });
        };

        bHandledActiveCtrlShortcut =
            TryExecuteCtrlShortcut(ImGuiKey_C) ||
            TryExecuteCtrlShortcut(ImGuiKey_D) ||
            TryExecuteCtrlShortcut(ImGuiKey_V);
    }

    if (!bHandledActiveCtrlShortcut &&
        !IO.WantTextInput &&
        !bAnyUIItemActive &&
        !bAnyPopupOpen &&
        !bAnyDragDropActive &&
        !bMouseOverContentBrowser &&
        ImGui::IsKeyPressed(ImGuiKey_Delete, false))
    {
        ExecuteActiveEditorCommand(EEditorCommandId::DeleteSelection);
    }

    if (Window)
    {
        HWND hWnd = Window->GetHWND();
        if (IO.WantTextInput)
        {
            ImmAssociateContextEx(hWnd, NULL, IACE_DEFAULT);
        }
        else
        {
            ImmAssociateContext(hWnd, NULL);
        }
    }
}
