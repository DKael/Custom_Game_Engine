#include "UBar.h"
#include "UBall.h"
#include "UInputManager.h"
#include "UGameScene.h"
#include "USceneManager.h"
#include "UGameManager.h"

// 생성자 및 소멸자
UBar::UBar(const FVector& _Location, const float _Speed, const float _Scale, int _PlayerNo, EPlaySide _Side)
	: UDiagram()
	, Location(_Location)
	, Speed(_Speed)
	, Scale(_Scale)
	, PlayerNo(_PlayerNo)
	, XLength(1.0f * _Scale)
	, YLength(0.025f)
	, Direction(0)
	, Side(_Side)
	, LoadedBulletCount(5)
	, ShootInterval(0.5f)
	, LastFireTime(std::chrono::steady_clock::now())
	, FlyingBulletVecSize(30)
	, CurrentShootSide(EDirection::Left)
	, ShootKey((PlayerNo == 0) ? VK_DOWN : 'S')
{
	FlyingBullet.reserve(FlyingBulletVecSize);
	for (int i = 0; i < FlyingBulletVecSize; i++)
	{
		FlyingBullet.push_back(UBullet(static_cast<int>(Side)));
	}
}

UBar::~UBar()
{
}

// 물리/이동 업데이트
void UBar::Update(float deltaTime)
{
	UInputManager* input = UInputManager::GetInstance();
	
	float moveX = input->GetAxisX(PlayerNo);
	if (moveX > 0.0f)      Direction = 1;
	else if (moveX < 0.0f) Direction = -1;
	else                   Direction = 0;
	Location.x += (Speed * deltaTime * Direction);

	if (PlayerNo == 0)
	{
		if (input->GetKeyDown(ShootKey))
		{
			Shoot();
		}
	}
	else
	{
		bool bKeyboardShoot = false;
		bool bGamepadShoot = false;
	
		if(input->getGamepadManager().IsConnected())
			bGamepadShoot = input->getGamepadManager().IsPressed(ABI::Windows::Gaming::Input::GamepadButtons_A);
		else
			bKeyboardShoot = input->GetKeyDown(ShootKey);


		if (bKeyboardShoot || bGamepadShoot)
		{
			Shoot();
		}

	}

	// 벽 충돌 적용
	ApplyWallCollision();
}

// 렌더링 (상수 버퍼 업데이트)
void UBar::Render(URenderer& renderer)
{
	renderer.UpdateConstant(Location, FVector(Scale, YLength, 0));
}

// 벽 충돌 적용
void UBar::ApplyWallCollision()
{
	Location.x = std::clamp(Location.x, leftBorder + XLength, rightBorder - XLength);
}

// 충격량 적용
void UBar::ApplyGravity(float deltaTime, const FVector& gravity)
{

}

bool UBar::CheckCollision(const UDiagram* Other)
{
	return false;
}

//void UBar::ResolveCollision(UBar* Other)
//{
//	
//}

void UBar::SetScale(const float _Scale)
{
	Scale = _Scale;

	if (Scale > MaxScale) Scale = MaxScale;
	else if (Scale < MinScale) Scale = MinScale;

	XLength = 1.000000f * Scale;
}

FVector UBar::GetLocation() const
{
	return Location;
}

void UBar::SetLocation(const FVector& NewLoc) {
	Location = NewLoc;
}

void UBar::SetLocation(FVector&& NewLoc) {
	Location = std::move(NewLoc);
}

void UBar::SetSpeed(const float _Speed)
{
	Speed = _Speed;

	if (Speed > MaxSpeed) Speed = MaxSpeed;
	else if (Speed < MinSpeed) Speed = MinSpeed;
}

void UBar::AddLife()
{
	UGameManager::GetInstance()->AddHealth(1);
}

void UBar::SpawnExtraBalls(int Count)
{
	OutputDebugStringA("SpawnExtraBalls called\n");

	if (USceneManager::GetInstance().GetCurrentSceneName() == ESceneType::InGame)
	{
		UGameScene* gameScene = dynamic_cast<UGameScene*>(USceneManager::GetInstance().GetCurrentScene());

		if (gameScene == nullptr)
			return;

		std::vector<UBall*>& balls = gameScene->GetActiveBalls();
	
		if (balls.empty())
			return;

		// 랜덤 공 선택
		int index = rand() % balls.size();
		UBall* selectedBall = balls[index];

		// 멀티볼 생성
		UBall** newBalls = UBall::CreateMultiBalls(selectedBall);

		if (newBalls)
		{
			// 씬에 추가
			gameScene->AddBall(newBalls[0]);
			gameScene->AddBall(newBalls[1]);

			delete[] newBalls;
		}
	}
}

void UBar::AddScore(int Amount)
{
	OutputDebugStringA("AddScore called\n");

	UGameManager::GetInstance()->AddScore(Amount);
}

void UBar::ModifyPaddleSize(float DeltaSize)
{
	OutputDebugStringA("ModifyPaddleSize called\n");

	float modifiedScale = Scale * DeltaSize;
	SetScale(modifiedScale);
}

void UBar::ModifyPaddleSpeed(float Multiplier)
{
	OutputDebugStringA("ModifyPaddleSpeed called\n");

	float modifiedSpeed = Speed * Multiplier;
	SetSpeed(modifiedSpeed);
}

void UBar::ModifyBallSpeed(float Multiplier)
{
	OutputDebugStringA("ModifyBallSpeed called\n");

	if (USceneManager::GetInstance().GetCurrentSceneName() == ESceneType::InGame)
	{
		UGameScene* gameScene = dynamic_cast<UGameScene*>(USceneManager::GetInstance().GetCurrentScene());

		if (gameScene == nullptr)
			return;

		std::vector<UBall*>& balls = gameScene->GetActiveBalls();

		if (balls.empty())
			return;

		for (int i = 0; i < balls.size(); i++)
		{
			balls[i]->SetSpeed(balls[i]->GetSpeed() * Multiplier);
		}
	}
}

void UBar::AddBullet(int BulletCount)
{
	OutputDebugStringA("AddBullet called\n");

	LoadedBulletCount += BulletCount;
}

void UBar::InitBarItemEffect()
{
	SetSpeed(1.0f);
	SetScale(0.15f);
}

std::vector<UBullet>& UBar::GetFlyingBulletVec()
{
	return FlyingBullet;
}

void UBar::Shoot()
{
	if (LoadedBulletCount == 0)
	{
		return;
	}

	auto CurrentTime = std::chrono::steady_clock::now();
	std::chrono::duration<float> ElapsedTime = CurrentTime - LastFireTime;
	if (ElapsedTime.count() < ShootInterval)
	{
		return;
	}

	for (UBullet& b : FlyingBullet)
	{
		if (!b.GetIsFlying())
		{
			b.SetIsFlying(true);
			if (CurrentShootSide == EDirection::Left)
			{
				b.SetLocation(Location + FVector(-XLength, 0.0f, 0.0f));
				CurrentShootSide = EDirection::Right;
			}
			else
			{
				b.SetLocation(Location + FVector(XLength, 0.0f, 0.0f));
				CurrentShootSide = EDirection::Left;
			}
			LastFireTime = CurrentTime;
			LoadedBulletCount--;
			return;
		}
	}
}