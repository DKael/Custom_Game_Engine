#include "Stage.h"
//Grid?

static const float HALF_BW = 0.065f;// (2.0f - a)/ 13.0f / 2.0f
static const float HALF_BH = 0.035f;// HALF_BW * 0.5f +a
static const float STEP_X = HALF_BW * 2.0f;//-+a // Row gap
static const float STEP_Y = HALF_BH * 2.0f;//+a   // Col gap
static const float START_X = 2 * HALF_BW;// +a // first block center x
static const float START_Y = 2 * HALF_BH;// +a // first block center y


#define N(c) (c) 
#define H 100
#define I 200
#define _ 0

static EBlockColor ColorTable[]=
{ 

	EBlockColor::NONE,		//0
	EBlockColor::Orange,	//1
	EBlockColor::Cyan,		//2
	EBlockColor::Green,		//3
	EBlockColor::Red,		//4
	EBlockColor::Blue,		//5
	EBlockColor::Magenta,	//6
	EBlockColor::Yellow,	//7.
	EBlockColor::White,		//8

};

static int stage1[6][13] =
{
	H,	 H,	  H,   _,	H,	 H,	  H,   H,	H,	 _,	  H,   H,	H,		//Hard
	N(4),N(4),N(4),_   ,N(4),N(4),N(4),N(4),N(4),_   ,N(4),N(4),N(4),	//R
	N(7),N(7),N(7),_   ,N(7),N(7),N(7),N(7),N(7),_   ,N(7),N(7),N(7),	//Y
	N(5),N(5),N(5),_   ,N(5),N(5),N(5),N(5),N(5),_   ,N(5),N(5),N(5),	//B
	N(6),N(6),N(6),_   ,N(6),N(6),N(6),N(6),N(6),_   ,N(6),N(6),N(6),	//M
	N(3),N(3),N(3),_   ,N(3),N(3),N(3),N(3),N(3),_   ,N(3),N(3),N(3)	//G
	//N(1)
};

//static int stage2[6][13] =
//{
//	//H,	 H,	  H,   H,	H,	 H,	  H,   H,	H,	 H,	  H,   H,	H,		//Hard
//	//N(4),N(4),N(4),N(4),N(4),N(4),N(4),N(4),N(4),N(4),N(4),N(4),N(4),	//R
//	//N(7),N(7),N(7),N(7),N(7),N(7),N(7),N(7),N(7),N(7),N(7),N(7),N(7),	//Y
//	//N(5),N(5),N(5),N(5),N(5),N(5),N(5),N(5),N(5),N(5),N(5),N(5),N(5),	//B
//	//N(6),N(6),N(6),N(6),N(6),N(6),N(6),N(6),N(6),N(6),N(6),N(6),N(6),	//M
//	//N(3),N(3),N(3),N(3),N(3),N(3),N(3),N(3),N(3),N(3),N(3),N(3),N(3)	//G
//	N(1)
//};
static int stage2[13][13] =
{
	N(8),_   ,_   ,_   ,_   ,_   ,_   ,_   ,_   ,_   ,_   ,_   ,_   ,
	N(8),N(1),_   ,_   ,_   ,_   ,_   ,_   ,_   ,_   ,_   ,_   ,_   ,
	N(8),N(1),N(2),_   ,_   ,_   ,_   ,_   ,_   ,_   ,_   ,_   ,_   ,
	N(8),N(1),N(2),N(3),_   ,_   ,_   ,_   ,_   ,_   ,_   ,_   ,_   ,
	N(8),N(1),N(2),N(3),_   ,_   ,_   ,_   ,_   ,_   ,_   ,_   ,_   ,
	N(8),N(1),N(2),N(3),_   ,N(5),_   ,_   ,_   ,_   ,_   ,_   ,_   ,
	N(8),N(1),N(2),N(3),_   ,N(5),N(6),_   ,_   ,_   ,_   ,_   ,_   ,
	N(8),N(1),N(2),N(3),_   ,N(5),N(6),N(7),_   ,_   ,_   ,_   ,_   ,
	N(8),N(1),N(2),N(3),_   ,N(5),N(6),N(7),N(8),_   ,_   ,_   ,_   ,
	N(8),N(1),N(2),N(3),_   ,N(5),N(6),N(7),N(8),N(1),_   ,_   ,_   ,
	N(8),N(1),N(2),N(3),_   ,N(5),N(6),N(7),N(8),N(1),N(2),_   ,_   ,
	N(8),N(1),N(2),N(3),_   ,N(5),N(6),N(7),N(8),N(1),N(2),N(3),_   ,
	H,	 H,	  H,   H,	_ ,	 H,	  H,   H,	H,	 H,	  H,   H,	N(4),
};
static int stage3[15][13] =
{
	N(3),N(3),N(3),N(3),N(3),N(3),N(3),N(3),N(3),N(3),N(3),N(3),N(3),
	_    ,_   ,_   ,_   ,_   ,_   ,_   ,_   ,_   ,_   ,_   ,_   ,_  ,
	N(8),N(8),N(8),I,	I,	 I,	  I,N(8),N(8), N(8),  I,   I,	I   ,
	_    ,_   ,_   ,_   ,_   ,_   ,_   ,_   ,_   ,_   ,_   ,_   ,_  ,
	N(4),N(4),N(4),N(4),N(4),N(4),N(4),N(4),N(4),N(4),N(4),N(4),N(4),
	_    ,_   ,_   ,_   ,_   ,_   ,_   ,_   ,_   ,_   ,_   ,_   ,_  ,
	I,   I,	  I,N(8),N(8), N(8),   I,   I,   I,	 I,   N(8),N(8),N(8),
	_    ,_   ,_   ,_   ,_   ,_   ,_   ,_   ,_   ,_   ,_   ,_   ,_  ,
	N(6),N(6),N(6),N(6),N(6),N(6),N(6),N(6),N(6),N(6),N(6),N(6),N(6),
	_    ,_   ,_   ,_   ,_   ,_   ,_   ,_   ,_   ,_   ,_   ,_   ,_  ,
	N(5),N(5),N(5),I,	I,	 I,	  I, N(5),N(5),N(5),  I,   I,	I,
	_    ,_   ,_   ,_   ,_   ,_   ,_   ,_   ,_   ,_   ,_   ,_   ,_  ,
	N(2),N(2),N(2),N(2),N(2),N(2),N(2),N(2),N(2),N(2),N(2),N(2),N(2),
	_    ,_   ,_   ,_   ,_   ,_   ,_   ,_   ,_   ,_   ,_   ,_   ,_  ,
	I,   I,	  I,  N(2),N(2),N(2), I,   I,   I,	 I,   N(2),N(2),N(2),

};


