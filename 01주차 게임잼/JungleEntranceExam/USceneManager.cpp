#include "USceneManager.h"
#include "TitleScene.h"
#include "UGameScene.h"
#include "UClearScene.h"
#include "USoundManager.h"

USceneManager* USceneManager::sceneManager = nullptr;

void USceneManager::LoadScene(ESceneType sceneName)
{
	currentScene->SetActive(false);

	//타이틀 화면 혹은 인게임 화면으로 변환
	if (currentScene->GetSceneType() == sceneName) {
		return;
	}

    switch (sceneName)
    {
    case ESceneType::Title:
        currentScene = titleScene;
        if (USoundManager::GetInstance().IsPlaying("BGM") == false)
        {
            USoundManager::GetInstance().StopAll();
            USoundManager::GetInstance().Play("BGM");
        }
        break;

    case ESceneType::InGame:
        inGameScene->Init(); 
        currentScene = inGameScene;
        if (USoundManager::GetInstance().IsPlaying("BGM") == false)
        {
            USoundManager::GetInstance().StopAll();
            USoundManager::GetInstance().Play("BGM");
        }
        break;

    case ESceneType::Clear:
        currentScene = clearScene;
        break;

    case ESceneType::None:
        break;
    }

    if (currentScene != nullptr)
    {
        currentScene->Init();
    }

	currentScene->SetActive(true);
}

ESceneType USceneManager::GetCurrentSceneName() const
{
	return currentScene->GetSceneType();
}

void USceneManager::AddObjectToCurrentScene(UGameObject* Object)
{
	currentScene->AddObject(Object);
}

void USceneManager::Release()
{
    if (sceneManager != nullptr)
    {
        delete sceneManager;
        sceneManager = nullptr;
    }
}

USceneManager::USceneManager()
{
	titleScene = new UTitleScene();
	inGameScene = new UGameScene();
    clearScene = new UClearScene();
	currentScene = titleScene;
}

USceneManager::~USceneManager()
{
	titleScene->Release();
	currentScene->Release();

	delete titleScene;
	delete inGameScene;
}
