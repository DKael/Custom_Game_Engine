#pragma once

#include "Core/CoreMinimal.h"
#include "Core/Paths.h"
#include "Core/Singleton.h"
#include "Math/Vector.h"
#include "Render/Common/ViewTypes.h"

class FEditorSettings : public TSingleton<FEditorSettings>
{
	friend class TSingleton<FEditorSettings>;

public:
	// Viewport
	float CameraSpeed = 10.f;
	float CameraRotationSpeed = 60.f;
	float CameraZoomSpeed = 300.f;
	FVector InitViewPos = FVector(10, 0, 5);
	FVector InitLookAt = FVector(0, 0, 0);

	// Viewport 레이아웃 상태
	int32 ActiveViewportCount = 4;  // 현재 표시 중인 뷰포트 수 (1 또는 4)
	int32 SingleViewportIndex = 0;  // ActiveViewportCount == 1 일 때 표시할 뷰포트 인덱스

	// Splitter layout
	float SplitterVRatio = 0.5f;  // RootSplitterV (위:아래)
	float SplitterHRatio = 0.5f;  // SplitterH (좌:우)

	// View
	EViewMode ViewMode = EViewMode::Unlit;
	FShowFlags ShowFlags;
	FFXAASettings DefaultFXAASettings;

	// Grid
	float GridSpacing = 1.0f;
	int32 GridHalfLineCount = 100;
	float GridLineThickness = 1.0f;
	float GridMajorLineThickness = 1.35f;
	float GridMajorLineInterval = 10.0f;
	float GridMinorIntensity = 0.65f;
	float GridMajorIntensity = 1.0f;
	float GridRangeScale = 1.0f;
	float GridMaxDistanceScale = 1.5f;
	float AxisThickness = 1.75f;
	float AxisLengthScale = 1.0f;

	// Camera Sensitivity
	float CameraMoveSensitivity = 1.0f;
	float CameraRotateSensitivity = 1.0f;

	// Spatial index / BVH maintenance
	int32 SpatialBatchRefitMinDirtyCount = 8;
	int32 SpatialBatchRefitDirtyPercentThreshold = 15;
	int32 SpatialRotationStructuralChangeThreshold = 8;
	int32 SpatialRotationDirtyCountThreshold = 24;
	int32 SpatialRotationDirtyPercentThreshold = 30;

	// File paths
	FString DefaultSavePath = FPaths::ToUtf8(FPaths::SceneDir());

	void SaveToFile(const FString& Path) const;
	void LoadFromFile(const FString& Path);

	static FString GetDefaultSettingsPath() { return FPaths::ToUtf8(FPaths::SettingsFilePath()); }
};
