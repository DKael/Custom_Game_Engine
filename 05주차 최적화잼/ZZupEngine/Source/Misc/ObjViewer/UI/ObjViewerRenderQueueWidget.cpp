#include "ObjViewerRenderQueueWidget.h"

#include "Misc/ObjViewer/ObjViewerEngine.h" 
#include "Core/Logging/GPUProfiler.h"
#include "Engine/Runtime/Engine.h"
#include "Render/Renderer/Renderer.h"
#include "Render/Common/RenderTypes.h"
#include "ImGui/imgui.h"

#include <cfloat>

void FObjViewerRenderQueueWidget::Render(float DeltaTime)
{
#if STATS
	(void)DeltaTime;

	ImGui::SetNextWindowCollapsed(true, ImGuiCond_Once);
	ImGui::SetNextWindowSize(ImVec2(700.0f, 380.0f), ImGuiCond_Once);

	if (!ImGui::Begin("Render Queue Stats"))
	{
		ImGui::End();
		return;
	}

	// --- 컨트롤 ---
	if (bPaused)
	{
		if (ImGui::Button("Resume")) bPaused = false;
		ImGui::SameLine();
		ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.0f, 1.0f), "PAUSED");
	}
	else
	{
		if (ImGui::Button("Pause")) bPaused = true;
	}

	ImGui::SameLine(0.0f, 20.0f);
	ImGui::Checkbox("Active passes only", &bShowOnlyActive);

	if (bPaused)
	{
		ImGui::End();
		return;
	}

	// --- 패스 이름 테이블 (ERenderPass 순서와 일치해야 함) ---
	static const char* kPassLabels[] =
	{
		"Opaque",
		"Font",
		"SubUV",
		"Translucent",
		"SelectionMask",
		"Grid",
		"Editor",
		"DepthLess",
		"PostProcess",
	};
	static const char* kGPUNames[] =
	{
		"Pass_Opaque",
		"Pass_Font",
		"Pass_SubUV",
		"Pass_Translucent",
		"Pass_SelectionMask",
		"Pass_Grid",
		"Pass_Editor",
		"Pass_DepthLess",
		"Pass_PostProcess",
	};
	static_assert(
		sizeof(kPassLabels) / sizeof(kPassLabels[0]) == static_cast<size_t>(ERenderPass::MAX),
		"kPassLabels count must match ERenderPass::MAX"
	);

	// --- GPU 스냅샷에서 패스 이름으로 시간 조회 ---
	const TArray<FStatEntry>& GPUSnapshot = FGPUProfiler::Get().GetGPUSnapshot();
	auto FindGPUTime = [&](const char* Name) -> double
	{
		for (const FStatEntry& E : GPUSnapshot)
		{
			if (E.Name == Name) return E.LastTime * 1000.0; // → ms
		}
		return -1.0;
	};

	// --- FRenderer 에서 패스별 정렬 통계 획득 ---
	if (!Engine)
	{
		ImGui::TextDisabled("Engine not available.");
		ImGui::End();
		return;
	}

	const FRenderQueueStats* Stats = Engine->GetRenderer().GetPassStats();

	// --- 테이블 ---
	constexpr ImGuiTableFlags TableFlags =
		ImGuiTableFlags_Borders       |
		ImGuiTableFlags_RowBg         |
		ImGuiTableFlags_Resizable     |
		ImGuiTableFlags_ScrollY       |
		ImGuiTableFlags_SizingFixedFit;

	if (ImGui::BeginTable("RenderQueueTable", 7, TableFlags, ImVec2(0.0f, 280.0f)))
	{
		ImGui::TableSetupScrollFreeze(0, 1);
		ImGui::TableSetupColumn("Pass",          ImGuiTableColumnFlags_WidthFixed, 100.0f);
		ImGui::TableSetupColumn("Commands",      ImGuiTableColumnFlags_WidthFixed,  72.0f);
		ImGui::TableSetupColumn("TypeChanges",   ImGuiTableColumnFlags_WidthFixed,  80.0f);
		ImGui::TableSetupColumn("MeshRebinds",   ImGuiTableColumnFlags_WidthFixed,  80.0f);
		ImGui::TableSetupColumn("SRV Rebinds",   ImGuiTableColumnFlags_WidthFixed,  80.0f);
		ImGui::TableSetupColumn("CBuffer Upd",   ImGuiTableColumnFlags_WidthFixed,  80.0f);
		ImGui::TableSetupColumn("GPU (ms)",      ImGuiTableColumnFlags_WidthFixed,  70.0f);
		ImGui::TableHeadersRow();

		for (uint32 i = 0; i < (uint32)ERenderPass::MAX; ++i)
		{
			const FRenderQueueStats& S = Stats[i];

			if (bShowOnlyActive && S.CommandCount == 0) continue;

			ImGui::TableNextRow();

			// Pass 이름
			ImGui::TableSetColumnIndex(0);
			ImGui::TextUnformatted(kPassLabels[i]);

			// Commands
			ImGui::TableSetColumnIndex(1);
			ImGui::Text("%u", S.CommandCount);

			// TypeChanges — 셰이더 재바인딩
			ImGui::TableSetColumnIndex(2);
			if (S.TypeChangeCount > 0)
				ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.0f, 1.0f), "%u", S.TypeChangeCount);
			else
				ImGui::Text("0");

			// MeshRebinds — VB/IB 재바인딩
			ImGui::TableSetColumnIndex(3);
			if (S.MeshRebindCount > 1) // 첫 커맨드는 항상 1회 바인딩하므로 >1 부터 강조
				ImGui::TextColored(ImVec4(0.6f, 0.9f, 1.0f, 1.0f), "%u", S.MeshRebindCount);
			else
				ImGui::Text("%u", S.MeshRebindCount);

			// SRV Rebinds
			ImGui::TableSetColumnIndex(4);
			if (S.SRVRebindCount > 1)
				ImGui::TextColored(ImVec4(0.6f, 0.9f, 1.0f, 1.0f), "%u", S.SRVRebindCount);
			else
				ImGui::Text("%u", S.SRVRebindCount);

			// CBuffer Updates
			ImGui::TableSetColumnIndex(5);
			if (S.CBufferUpdateCount > 1)
				ImGui::TextColored(ImVec4(0.8f, 0.6f, 1.0f, 1.0f), "%u", S.CBufferUpdateCount);
			else
				ImGui::Text("%u", S.CBufferUpdateCount);

			// GPU Time
			ImGui::TableSetColumnIndex(6);
			double GpuMs = FindGPUTime(kGPUNames[i]);
			if (GpuMs >= 0.0)
				ImGui::Text("%.3f", GpuMs);
			else
				ImGui::TextDisabled("N/A");
		}

		ImGui::EndTable();
	}

	// --- 범례 ---
	ImGui::Spacing();
	ImGui::TextDisabled("Color guide:");
	ImGui::SameLine();
	ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.0f, 1.0f), "TypeChange(shader)  ");
	ImGui::SameLine();
	ImGui::TextColored(ImVec4(0.6f, 0.9f, 1.0f, 1.0f), "Mesh/SRV rebind  ");
	ImGui::SameLine();
	ImGui::TextColored(ImVec4(0.8f, 0.6f, 1.0f, 1.0f), "CBuffer update");

	ImGui::End();
#endif
}
