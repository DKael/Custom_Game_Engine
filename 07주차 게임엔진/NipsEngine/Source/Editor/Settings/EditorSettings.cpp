#include "Editor/Settings/EditorSettings.h"
#include "SimpleJSON/json.hpp"

#include <algorithm>
#include <fstream>
#include <filesystem>

namespace EditorKey
{
	// Section
	constexpr const char* Viewport = "Viewport";
	constexpr const char* Paths = "Paths";

	// Splitter / Layout
	constexpr const char* SplitterVRatio      = "SplitterVRatio";
	constexpr const char* SplitterHRatio      = "SplitterHRatio";
	constexpr const char* ActiveViewportCount = "ActiveViewportCount";
	constexpr const char* SingleViewportIndex = "SingleViewportIndex";

	// Viewport
	constexpr const char* CameraSpeed = "CameraSpeed";
	constexpr const char* CameraRotationSpeed = "CameraRotationSpeed";
	constexpr const char* CameraZoomSpeed = "CameraZoomSpeed";
	constexpr const char* CameraMoveSensitivity = "CameraMoveSensitivity";
	constexpr const char* CameraRotateSensitivity = "CameraRotateSensitivity";
	constexpr const char* InitViewPos = "InitViewPos";
	constexpr const char* InitLookAt = "InitLookAt";

	// Light
    constexpr const char* bDirectionalLightDebug = "bDirectionalLightDebug";
    constexpr const char* bPointLightDebug = "bPointLightDebug";
    constexpr const char* bSpotLightDebug = "bSpotLightDebug";
    constexpr const char* bShowLightHitmapOverlay = "bShowLightHitmapOverlay";
    constexpr const char* bLightCullingMode = "bLightCullingMode";

	// View
	constexpr const char* View = "View";
	constexpr const char* ViewMode = "ViewMode";
	constexpr const char* bPrimitives = "bPrimitives";
	constexpr const char* bGrid = "bGrid";
	constexpr const char* bAxis = "bAxis";
	constexpr const char* bGizmo = "bGizmo";
	constexpr const char* bBillboardText = "bBillboardText";
	constexpr const char* bBoundingVolume = "bBoundingVolume";
	constexpr const char* bBVHBoundingVolume = "bBVHBoundingVolume";
	constexpr const char* bDecal = "bDecal";

	// FXAA
	constexpr const char* FXAA = "FXAA";
	constexpr const char* Subpix = "Subpix";
	constexpr const char* EdgeThreshold = "EdgeThreshold";
	constexpr const char* EdgeThresholdMin = "EdgeThresholdMin";

	// Grid
	constexpr const char* Grid = "Grid";
	constexpr const char* GridSpacing = "GridSpacing";
	constexpr const char* GridHalfLineCount = "GridHalfLineCount";
	constexpr const char* GridLineThickness = "GridLineThickness";
	constexpr const char* GridMajorLineThickness = "GridMajorLineThickness";
	constexpr const char* GridMajorLineInterval = "GridMajorLineInterval";
	constexpr const char* GridMinorIntensity = "GridMinorIntensity";
	constexpr const char* GridMajorIntensity = "GridMajorIntensity";
	constexpr const char* GridRangeScale = "GridRangeScale";
	constexpr const char* GridMaxDistanceScale = "GridMaxDistanceScale";
	constexpr const char* AxisThickness = "AxisThickness";
	constexpr const char* AxisLengthScale = "AxisLengthScale";

	// Spatial index / BVH maintenance
	constexpr const char* SpatialIndex = "SpatialIndex";
	constexpr const char* BatchRefitMinDirtyCount = "BatchRefitMinDirtyCount";
	constexpr const char* BatchRefitDirtyPercentThreshold = "BatchRefitDirtyPercentThreshold";
	constexpr const char* RotationStructuralChangeThreshold = "RotationStructuralChangeThreshold";
	constexpr const char* RotationDirtyCountThreshold = "RotationDirtyCountThreshold";
	constexpr const char* RotationDirtyPercentThreshold = "RotationDirtyPercentThreshold";

	// Paths
	constexpr const char* DefaultSavePath = "DefaultSavePath";
}

