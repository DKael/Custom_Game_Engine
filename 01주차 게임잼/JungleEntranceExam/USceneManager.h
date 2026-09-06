#pragma once
#include "Types.h"

class UScene;
class UGameObject;

class USceneManager
{
public:

	static USceneManager& GetInstance() {

		if (sceneManager == nullptr) {
			sceneManager = new USceneManager();
		}
		return *sceneManager;
	}

	void LoadScene(ESceneType sceneName);
	UScene* GetCurrentScene() const { return currentScene; }
	ESceneType GetCurrentSceneName() const;
	void AddObjectToCurrentScene(UGameObject* Object);
	
	void Release();
private:
	USceneManager();
	~USceneManager();

private:
	UScene* titleScene = nullptr;
	UScene* inGameScene = nullptr;
	UScene* clearScene = nullptr;
	UScene* currentScene = nullptr;

	static USceneManager* sceneManager;
};

