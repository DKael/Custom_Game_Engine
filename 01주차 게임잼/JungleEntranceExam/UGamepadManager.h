#pragma once

#include <windows.gaming.input.h>
#include <vector>
#include <cmath>

using namespace ABI::Windows::Gaming::Input;

class UGamepadManager 
{
public:
	UGamepadManager();

	~UGamepadManager();
	void Update();
	bool IsPressed(GamepadButtons btn) const;
	float GetLeftThumbstickX() const;

	bool IsConnected() const { return pGamepad != nullptr; }
private:
	IGamepadStatics* pGamepadStatics = nullptr;
	IGamepad* pGamepad = nullptr;
	GamepadReading currentReading = {};

	const float Deadzone = 0.15f;
};