void FEditorSettings::SaveToFile(const FString& Path) const
{
	using namespace json;

	JSON Root = Object();

	// Viewport
	JSON Viewport = Object();
	Viewport[EditorKey::CameraSpeed] = CameraSpeed;
	Viewport[EditorKey::CameraRotationSpeed] = CameraRotationSpeed;
	Viewport[EditorKey::CameraZoomSpeed] = CameraZoomSpeed;
	Viewport[EditorKey::CameraMoveSensitivity] = CameraMoveSensitivity;
	Viewport[EditorKey::CameraRotateSensitivity] = CameraRotateSensitivity;

	JSON InitPos = Array(InitViewPos.X, InitViewPos.Y, InitViewPos.Z);
	Viewport[EditorKey::InitViewPos] = InitPos;

	JSON LookAt = Array(InitLookAt.X, InitLookAt.Y, InitLookAt.Z);
	Viewport[EditorKey::InitLookAt] = LookAt;

	Viewport[EditorKey::SplitterVRatio]      = SplitterVRatio;
	Viewport[EditorKey::SplitterHRatio]      = SplitterHRatio;
	Viewport[EditorKey::ActiveViewportCount] = ActiveViewportCount;
	Viewport[EditorKey::SingleViewportIndex] = SingleViewportIndex;

	Root[EditorKey::Viewport] = Viewport;

	// View
	JSON ViewObj = Object();
	ViewObj[EditorKey::ViewMode] = static_cast<int32>(ViewMode);
	ViewObj[EditorKey::bPrimitives] = ShowFlags.bPrimitives;
	ViewObj[EditorKey::bGrid] = ShowFlags.bGrid;
	ViewObj[EditorKey::bAxis] = ShowFlags.bAxis;
	ViewObj[EditorKey::bGizmo] = ShowFlags.bGizmo;
	ViewObj[EditorKey::bBillboardText] = ShowFlags.bBillboardText;
	ViewObj[EditorKey::bBoundingVolume] = ShowFlags.bBoundingVolume;
	ViewObj[EditorKey::bBVHBoundingVolume] = ShowFlags.bBVHBoundingVolume;
	ViewObj[EditorKey::bDecal] = ShowFlags.bDecal;
	Root[EditorKey::View] = ViewObj;

	// Light
    ViewObj[EditorKey::bDirectionalLightDebug] = ShowFlags.bDirectionalLightDebug;
    ViewObj[EditorKey::bPointLightDebug] = ShowFlags.bPointLightDebug;
    ViewObj[EditorKey::bSpotLightDebug] = ShowFlags.bSpotLightDebug;
    ViewObj[EditorKey::bShowLightHitmapOverlay] = ShowFlags.bShowLightHitmapOverlay;

	//FXAA
	JSON FXAAObj = Object();
	FXAAObj[EditorKey::Subpix] = DefaultFXAASettings.Subpix;
	FXAAObj[EditorKey::EdgeThreshold] = DefaultFXAASettings.EdgeThreshold;
	FXAAObj[EditorKey::EdgeThresholdMin] = DefaultFXAASettings.EdgeThresholdMin;
	Root[EditorKey::FXAA] = FXAAObj;

	// Grid
	JSON GridObj = Object();
	GridObj[EditorKey::GridSpacing] = GridSpacing;
	GridObj[EditorKey::GridHalfLineCount] = GridHalfLineCount;
	GridObj[EditorKey::GridLineThickness] = GridLineThickness;
	GridObj[EditorKey::GridMajorLineThickness] = GridMajorLineThickness;
	GridObj[EditorKey::GridMajorLineInterval] = GridMajorLineInterval;
	GridObj[EditorKey::GridMinorIntensity] = GridMinorIntensity;
	GridObj[EditorKey::GridMajorIntensity] = GridMajorIntensity;
	GridObj[EditorKey::GridRangeScale] = GridRangeScale;
	GridObj[EditorKey::GridMaxDistanceScale] = GridMaxDistanceScale;
	GridObj[EditorKey::AxisThickness] = AxisThickness;
	GridObj[EditorKey::AxisLengthScale] = AxisLengthScale;
	Root[EditorKey::Grid] = GridObj;

	// Spatial index / BVH maintenance
	JSON SpatialIndexObj = Object();
	SpatialIndexObj[EditorKey::BatchRefitMinDirtyCount] = SpatialBatchRefitMinDirtyCount;
	SpatialIndexObj[EditorKey::BatchRefitDirtyPercentThreshold] = SpatialBatchRefitDirtyPercentThreshold;
	SpatialIndexObj[EditorKey::RotationStructuralChangeThreshold] = SpatialRotationStructuralChangeThreshold;
	SpatialIndexObj[EditorKey::RotationDirtyCountThreshold] = SpatialRotationDirtyCountThreshold;
	SpatialIndexObj[EditorKey::RotationDirtyPercentThreshold] = SpatialRotationDirtyPercentThreshold;
	Root[EditorKey::SpatialIndex] = SpatialIndexObj;

	// Paths
	JSON PathsObj = Object();
	PathsObj[EditorKey::DefaultSavePath] = DefaultSavePath;
	Root[EditorKey::Paths] = PathsObj;

	// Ensure directory exists
	std::filesystem::path FilePath(FPaths::ToWide(Path));
	if (FilePath.has_parent_path())
	{
		std::filesystem::create_directories(FilePath.parent_path());
	}

	std::ofstream File(FilePath);
	if (File.is_open())
	{
		File << Root;
	}
}

