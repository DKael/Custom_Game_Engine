#include "ObjViewerControlWidget.h"
#include "Misc/ObjViewer/ObjViewerEngine.h"
#include "Misc/ObjViewer/Viewport/ObjViewerViewportClient.h"
#include "Misc/ObjViewer/Settings/ObjViewerSettings.h"
#include "Viewport/ViewportCamera.h"
#include "Engine/Geometry/Transform.h"
#include "ImGui/imgui.h"

void FObjViewerControlWidget::Render(float DeltaTime)
{
	if (!Engine) return;

	FViewportCamera* Camera = Engine->GetCamera();
	FObjViewerSettings& Settings = FObjViewerSettings::Get();
	if (!Camera) return;

	// 우측 상단 배치 및 기본 크기 설정
	const ImGuiViewport* Viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(ImVec2(Viewport->WorkPos.x + Viewport->WorkSize.x - 350.0f, Viewport->WorkPos.y), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(350.0f, 400.0f), ImGuiCond_FirstUseEver);

	// 같은 이름("ObjViewer Panel")을 사용하여 StatWidget과 창을 통합할 수 있다.
	if (ImGui::Begin("ObjViewer Panel")) 
	{
		if (ImGui::CollapsingHeader("Camera Settings", ImGuiTreeNodeFlags_DefaultOpen))
		{
			FVector CamPos = Camera->GetLocation();
			ImGui::Text("[Position] X: %.2f, Y: %.2f, Z: %.2f", CamPos.X, CamPos.Y, CamPos.Z);
			
			ImGui::DragFloat("Move Speed", &Settings.CameraMoveSensitivity, 0.001f, 0.01f, 1.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
            ImGui::DragFloat("Rotation Speed", &Settings.CameraRotateSensitivity, 0.01f, 0.01f, 2.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
		}

		if (ImGui::CollapsingHeader("View Mode", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Checkbox("Show Grid", &Settings.ShowFlags.bGrid);
			ImGui::Checkbox("Show Axis", &Settings.ShowFlags.bAxis);
			ImGui::Checkbox("Occlusion Culling", &Settings.bOcclusionCulling);

#if _DEBUG
			bool bWireframe = (Settings.ViewMode == EViewMode::Wireframe);
			if (ImGui::Checkbox("Wireframe Mode", &bWireframe))
			{
				Settings.ViewMode = bWireframe ? EViewMode::Wireframe : EViewMode::Lit;
			}

			ImGui::Checkbox("BLAS (BVH)", &Settings.ShowFlags.bBLAS);
#endif
		}

		if (ImGui::CollapsingHeader("Occlusion Stats", ImGuiTreeNodeFlags_DefaultOpen))
		{
			const FOcclusionDebugState& Occlusion = Engine->GetRenderer().GetOcclusionDebugState();
			ImGui::Text("Enabled: %s", Occlusion.bEnabled ? "Yes" : "No");
			ImGui::Text("Eligible: %s", Occlusion.bEligible ? "Yes" : "No");
			ImGui::Text("Using Cache: %s", Occlusion.bUsingCachedResults ? "Yes" : "No");
			ImGui::Text("Submitted This Frame: %s", Occlusion.bSubmittedThisFrame ? "Yes" : "No");
			ImGui::Text("Readback Pending: %s", Occlusion.bReadbackPending ? "Yes" : "No");
			ImGui::Text("Has Readback Result: %s", Occlusion.bHasReadbackResult ? "Yes" : "No");
			ImGui::Text("Has Cached Result: %s", Occlusion.bHasCachedResults ? "Yes" : "No");
			ImGui::Text("Accepted Cull Result: %s", Occlusion.bAcceptedCullResult ? "Yes" : "No");
			ImGui::Text("Visible Commands: %u", Occlusion.VisibleCount);
			ImGui::Text("Proposed Survivors: %u", Occlusion.ProposedSurvivorCount);
			ImGui::Text("Cached Survivors: %u", Occlusion.CachedSurvivorCount);
			ImGui::Text("Cull Savings: %u", Occlusion.CullSavings);
			ImGui::Text("Required Savings: %u", Occlusion.RequiredSavings);
			ImGui::Text("Last Submitted: %u", Occlusion.LastSubmittedCount);
		}
	}
	ImGui::End();
}
