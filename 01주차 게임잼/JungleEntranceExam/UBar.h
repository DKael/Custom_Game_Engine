#pragma once

#include "UDiagram.h"
#include "ItemEffectReceiver.h"
#include "UBullet.h"

enum class EDirection
{
    Left = -1,
    Right = 1
};

enum class EPlaySide
{
    Up = 1,
    Down = -1
};

class UBar : public UDiagram, public IItemEffectReceiver
{
public:
    FVector Location;           
    float Speed;               
    float Scale;
    float XLength;               
    float YLength;              
    int PlayerNo;
    int Direction;
    EPlaySide Side;

    int LoadedBulletCount;
    float ShootInterval;
    std::chrono::steady_clock::time_point LastFireTime;
    std::vector<UBullet> FlyingBullet;
    int FlyingBulletVecSize;
    EDirection CurrentShootSide;
    int ShootKey;

    const float MinSpeed = 0.3f;
    const float MaxSpeed = 5.0f;

    const float MinScale = 0.05f;
    const float MaxScale = 0.3f;

public:
    UBar(const FVector& _Location, const float _Speed, const float _Scale, int _PlayerNo, EPlaySide _Side);

    virtual ~UBar() override;

    virtual void Update(float deltaTime);

    virtual void Render(URenderer& renderer);

    virtual void ApplyWallCollision();

    virtual void ApplyGravity(float deltaTime, const FVector& gravity);

    virtual bool CheckCollision(const UDiagram* Other) override;

    // void ResolveCollision(UBar* Other);

    void SetScale(const float _Scale);
    void SetSpeed(const float _Speed);

    FVector GetLocation() const;
    void SetLocation(const FVector& NewLoc);
    void SetLocation(FVector&& NewLoc);

    virtual void AddLife() override;
    virtual void SpawnExtraBalls(int Count) override;
    virtual void AddScore(int Amount) override;
    virtual void ModifyPaddleSize(float DeltaSize) override;
    virtual void ModifyPaddleSpeed(float Multiplier) override;
    virtual void ModifyBallSpeed(float Multiplier) override;
    virtual void AddBullet(int BulletCount);

    void InitBarItemEffect();

    std::vector<UBullet>& GetFlyingBulletVec();

    void Shoot();
};