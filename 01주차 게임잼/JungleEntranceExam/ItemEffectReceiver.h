#pragma once

class IItemEffectReceiver
{
public:
    virtual ~IItemEffectReceiver() {}

public:
    virtual void AddLife() = 0;
    virtual void SpawnExtraBalls(int Count) = 0;
    virtual void AddScore(int Amount) = 0;

    virtual void ModifyPaddleSize(float DeltaSize) = 0;
    virtual void ModifyPaddleSpeed(float Multiplier) = 0;
    virtual void ModifyBallSpeed(float Multiplier) = 0;
    virtual void AddBullet(int BulletCount) = 0;
};