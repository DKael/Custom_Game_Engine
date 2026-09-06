#include "FileInfo.h"
#include <fstream>

void HightScoreUpdate(int score)
{
	std::ifstream infile("Score.dat");
	int highScore = 0;
	if (infile.is_open())
	{
		infile >> highScore;
		infile.close();
	}
	if (score > highScore)
	{
		std::ofstream outfile("Score.dat");
		if (outfile.is_open())
		{
			outfile << score;
			outfile.close();
		}
	}
}

int GetHightScore()
{
	std::ifstream infile("Score.dat");
	int highScore = 0;
	if (infile.is_open())
	{
		infile >> highScore;
		infile.close();
	}
	return highScore;
}
