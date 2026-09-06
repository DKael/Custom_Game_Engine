#include "ItemLibrary.h"

FItemDesc ItemLibrary::MakeRandomItem()
{
    int r = rand() % 110;

    // 목숨 추가 아이템 (확률 : --%)
    if (r < 5)
        return MakeAddLife();
    // 멀티볼 아이템 (확률 : --%)
    if (r < 20)
        return MakeMultiBall(2);
    // 스코어 보너스 아이템 (확률 : --%)
    if (r < 30)
        return MakeScoreBonus(100);
    // 패들 길이 증가 아이템 (확률 : --%)
    if (r < 45)
        return MakePaddleExpand(1.2f);
    // 패들 길이 감소 아이템 (확률 : --%)
    if (r < 55)
        return MakePaddleShrink(0.8f);
    // 패들 속도 증가 아이템 (확률 : --%)
    if (r < 70)
        return MakePaddleSpeedUp(1.2f);
    // 패들 속도 감소 아이템 (확률 : --%)
    if (r < 80)
        return MakePaddleSpeedDown(0.8f);
    // 공 속도 증가 아이템 (확률 : --%)
    if (r < 90)
        return MakeBallSpeedUp(1.5f);
    // 공 속도 감소 아이템 (확률 : --%)
    if (r < 100)
        return MakeBallSpeedDown(0.7f);
    else
        return AddBullet(5);
}

FItemDesc ItemLibrary::MakeAddLife()
{
    FItemDesc Desc;
    Desc.Type = EItemType::AddLife;
    Desc.ApplyType = EItemApplyType::Immediate;
    Desc.DebugName = "AddLife";
    return Desc;
}

FItemDesc ItemLibrary::MakeMultiBall(int Count)
{
    FItemDesc Desc;
    Desc.Type = EItemType::MultiBall;
    Desc.ApplyType = EItemApplyType::Immediate;
    Desc.IntValue = Count;
    Desc.DebugName = "MultiBall";
    return Desc;
}

FItemDesc ItemLibrary::MakeScoreBonus(int Score)
{
    FItemDesc Desc;
    Desc.Type = EItemType::ScoreBonus;
    Desc.ApplyType = EItemApplyType::Immediate;
    Desc.IntValue = Score;
    Desc.DebugName = "ScoreBonus";
    return Desc;
}

FItemDesc ItemLibrary::MakePaddleExpand(float DeltaSize)
{
    FItemDesc Desc;
    Desc.Type = EItemType::PaddleExpand;
    Desc.ApplyType = EItemApplyType::Immediate;
    Desc.FloatValue = DeltaSize;
    Desc.DebugName = "PaddleExpand";
    return Desc;
}

FItemDesc ItemLibrary::MakePaddleShrink(float DeltaSize)
{
    FItemDesc Desc;
    Desc.Type = EItemType::PaddleShrink;
    Desc.ApplyType = EItemApplyType::Immediate;
    Desc.FloatValue = DeltaSize;
    Desc.DebugName = "PaddleShrink";
    return Desc;
}

FItemDesc ItemLibrary::MakePaddleSpeedUp(float Multiplier)
{
    FItemDesc Desc;
    Desc.Type = EItemType::PaddleSpeedUp;
    Desc.ApplyType = EItemApplyType::Immediate;
    Desc.FloatValue = Multiplier;
    Desc.DebugName = "PaddleSpeedUp";
    return Desc;
}

FItemDesc ItemLibrary::MakePaddleSpeedDown(float Multiplier)
{
    FItemDesc Desc;
    Desc.Type = EItemType::PaddleSpeedDown;
    Desc.ApplyType = EItemApplyType::Immediate;
    Desc.FloatValue = Multiplier;
    Desc.DebugName = "PaddleSpeedDown";
    return Desc;
}

FItemDesc ItemLibrary::MakeBallSpeedUp(float Multiplier)
{
    FItemDesc Desc;
    Desc.Type = EItemType::BallSpeedUp;
    Desc.ApplyType = EItemApplyType::Immediate;
    Desc.FloatValue = Multiplier;
    Desc.DebugName = "BallSpeedUp";
    return Desc;
}

FItemDesc ItemLibrary::MakeBallSpeedDown(float Multiplier)
{
    FItemDesc Desc;
    Desc.Type = EItemType::BallSpeedDown;
    Desc.ApplyType = EItemApplyType::Immediate;
    Desc.FloatValue = Multiplier;
    Desc.DebugName = "BallSpeedDown";
    return Desc;
}

FItemDesc ItemLibrary::AddBullet(int BulletCount)
{
    FItemDesc Desc;
    Desc.Type = EItemType::AddBullet;
    Desc.ApplyType = EItemApplyType::Immediate;
    Desc.IntValue = BulletCount;
    Desc.DebugName = "MultiBall";
    return Desc;
}
