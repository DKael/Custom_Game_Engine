#include "UGameManager.h"
#include "USceneManager.h"
#include "USoundManager.h"
#include "UItemManager.h"
#include "FileInfo.h"
UGameManager* UGameManager::gameManager = nullptr;

UGameManager::UGameManager()
{
	RessetGM();
}

void UGameManager::initialize()
{
}

void UGameManager::Exit()
{
	RessetGM();

	USoundManager::GetInstance().Play("GameOver");
	USceneManager::GetInstance().LoadScene(ESceneType::Title);
}

UGameManager* UGameManager::GetInstance()
{
	if (gameManager == nullptr) {
		gameManager = new UGameManager();
	}
	return gameManager;
}

void UGameManager::Update(float deltaTime)
{

}

void UGameManager::AddScore(const unsigned int value)
{
	currentScore += value;
	//UpdateScore
}

void UGameManager::SetScore(const unsigned int value)
{
	currentScore = value;
}

void UGameManager::RessetGM()
{
	currentHealth = MaxHealth;
	currentScore = 0;
}

void UGameManager::Release()
{
	if (gameManager != nullptr)
	{
		delete gameManager;
		gameManager = nullptr;
	}
}

void UGameManager::AddHealth(const unsigned int value)
{
	currentHealth += value;
	currentHealth = currentHealth < MaxHealth ? currentHealth : MaxHealth;
}

void UGameManager::SubHealth(const unsigned int value)
{
	currentHealth = max(currentHealth - value, 0);
	if (value >= currentHealth) {
		
		return;
	}
}