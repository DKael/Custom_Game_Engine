#pragma once
#ifndef __FILE_INFO_H__
#define __FILE_INFO_H__


typedef struct ScoreData
{
	int HighScore;
} score;

void HightScoreUpdate(int score);
int GetHightScore();
#endif