#undef N
#undef H
#undef I
#undef _
static UBlock* MakeBlock(int code, float cx, float cy, int round)
{
	if(code == 0)
		return nullptr;
	EBlockType type;
	EBlockColor color;
	if (code == 200)
	{
		type = EBlockType::Immortal;
		color = EBlockColor::Gold;
	}
	else if(code == 100)
	{
		type = EBlockType::Hard;
		color = EBlockColor::Silver;
	}
	else
	{
		type = EBlockType::Normal;
		color = ColorTable[code];
	}
	UBlock* block = new UBlock(type, color, round);
	//block->Init(); grid 사용할것
	return block;
}
template <int ROWS,int COLS>
static std::vector<UBlock*> BulidStage(int(&map)[ROWS][COLS], int round)
{
	std::vector<UBlock*> blocks;
	blocks.reserve(ROWS*COLS);

	for (int r = 0; r < ROWS; ++r)
	{
		for (int c = 0; c < COLS; ++c)
		{
			int code = map[r][c];
			if (code == 0)
			{
				blocks.push_back(nullptr);
				continue;
			}
			float cx = -1.0f + START_X + (c * STEP_X)+0.1f;
			float cy = 1.0f - (START_Y + r * STEP_Y)-0.5f;
			UBlock* b = MakeBlock(code, cx, cy, round);
			if (b) {
				b->Init(cx, cy, HALF_BW, HALF_BH);
				blocks.push_back(b);
			}
		}
	}
	return blocks;
}
template <int ROWS, int COLS>
static void GetStageMap(int(&map)[ROWS][COLS], int& outRow, int& outCol)
{
	outRow = ROWS;
	outCol = COLS;
}
void GetStageInfo(int StageNum, int& outRow, int& outCol)
{
	switch (StageNum)
	{
	case 1: GetStageMap(stage1, outRow, outCol); break;
	case 2: GetStageMap(stage2, outRow, outCol); break;
	case 3: GetStageMap(stage3, outRow, outCol); break;
	default:GetStageMap(stage1, outRow, outCol); break;
	}
}

std::vector<UBlock*> CreateStage(int Round)
{
	std::vector<UBlock*> result;
	switch (Round)
	{
		
	case 1:
		result = BulidStage(stage1, Round);
		break;
	case 2:
		result = BulidStage(stage2, Round);
		break;
	case 3:
		result = BulidStage(stage3, Round);
		break;
	default:
		result = BulidStage(stage1, Round);
		break;
	}
    return result;
}


int GetStageRow(int Round)
{
	return 0;
}

StageData GetStageData()
{
	StageData Data;

	Data.HALF_BW = HALF_BW;
	Data.HALF_BH = HALF_BH;
	Data.STEP_X = STEP_X;
	Data.STEP_Y = STEP_Y;
	Data.START_X = START_X;
	Data.START_Y = START_Y;

	return Data;
}