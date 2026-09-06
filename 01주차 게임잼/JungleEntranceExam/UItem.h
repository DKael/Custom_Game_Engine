#pragma once

#include "UDiagram.h"
#include "ItemEffectReceiver.h"

enum class EItemType
{
    None = 0,

    AddLife,        // 목숨 추가
    MultiBall,      // 공 갯수 증가
    ScoreBonus,     // 점수 추가

    PaddleExpand,   // 패들 길이 증가
    PaddleShrink,   // 패들 길이 감소
    PaddleSpeedUp,  // 패들 속도 증가
    PaddleSpeedDown,// 패들 속도 감소
    BallSpeedUp,    // 공 속도 증가
    BallSpeedDown,  // 공 속도 감소

    AddBullet
};

enum class EItemApplyType
{
    Immediate,
    Duration,
};

//struct FRect
//{
//    float Left = 0.0f;
//    float Top = 0.0f;
//    float Right = 0.0f;
//    float Bottom = 0.0f;
//};

struct FItemDesc
{
    EItemType Type = EItemType::None;
    EItemApplyType ApplyType = EItemApplyType::Immediate;

    float FallSpeed = 0.5f;

    int IntValue = 0;
    float FloatValue = 0.0f;
    float Duration = 0.0f;

    const char* DebugName = "";
};

struct FItemSpawnInfo
{
    EItemType Type = EItemType::None;
    FVector SpawnPosition;
    FVector InitialVelocity;
};

class UItem : public UDiagram
{
public:
    FVector Location;           // 아이템의 위치
    FVector Direction;          // 아이템의 이동 방향
    float Speed;                // 아이템의 이동속도
    float Scale;                // 아이템의 크기
    //float XLength;              // 아이템의 가로 절반길이
    //float YLength;              // 아이템의 세로 절반길이

    FItemDesc ItemDesc;         // 아이템 속성 모음
    bool bAlive = true;         // 유효 검사
    bool bCollected = false;    

    // 생성자 및 소멸자
public:
    UItem(const FVector& _Location, const FVector& _Direction, const float _Speed, const float _Scale, const FItemDesc& _ItemDesc);
    virtual ~UItem() override;

    // UPrimitive 인터페이스 구현
    // 물리/이동 업데이트
    virtual void Update(float deltaTime);

    // 렌더링 (상수 버퍼 업데이트)
    virtual void Render(URenderer& renderer);

    // 벽 충돌 적용
    virtual void ApplyWallCollision();

    // 충격량 적용
    virtual void ApplyGravity(float deltaTime, const FVector& gravity);

    virtual bool CheckCollision(const UDiagram* Other) override;

    // void ResolveCollision(UBar* Other);

    void SetScale(const float _Scale);

    bool IsAlive() const;
    void Destroy();

public:
    void ApplyTo(IItemEffectReceiver* Receiver);
    
    //EItemType GetType() const;
    //const FItemDesc& GetDesc() const;

    //FVector GetLocation() const;
    //void SetLocation(const FVector& InLocation);

    //FVector GetDirection() const;
    //void SetDirection(const FVector& InDirection);

    //void SetBoundsSize(float InWidth, float InHeight);
    //FRect GetBounds() const;

    //bool CheckCollision(const FRect& OtherBounds) const;

private:
    //void Move(float DeltaTime);
    void ApplyEffect(IItemEffectReceiver* Receiver);
};

