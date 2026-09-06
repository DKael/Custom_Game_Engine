#pragma once

/*

	는 Renderer에게 Draw Call 요청을 vector의 형태로 전달하는 역할을 합니다.
	Renderer가 RenderBus에 담긴 Draw Call 요청들을 처리할 수 있게 합니다.
*/

//	TODO : CoreType.h 경로 변경 요구
#include "Core/CoreMinimal.h"
#include "Render/Scene/RenderCommand.h"
#include "Render/Scene/DebugCommand.h"
#include "Render/Scene/LightInfo.h"

#include "Render/Common/ViewTypes.h"


class FRenderBus
{
public:
	void Clear();
	void AddCommand(ERenderPass Pass, const FRenderCommand& InCommand);
	void AddCommand(ERenderPass Pass, FRenderCommand&& InCommand);
	void AddDebugCommand(ERenderPass Pass, FDebugRenderCommand& InCommand);
    void AddDebugCommand(ERenderPass Pass, FDebugRenderCommand&& InCommand);
	const TArray<FRenderCommand>& GetCommands(ERenderPass Pass) const;
    const TArray<FDebugRenderCommand>& GetDebugCommands(ERenderPass Pass) const;
    TArray<FDirectionalLightConstants> DirectionalLights;
    TArray<FPointLightConstatns>       PointlLights;

    const TArray<FDirectionalLightConstants>& GetDirectionalLights() const { return DirectionalLights; }
    void AddDirectionalLight(const FDirectionalLightConstants& InLight) { DirectionalLights.push_back(InLight); }
    const TArray<FSpotLightInfo>& GetSpotLightInfos() const { return SpotLightInfos; };
    void AddSpotLightInfo(const FSpotLightInfo& LightInfo) { SpotLightInfos.push_back(LightInfo); };

	const TArray<FPointLightConstatns>& GetPointlLights() const { return PointlLights; }
    void AddPointLight(const FPointLightConstatns& InLight) { PointlLights.push_back(InLight); }

	// Getter,Setter
	void SetViewProjection(const FMatrix& InView, const FMatrix& InProj);
	void SetRenderSettings(const EViewMode NewViewMode, const FShowFlags NewShowFlags);
	void SetFogConstants(const FFogConstants& InFog);
	void SetLightingConstants(const FLightingConstants& InLighting);

	const FFogConstants& GetFogConstants() const { return FogConstants; }
	const FLightingConstants& GetLightingConstants() const { return LightingConstants; }
	const FMatrix& GetView() const { return View; }
	const FMatrix& GetProj() const { return Proj; }
	const FVector& GetCameraPosition() const { return CameraPosition;  }
	const FVector& GetCameraForward() const { return CameraForward; }
	const FVector& GetCameraUp() const { return CameraUp; }
	const FVector& GetCameraRight() const { return CameraRight; }
	EViewMode GetViewMode() const { return ViewMode; }
	FShowFlags GetShowFlags() const { return ShowFlags; }
	const FVector& GetWireframeColor() const { return WireframeColor; }
	void SetWireframeColor(const FVector& InColor) { WireframeColor = InColor; }
	bool IsOrthographic() const { return Proj.M[3][3] == 1.0f; }


private:
	TArray<FRenderCommand> PassQueues[(uint32)ERenderPass::MAX];
	TArray<FDebugRenderCommand>  DebugCommandQueues[(uint32)ERenderPass::MAX];

	FMatrix View;
	FMatrix Proj;
	FVector CameraPosition;
	FVector CameraForward;
	FVector CameraRight;
	FVector CameraUp;

	//Editor Settings
	EViewMode ViewMode;
	FShowFlags ShowFlags;
	FVector WireframeColor = FVector(1.0f, 1.0f, 1.0f);

	FFogConstants FogConstants;
    FLightingConstants LightingConstants;
	TArray<FSpotLightInfo> SpotLightInfos;
};