void FEditorSettings::LoadFromFile(const FString& Path)
{
	using namespace json;

	std::ifstream File(std::filesystem::path(FPaths::ToWide(Path)));
	if (!File.is_open())
	{
		return;
	}

	FString Content((std::istreambuf_iterator<char>(File)),
		std::istreambuf_iterator<char>());

	JSON Root = JSON::Load(Content);

	// Viewport
	if (Root.hasKey(EditorKey::Viewport))
	{
		JSON Viewport = Root[EditorKey::Viewport];

		if (Viewport.hasKey(EditorKey::CameraSpeed))
			CameraSpeed = static_cast<float>(Viewport[EditorKey::CameraSpeed].ToFloat());
		if (Viewport.hasKey(EditorKey::CameraRotationSpeed))
			CameraRotationSpeed = static_cast<float>(Viewport[EditorKey::CameraRotationSpeed].ToFloat());
		if (Viewport.hasKey(EditorKey::CameraZoomSpeed))
			CameraZoomSpeed = static_cast<float>(Viewport[EditorKey::CameraZoomSpeed].ToFloat());
		if (Viewport.hasKey(EditorKey::CameraMoveSensitivity))
			CameraMoveSensitivity = static_cast<float>(Viewport[EditorKey::CameraMoveSensitivity].ToFloat());
		if (Viewport.hasKey(EditorKey::CameraRotateSensitivity))
			CameraRotateSensitivity = static_cast<float>(Viewport[EditorKey::CameraRotateSensitivity].ToFloat());

		if (Viewport.hasKey(EditorKey::InitViewPos))
		{
			JSON Pos = Viewport[EditorKey::InitViewPos];
			InitViewPos = FVector(
				static_cast<float>(Pos[0].ToFloat()),
				static_cast<float>(Pos[1].ToFloat()),
				static_cast<float>(Pos[2].ToFloat()));
		}

		if (Viewport.hasKey(EditorKey::InitLookAt))
		{
			JSON Look = Viewport[EditorKey::InitLookAt];
			InitLookAt = FVector(
				static_cast<float>(Look[0].ToFloat()),
				static_cast<float>(Look[1].ToFloat()),
				static_cast<float>(Look[2].ToFloat()));
		}

		if (Viewport.hasKey(EditorKey::SplitterVRatio))
			SplitterVRatio = static_cast<float>(Viewport[EditorKey::SplitterVRatio].ToFloat());
		if (Viewport.hasKey(EditorKey::SplitterHRatio))
			SplitterHRatio = static_cast<float>(Viewport[EditorKey::SplitterHRatio].ToFloat());

		if (Viewport.hasKey(EditorKey::ActiveViewportCount))
		{
			const int32 Count = Viewport[EditorKey::ActiveViewportCount].ToInt();
			ActiveViewportCount = (Count == 1) ? 1 : 4;  // 1 또는 4만 유효
		}
		if (Viewport.hasKey(EditorKey::SingleViewportIndex))
		{
			const int32 Idx = Viewport[EditorKey::SingleViewportIndex].ToInt();
			SingleViewportIndex = (Idx >= 0 && Idx < 4) ? Idx : 0;
		}
	}

	// View
	if (Root.hasKey(EditorKey::View))
	{
		JSON ViewObj = Root[EditorKey::View];

		if (ViewObj.hasKey(EditorKey::ViewMode))
		{
			int32 Mode = ViewObj[EditorKey::ViewMode].ToInt();
			if (Mode >= 0 && Mode < static_cast<int32>(EViewMode::Count))
				ViewMode = static_cast<EViewMode>(Mode);
		}
		if (ViewObj.hasKey(EditorKey::bPrimitives))
			ShowFlags.bPrimitives = ViewObj[EditorKey::bPrimitives].ToBool();
		if (ViewObj.hasKey(EditorKey::bGrid))
			ShowFlags.bGrid = ViewObj[EditorKey::bGrid].ToBool();
		if (ViewObj.hasKey(EditorKey::bAxis))
			ShowFlags.bAxis = ViewObj[EditorKey::bAxis].ToBool();
		if (ViewObj.hasKey(EditorKey::bGizmo))
			ShowFlags.bGizmo = ViewObj[EditorKey::bGizmo].ToBool();
		if (ViewObj.hasKey(EditorKey::bDirectionalLightDebug))
			ShowFlags.bDirectionalLightDebug = ViewObj[EditorKey::bDirectionalLightDebug].ToBool();
		if (ViewObj.hasKey(EditorKey::bPointLightDebug))
			ShowFlags.bPointLightDebug = ViewObj[EditorKey::bPointLightDebug].ToBool();
		if (ViewObj.hasKey(EditorKey::bSpotLightDebug))
			ShowFlags.bSpotLightDebug = ViewObj[EditorKey::bSpotLightDebug].ToBool();
		if (ViewObj.hasKey(EditorKey::bShowLightHitmapOverlay))
			ShowFlags.bShowLightHitmapOverlay = ViewObj[EditorKey::bShowLightHitmapOverlay].ToBool();
        if (ViewObj.hasKey(EditorKey::bLightCullingMode))
            ShowFlags.bLightCullingMode = ViewObj[EditorKey::bLightCullingMode].ToBool();
		if (ViewObj.hasKey(EditorKey::bBillboardText))
			ShowFlags.bBillboardText = ViewObj[EditorKey::bBillboardText].ToBool();
		if (ViewObj.hasKey(EditorKey::bBoundingVolume))
			ShowFlags.bBoundingVolume = ViewObj[EditorKey::bBoundingVolume].ToBool();
		if (ViewObj.hasKey(EditorKey::bBVHBoundingVolume))
			ShowFlags.bBVHBoundingVolume = ViewObj[EditorKey::bBVHBoundingVolume].ToBool();
		if (ViewObj.hasKey(EditorKey::bDecal))
			ShowFlags.bDecal = ViewObj[EditorKey::bDecal].ToBool();
	}

	if (Root.hasKey(EditorKey::FXAA))
	{
		JSON FXAAObj = Root[EditorKey::FXAA];

		if (FXAAObj.hasKey(EditorKey::Subpix))
			DefaultFXAASettings.Subpix = static_cast<float>(FXAAObj[EditorKey::Subpix].ToFloat());
		if (FXAAObj.hasKey(EditorKey::EdgeThreshold))
			DefaultFXAASettings.EdgeThreshold = static_cast<float>(FXAAObj[EditorKey::EdgeThreshold].ToFloat());
		if (FXAAObj.hasKey(EditorKey::EdgeThresholdMin))
			DefaultFXAASettings.EdgeThresholdMin = static_cast<float>(FXAAObj[EditorKey::EdgeThresholdMin].ToFloat());
	}

	// Grid
	if (Root.hasKey(EditorKey::Grid))
	{
		JSON GridObj = Root[EditorKey::Grid];

		if (GridObj.hasKey(EditorKey::GridSpacing))
			GridSpacing = static_cast<float>(GridObj[EditorKey::GridSpacing].ToFloat());
		if (GridObj.hasKey(EditorKey::GridHalfLineCount))
			GridHalfLineCount = GridObj[EditorKey::GridHalfLineCount].ToInt();
		if (GridObj.hasKey(EditorKey::GridLineThickness))
			GridLineThickness = static_cast<float>(GridObj[EditorKey::GridLineThickness].ToFloat());
		if (GridObj.hasKey(EditorKey::GridMajorLineThickness))
			GridMajorLineThickness = static_cast<float>(GridObj[EditorKey::GridMajorLineThickness].ToFloat());
		if (GridObj.hasKey(EditorKey::GridMajorLineInterval))
			GridMajorLineInterval = static_cast<float>(GridObj[EditorKey::GridMajorLineInterval].ToFloat());
		if (GridObj.hasKey(EditorKey::GridMinorIntensity))
			GridMinorIntensity = static_cast<float>(GridObj[EditorKey::GridMinorIntensity].ToFloat());
		if (GridObj.hasKey(EditorKey::GridMajorIntensity))
			GridMajorIntensity = static_cast<float>(GridObj[EditorKey::GridMajorIntensity].ToFloat());
		if (GridObj.hasKey(EditorKey::GridRangeScale))
			GridRangeScale = static_cast<float>(GridObj[EditorKey::GridRangeScale].ToFloat());
		if (GridObj.hasKey(EditorKey::GridMaxDistanceScale))
			GridMaxDistanceScale = static_cast<float>(GridObj[EditorKey::GridMaxDistanceScale].ToFloat());
		if (GridObj.hasKey(EditorKey::AxisThickness))
			AxisThickness = static_cast<float>(GridObj[EditorKey::AxisThickness].ToFloat());
		if (GridObj.hasKey(EditorKey::AxisLengthScale))
			AxisLengthScale = static_cast<float>(GridObj[EditorKey::AxisLengthScale].ToFloat());
	}

	// Spatial index / BVH maintenance
	if (Root.hasKey(EditorKey::SpatialIndex))
	{
		JSON SpatialIndexObj = Root[EditorKey::SpatialIndex];

		if (SpatialIndexObj.hasKey(EditorKey::BatchRefitMinDirtyCount))
		{
			const int32 Value = static_cast<int32>(SpatialIndexObj[EditorKey::BatchRefitMinDirtyCount].ToInt());
			SpatialBatchRefitMinDirtyCount = std::max<int32>(1, Value);
		}
		if (SpatialIndexObj.hasKey(EditorKey::BatchRefitDirtyPercentThreshold))
		{
			const int32 Value =
				static_cast<int32>(SpatialIndexObj[EditorKey::BatchRefitDirtyPercentThreshold].ToInt());
			SpatialBatchRefitDirtyPercentThreshold =
				std::clamp<int32>(Value, 1, 100);
		}
		if (SpatialIndexObj.hasKey(EditorKey::RotationStructuralChangeThreshold))
		{
			const int32 Value =
				static_cast<int32>(SpatialIndexObj[EditorKey::RotationStructuralChangeThreshold].ToInt());
			SpatialRotationStructuralChangeThreshold =
				std::max<int32>(1, Value);
		}
		if (SpatialIndexObj.hasKey(EditorKey::RotationDirtyCountThreshold))
		{
			const int32 Value = static_cast<int32>(SpatialIndexObj[EditorKey::RotationDirtyCountThreshold].ToInt());
			SpatialRotationDirtyCountThreshold =
				std::max<int32>(1, Value);
		}
		if (SpatialIndexObj.hasKey(EditorKey::RotationDirtyPercentThreshold))
		{
			const int32 Value =
				static_cast<int32>(SpatialIndexObj[EditorKey::RotationDirtyPercentThreshold].ToInt());
			SpatialRotationDirtyPercentThreshold =
				std::clamp<int32>(Value, 1, 100);
		}
	}

	// Paths
	if (Root.hasKey(EditorKey::Paths))
	{
		JSON PathsObj = Root[EditorKey::Paths];

		if (PathsObj.hasKey(EditorKey::DefaultSavePath))
			DefaultSavePath = PathsObj[EditorKey::DefaultSavePath].ToString();
	}
}
