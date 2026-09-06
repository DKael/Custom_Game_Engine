#include "UInputManager.h"

UInputManager* UInputManager::instance = nullptr;

UInputManager* UInputManager::GetInstance()
{
    if (instance == nullptr) {
        instance = new UInputManager();
    }
    return instance;
}

void UInputManager::Update()
{
    for (int i = 0; i < 256; ++i)
    {
        if (GetAsyncKeyState(i) & 0x8000)
        {
            if (KeyStates[i] == EKeyState::None || KeyStates[i] == EKeyState::Up) {
                KeyStates[i] = EKeyState::Down;
            }
            else if (KeyStates[i] == EKeyState::Down) {
                KeyStates[i] = EKeyState::Press;
            }
        }
        else
        {
            if (KeyStates[i] == EKeyState::Down || KeyStates[i] == EKeyState::Press) {
                KeyStates[i] = EKeyState::Up;
            }
            else if (KeyStates[i] == EKeyState::Up) {
                KeyStates[i] = EKeyState::None;
            }
        }
    }
    GamepadMgr.Update();

	PlayerAxesX[0] = 0.0f;
    if (GetKeyPress(VK_LEFT) && GetKeyPress(VK_RIGHT))
    {
        PlayerAxesX[0] = 0.0f;
    }
    if (GetKeyPress(VK_LEFT))
    {
        PlayerAxesX[0] = -1.0f;
    }
    if (GetKeyPress(VK_RIGHT))
    {
        PlayerAxesX[0] = 1.0f;
    }

    PlayerAxesX[1] = 0.0f;
    if (GamepadMgr.IsConnected())
    {
		PlayerAxesX[1] = GamepadMgr.GetLeftThumbstickX();
    }
    else
    {
        if (GetKeyPress('A') && GetKeyPress('D'))
        {
            PlayerAxesX[1] = 0.0f;
        }
        if (GetKeyPress('A'))
        {
            PlayerAxesX[1] = -1.0f;
        }
        if (GetKeyPress('D'))
        {
            PlayerAxesX[1] = 1.0f;
        }
    }

}

void UInputManager::Release()
{
    if (instance != nullptr)
    {
        delete instance;
        instance = nullptr;
    }
}

float UInputManager::GetAxisX(int playerIndex) const
{
    return (playerIndex >= 0 && playerIndex < 2) ? PlayerAxesX[playerIndex] : 0.0f;
}


UInputManager::UInputManager() {
    for (int i = 0; i < 256; ++i) {
        KeyStates[i] = EKeyState::None;
    }
}

UInputManager::~UInputManager()
{
    
}
