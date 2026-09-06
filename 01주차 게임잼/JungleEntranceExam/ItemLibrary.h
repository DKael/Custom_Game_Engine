#pragma once

#include "UItem.h"

class ItemLibrary
{
public:
    static FItemDesc MakeRandomItem();

    static FItemDesc MakeAddLife();
    static FItemDesc MakeMultiBall(int Count = 2);
    static FItemDesc MakeScoreBonus(int Score = 100);
    static FItemDesc MakePaddleExpand(float DeltaSize);
    static FItemDesc MakePaddleShrink(float DeltaSize);
    static FItemDesc MakePaddleSpeedUp(float Multiplier);
    static FItemDesc MakePaddleSpeedDown(float Multiplier);
    static FItemDesc MakeBallSpeedUp(float Multiplier);
    static FItemDesc MakeBallSpeedDown(float Multiplier);
    static FItemDesc AddBullet(int BulletCount = 10);
};

