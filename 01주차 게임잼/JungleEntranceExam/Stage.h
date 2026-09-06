#pragma once
#include "UBlock.h"
#include <vector>

struct StageData
{
	float HALF_BW;
	float HALF_BH;
	float STEP_X;
	float STEP_Y;
	float START_X;
	float START_Y;
};

std::vector<UBlock*> CreateStage(int Round);

void GetStageInfo(int StageNum, int& outRow, int& outCol);

StageData GetStageData();