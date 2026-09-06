#pragma once
#include "UScene.h"
#include "Stage.h"

class UGameManager;
class UBall;
class UBar;
class UBlock;
class UParticlePool;
class UBullet;

class UGameScene:
    public UScene
{
public:
    UGameScene();
    ~UGameScene();

    void UIRender();
    void RenderLifeUI(URenderer& render);

    // UScene을(를) 통해 상속됨
    void Update(float delta) override;
    void Render(URenderer render) override;
    bool HaveBalls();

    // UScene을(를) 통해 상속됨
    void Init() override;
    void Release() override;

    void NextStage(int StageNo);

    void AddObject(UGameObject* Object) override;
    bool bIsBrickEmpty();

    std::vector<UBall*>& GetActiveBalls();
    void AddBall(UBall* ball);
	UParticlePool* GetParticlePool() const { return particlePool; }
    void StopAllBall();

    // 아이템 효과 초기화
    void InitItemEffect();

private:
    UGameManager* gameManager = nullptr;
	UParticlePool* particlePool = nullptr;
    std::vector<UBlock*> stageblocks;
    std::vector<UBall*> ActiveBallList;
    UBar* Bar_1;
    UBar* Bar_2;
    int CurrentStage=1;
    int CurrentStageRow;
    int CurrentStageCol;
    bool ShowStageClearModal{ false };
    bool ShowGameOverModal{ false };
    StageData StageData;
};