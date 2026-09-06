#pragma once

//게임을 계속 진행하는지 혹은 종료시킬지 상태를 파악하는 매니저
class UGameManager
{
private:
	UGameManager();
	~UGameManager() = default;

	void initialize();

public:
	
	static UGameManager* GetInstance();
	void Update(float deltaTime);
	void Exit();

	int GetTotalScore() const { return currentScore; }
	int GetCurLife() const { return currentHealth; }
	void AddScore(const unsigned int value);
	void SetScore(const unsigned int value);
	void RessetGM();
	void Release();

	void AddHealth(const unsigned int value);
	void SubHealth(const unsigned int value);

private:

	const unsigned int MaxHealth = 3;
	unsigned int currentHealth;
	unsigned int currentScore;

	static UGameManager* gameManager;
};

