#pragma once

#include "Core/CoreTypes.h"

enum class EViewMode : int32
{
	Lit = 0,
	Unlit,
	Wireframe,
    DepthScene,
	Fog,
	Count
};

struct FFXAASettings
{
    float Subpix = 0.75f;
    float EdgeThreshold = 0.125f;
    float EdgeThresholdMin = 0.0312f;
};

struct FViewportPostProcessSettings
{
    bool bFXAAEnabled = false;
    bool bUseGlobalFXAASettings = true;
    FFXAASettings LocalFXAASettings;
};

struct FShowFlags
{
    bool bPrimitives = true;
    bool bGrid = true;
    bool bAxis = true;
    bool bGizmo = true;
    bool bBillboardText = false;
    bool bBoundingVolume = false;
    bool bBVHBoundingVolume = false;
    bool bFXAAEnabled = false;
    bool bDecal = false;
    bool bFog = false;
};
