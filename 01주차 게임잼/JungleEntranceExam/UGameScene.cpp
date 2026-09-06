#include "UGameScene.h"
#include "UGameObject.h"
#include "UGameManager.h" 
#include "UBall.h"
#include "UBar.h"
#include "Util.h"
#include "UClearScene.h"
#include "USceneManager.h"
#include "USoundManager.h"
#include "UItemManager.h"
#include "Stage.h"
#include "UParticlePool.h"
#include "FileInfo.h"
UGameScene::UGameScene()
{
    Init();
}

UGameScene::~UGameScene()
{
    Release();
}

void UGameScene::UIRender()
{
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    const ImGuiViewport* viewport = ImGui::GetMainViewport();

    float padding = 20.0f;

    ImVec2 windowPos(viewport->WorkPos.x + viewport->WorkSize.x - padding, viewport->WorkPos.y + padding);

    ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always, ImVec2(1.0f, 0.0f));

    ImGuiWindowFlags hudFlags =
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoInputs |
        ImGuiWindowFlags_AlwaysAutoResize;

    ImGui::Begin("HUD", nullptr, hudFlags);

    ImGui::SetWindowFontScale(3.0f);
    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "HIGH:");

    ImGui::SetWindowFontScale(3.0f);
    ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "%d", GetHightScore());

    ImGui::SetWindowFontScale(3.0f);
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "SCORE:");

    ImGui::SetWindowFontScale(3.0f);
    ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "%d", gameManager->GetTotalScore());
    //ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "LIVES: %d", gameManager->GetCurLife());

    if (ShowStageClearModal)
    {
        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(300, 150)); // 창 크기 조절

        // 팝업 열기 (이름은 고유해야 함)
        ImGui::OpenPopup("Stage Clear Check");

        if (ImGui::BeginPopupModal("Stage Clear Check", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
        {
            ImGui::Text("\n             Stage %d Clear!         \n\n", CurrentStage);
            ImGui::Separator();

            // 버튼 배치 (중앙 정렬을 위해 약간의 여백 추가 가능)
            ImGui::SetCursorPosX(100);
            if (ImGui::Button("Next Stage", ImVec2(100, 40)))
            {
                this->NextStage(CurrentStage + 1);
                Bar_1->InitBarItemEffect();
                Bar_2->InitBarItemEffect();
                ShowStageClearModal = false;
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }
    else if (ShowGameOverModal)
    {
        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(300, 150)); // 창 크기 조절

        // 팝업 열기 (이름은 고유해야 함)
        ImGui::OpenPopup("Game Over Check");

        if (ImGui::BeginPopupModal("Game Over Check", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
        {
            ImGui::Text("\n               Game Over!         \n\n");
            ImGui::Separator();

            // 버튼 배치 (중앙 정렬을 위해 약간의 여백 추가 가능)
            ImGui::SetCursorPosX(100);
            if (ImGui::Button("TITLE MENU", ImVec2(100, 40)))
            {
                ShowGameOverModal = false;
                gameManager->Exit();
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }
    ImGui::End();

    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

void UGameScene::RenderLifeUI(URenderer& renderer)
{
    const int maxLives = 3;
    const int curLives = gameManager->GetCurLife();
    const float uiRadius = 0.03f;   // UI용 작은 공 크기
    const float spacing = 0.08f;    // 공 사이의 간격
    FVector startPos(-0.9f, 0.9f, 0.0f); // 화면 왼쪽 상단 좌표

    for (int i = 0; i < maxLives; ++i) {

        FVector pos = startPos + FVector(i * spacing, 0, 0);

        if (i < curLives)
        {
            renderer.UpdateConstant(pos, FVector(uiRadius, uiRadius, 0), FColor(1.0, 1.0, 1.0, 1.0f));
        }
        else
        {
            renderer.UpdateConstant(pos, FVector(uiRadius, uiRadius, 0), FColor(1.0, 1.0, 1.0, 0.1f));
        }

        renderer.RenderNewSphere();
    }
}

//해당 게임에서 생성되는 모든 오브젝트여기서 생성
void UGameScene::Init()
{
    UGameObjectList.clear();
    SceneType = ESceneType::InGame;
    ShowStageClearModal = false;
    ShowGameOverModal = false;
    ActiveBallList.clear();

    //1번 플레이어가 움직이는 바
    Bar_1 = new UBar(FVector(0.0f, -0.95f, 0.0f), 1.0f, 0.15f, 0, EPlaySide::Up);

    //2번 플레이어가 움직이는 바
    Bar_2 = new UBar(FVector(0.0f, 0.95f, 0.0f), 1.0f, 0.15f, 1, EPlaySide::Down);

    AddObject(UBall::CreateBallAtBar(*Bar_1));
    AddObject(Bar_1);
    AddObject(Bar_2);

    CurrentStage = 1;
    stageblocks = CreateStage(CurrentStage);
    for (auto& b : stageblocks)
    {
        if (!b)
            nullptr;
        AddObject(b);
    }

    GetStageInfo(CurrentStage, CurrentStageRow, CurrentStageCol);
    StageData = GetStageData();

    //게임매니저 초기화
    particlePool = new UParticlePool(200);
    gameManager = UGameManager::GetInstance();
    gameManager->RessetGM();
}

void UGameScene::NextStage(int StageNo)
{
    Bar_1->SetLocation(FVector(0.0f, -0.95f, 0.0f));
    Bar_2->SetLocation(FVector(0.0f, 0.95f, 0.0f));

    for (UBall* ball : ActiveBallList)
    {
        if (ball) 
            delete ball;
    }
    ActiveBallList.clear();
    for (auto* block : stageblocks)
    {
        if (block) 
            delete block;
    }
    stageblocks.clear();
    UGameObjectList.clear();
    AddObject(Bar_1);
    AddObject(Bar_2);
    AddObject(UBall::CreateBallAtBar(*Bar_1));

    CurrentStage = StageNo;
    stageblocks = CreateStage(CurrentStage);
    GetStageInfo(CurrentStage, CurrentStageRow, CurrentStageCol);
    for (auto& b : stageblocks)
    {
        if (b) AddObject(b);
    }
    UItemManager::Get().Clear();
    StageData = GetStageData();
}
void UGameScene::Release()
{
    //Map에서 할당한 brick들을 해제해야함

    //생성된 모든 Ball을 제거
    for (UBall* ball : ActiveBallList)
    {
        if (ball != nullptr) {
            delete ball;
        }
    }
    ActiveBallList.clear();

    //생성된 모든 UGameObject을 제거
    for (UGameObject* Object : UGameObjectList)
    {
        if (Object != nullptr) {
            delete Object;
        }
    }

    UGameObjectList.clear();
    if (particlePool != nullptr)
    {
        delete particlePool;
        particlePool = nullptr;
    }
    stageblocks.clear();
}

/// <summary>
/// 해당 맵에 있는 모든 객체에 업데이트를 호출함
/// </summary>
/// <param name="delta"></param>
void UGameScene::Update(float delta)
{
    if (ShowGameOverModal || ShowStageClearModal) return;

    FVector CollisionPos;
    FVector Dummy;
    for (UGameObject* Object : UGameObjectList)
    {
        UDiagram* Diagram = dynamic_cast<UDiagram*>(Object);
        if (Diagram != nullptr) {
            Diagram->Update(delta);
        }
    }

    for (UBall* ball : ActiveBallList)
    {
        if (ball != nullptr) {
            ball->Update(delta);
        }

        EBlockCollision CollisionState1 = ball->CheckBarCollision(*Bar_1, CollisionPos);
        ball->BallBounceAtBar(CollisionState1, *Bar_1, CollisionPos);

        EBlockCollision CollisionState2 = ball->CheckBarCollision(*Bar_2, CollisionPos);
        ball->BallBounceAtBar(CollisionState2, *Bar_2, CollisionPos);


        int idx = -1;
        int CurRow = 0;
        int CurCol = 0;
        for (auto* b : stageblocks)
        {
            idx++;
            if (!b || !b->IsActive()) continue;
            if (b->CheckSkip())
            {
                b->SetSkipCalc(false);
                continue;
            }


            EBlockCollision CollisionState = (*ball).CheckBlockCollision(*b, CollisionPos);
            if (CollisionState != EBlockCollision::None)
            {
                if (CollisionState == EBlockCollision::Corner)
                {
                    CurRow = idx / CurrentStageCol;
                    CurCol = idx % CurrentStageCol;

                    // 현재 충돌한 모서리 방향에 따른 인접 블록 조사 방향 설정
                    int dCol = (CollisionPos.x < b->CenterX) ? -1 : 1; // 왼쪽 모서리면 -1, 오른쪽이면 1
                    int dRow = (CollisionPos.y < b->CenterY) ? 1 : -1; // 아래쪽 모서리면 1, 위쪽이면 -1 (좌표계 확인 필요)

                    bool HasHorizontalBlock = false;
                    bool HasVerticalBlock = false;

                    // 1. 옆(수평) 방향에 블록이 있는지 확인
                    int CheckCol = CurCol + dCol;
                    if (CheckCol >= 0 && CheckCol < CurrentStageCol) {
                        if (stageblocks[CurRow * CurrentStageCol + CheckCol]
                            && stageblocks[CurRow * CurrentStageCol + CheckCol]->IsActive())
                            HasHorizontalBlock = true;
                    }

                    // 2. 위/아래(수직) 방향에 블록이 있는지 확인
                    int CheckRow = CurRow + dRow;
                    if (CheckRow >= 0 && CheckRow < CurrentStageRow) {
                        if (stageblocks[CheckRow * CurrentStageCol + CurCol]
                            && stageblocks[CheckRow * CurrentStageCol + CurCol]->IsActive())
                            HasVerticalBlock = true;
                    }

                    // 3. 판정 교정 로직
                    if (HasHorizontalBlock && !HasVerticalBlock) {
                        // 옆은 막혔고 위/아래가 비었음 -> 윗면/아랫면 충돌로 간주
                        CollisionState = EBlockCollision::Horizontal;
                        stageblocks[CurRow * CurrentStageCol + CheckCol]->SetSkipCalc(true);
                    }
                    else if (!HasHorizontalBlock && HasVerticalBlock) {
                        // 위/아래는 막혔고 옆이 비었음 -> 옆면 충돌로 간주
                        CollisionState = EBlockCollision::Vertical;
                        stageblocks[CheckRow * CurrentStageCol + CurCol]->SetSkipCalc(true);
                    }
                    // 둘 다 비어있으면(else) 원래의 Corner 판정 유지 (반사 벡터 사용)
                }

                (*ball).BallBounceAtBlock(CollisionState, *b, CollisionPos);
                //gameManager->SetScore(b->GetScore());
                USoundManager::GetInstance().Play("Brick");
            }
            
        }
    }

    std::vector<UBullet>& FlyingBulletVecRef1{ Bar_1->GetFlyingBulletVec() };
    for (UBullet& b : FlyingBulletVecRef1)
    {
        if (b.GetIsHit())
        {
            b.SetIsFlying(false);
            b.SetIsHit(false);
            continue;
        }
        if (b.GetIsFlying())
        {
            b.Update(delta);

            FVector& CurLocation{ b.GetLocation() };

            int CurCol{ static_cast<int>((CurLocation.x - (-1.0f + StageData.START_X + 0.035f)) / StageData.STEP_X) };

            if (CurCol < 0 || CurCol >= CurrentStageCol)
            {
                continue;
            }
            for (int CurRow{ CurrentStageRow - 1 }; CurRow >= 0;CurRow--)
            {
                UBlock* BlockPtr{ stageblocks[CurRow * CurrentStageCol + CurCol] };
                if (!BlockPtr)
                {
                    continue;
                }
                if (BlockPtr->IsActive())
                {
                    if (b.CheckBlockHit(*BlockPtr, delta))
                    {
                        b.SetIsHit(true);
                        FVector BulletDirection(0.0f, 1.0f, 0.0f);
                        BlockPtr->TakeDamage(BulletDirection);
                        //gameManager->SetScore(BlockPtr->GetScore());
                        CurLocation.y = BlockPtr->MinY;
                    }
                    else
                    {
                        break;
                    }
                }
            }
        }
    }

    std::vector<UBullet>& FlyingBulletVecRef2{ Bar_2->GetFlyingBulletVec() };
    for (UBullet& b : FlyingBulletVecRef2)
    {
        if (b.GetIsHit())
        {
            b.SetIsFlying(false);
            b.SetIsHit(false);
            continue;
        }
        if (b.GetIsFlying())
        {
            b.Update(delta);

            FVector& CurLocation{ b.GetLocation() };

            int CurCol{ static_cast<int>((CurLocation.x - (-1.0f + StageData.START_X + 0.035f)) / StageData.STEP_X) };

            if (CurCol < 0 || CurCol >= CurrentStageCol)
            {
                continue;
            }
            for (int CurRow{ 0 }; CurRow < CurrentStageRow;CurRow++)
            {
                UBlock* BlockPtr{ stageblocks[CurRow * CurrentStageCol + CurCol] };
                if (!BlockPtr)
                {
                    continue;
                }
                if (BlockPtr->IsActive())
                {
                    if (b.CheckBlockHit(*BlockPtr, delta))
                    {
                        b.SetIsHit(true);
                        FVector BulletDirection(0.0f, -1.0f, 0.0f);
                        BlockPtr->TakeDamage(BulletDirection);
                        //gameManager->SetScore(BlockPtr->GetScore());
                        CurLocation.y = BlockPtr->MaxY;
                    }
                    else
                    {
                        break;
                    }
                }
            }
        }
    }
    

    // Item Objects Update
    UItemManager::Get().Update(delta);
    UItemManager::Get().CheckCollision(Bar_1);
    UItemManager::Get().CheckCollision(Bar_2);
    gameManager->Update(delta);

    // 밖에 공이 나갔는지 판별
    if (!HaveBalls())
    {
        // 공이 다 나갔으니 새 공을 하나 스폰해줍니다.
        UBall* newBall = UBall::CreateBallAtBar(*Bar_1);
        ActiveBallList.push_back(newBall);

        // 아이템 관련 리소스 해제
        UItemManager::Get().Clear();

        USoundManager::GetInstance().Play("Damage");
        if (gameManager->GetCurLife() > 1)
        {
            gameManager->SubHealth(1);

            Bar_1->InitBarItemEffect();
            Bar_2->InitBarItemEffect();
        }
        else
        {
            gameManager->SubHealth(1);
            StopAllBall();
            HightScoreUpdate(gameManager->GetTotalScore());
            ShowGameOverModal = true;
        }
    }
    particlePool->Update(delta);
    if (bIsBrickEmpty()) // 벽돌 다 깨짐!
    {

        UItemManager::Get().Clear();

        if (CurrentStage != 3)
        {
            StopAllBall();
            ShowStageClearModal = true;
        }
        else
        {
            USoundManager::GetInstance().StopAll();
            USoundManager::GetInstance().Play("Victory");
            UClearScene::FinalScore = gameManager->GetTotalScore();
            HightScoreUpdate(UClearScene::FinalScore);
            USceneManager::GetInstance().LoadScene(ESceneType::Clear);
            return;
        }
    }
}

/// <summary>
/// 모든 공을 확인하고 남은 게 있는지 여부를 반환
/// </summary>
/// <returns></returns>
bool UGameScene::HaveBalls()
{
    bool hasBallLeft = false;

    for (auto it = ActiveBallList.begin(); it != ActiveBallList.end(); )
    {
        UBall* ball = *it;

        //예외 처리
        if (ball == nullptr) {
            it = ActiveBallList.erase(it);
            continue;
        }

        FVector Location = ball->Location;
        float Radius = ball->Radius;

        //만약에 공이 밖으로 나가지 않았으면 다음거 확인
        if (Location.y < 1 - Radius && Location.y > -1 + Radius) {
            //공이 아직 남아있음
            hasBallLeft = true;
            ++it;
        }
        else {
            delete ball;
            it = ActiveBallList.erase(it);
        }
    }

    return hasBallLeft;
}

/// <summary>
/// 공만 따로 구분짓는 코드 
/// </summary>
/// <param name="Object"></param>
void UGameScene::AddObject(UGameObject* Object)
{
    UBall* Ball = dynamic_cast<UBall*>(Object);

    if (Ball != nullptr) {
        ActiveBallList.push_back(Ball);
    }
    else {
        UGameObjectList.push_back(Object);
    }
}

bool UGameScene::bIsBrickEmpty()
{
    for (auto& b : stageblocks) {
        if (b && b->IsActive()) return false;
    }
    return true;
}

std::vector<UBall*>& UGameScene::GetActiveBalls()
{
    return ActiveBallList;
}

void UGameScene::AddBall(UBall* ball)
{
    ActiveBallList.push_back(ball);
}

void UGameScene::StopAllBall()
{
    for (UBall* ball : ActiveBallList)
    {
        if (ball != nullptr) {
            ball->StopMove();
        }
    }
}

void UGameScene::InitItemEffect()
{
    // 각 바의 아이템 효과 초기화
    Bar_1->InitBarItemEffect();
    Bar_2->InitBarItemEffect();
}

void UGameScene::Render(URenderer render)
{
    for (UGameObject* Object : UGameObjectList)
    {
        UDiagram* Diagram = dynamic_cast<UDiagram*>(Object);

        if (Diagram != nullptr) {
            Diagram->Render(render);
            render.RenderRectangle();
        }
    }

    for (UBall* ball : ActiveBallList)
    {
        if (ball != nullptr) {
            ball->Render(render);
            render.RenderSphere();
        }
    }


    // Item Objects Render
    UItemManager::Get().Render(render);

    for (auto* b : stageblocks)
    {

        if (!b) //<-added

            continue;
        b->Render(render);
    }

    std::vector<UBullet>& FlyingBulletVecRef1{ Bar_1->GetFlyingBulletVec() };
    for (UBullet& b : FlyingBulletVecRef1)
    {
        if (b.GetIsFlying())
        {
            b.Render(render);
            render.RenderBullet();
        }
    }
    if (Bar_1->LoadedBulletCount > 0)
    {
        UBullet Dummy(static_cast<int>(Bar_1->Side));
        Dummy.SetLocation(Bar_1->Location + FVector(Bar_1->XLength * static_cast<int>(Bar_1->CurrentShootSide), 0.0f, 0.0f));
        Dummy.Render(render);
        render.RenderBullet();
    }

    std::vector<UBullet>& FlyingBulletVecRef2{ Bar_2->GetFlyingBulletVec() };
    for (UBullet& b : FlyingBulletVecRef2)
    {
        if (b.GetIsFlying())
        {
            b.Render(render);
            render.RenderBullet();
        }
    }
    if (Bar_2->LoadedBulletCount > 0)
    {
        UBullet Dummy(static_cast<int>(Bar_2->Side));
        Dummy.SetLocation(Bar_2->Location + FVector(Bar_2->XLength * static_cast<int>(Bar_2->CurrentShootSide), 0.0f, 0.0f));
        Dummy.Render(render);
        render.RenderBullet();
    }
    

    if (particlePool) {
        particlePool->Render(render);
    }

    RenderLifeUI(render);
}
