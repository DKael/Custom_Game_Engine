#include "UGamepadManager.h"
extern "C" {
	HRESULT __stdcall RoGetActivationFactory(HSTRING activatableClassId, REFIID iid, void** factory);
	HRESULT __stdcall WindowsCreateStringReference(const wchar_t* sourceString, uint32_t length, HSTRING_HEADER* hstringHeader, HSTRING* string);
	HRESULT __stdcall RoInitialize(int initType);
	void    __stdcall RoUninitialize();
;
}
#pragma comment(lib, "runtimeobject.lib")
UGamepadManager::UGamepadManager()
{
	RoInitialize(1);
	//// HSTRING 积己 霖厚
	HSTRING_HEADER hstrHeader;
	HSTRING hClassName;
	const wchar_t* className = RuntimeClass_Windows_Gaming_Input_Gamepad;
	// HSTRING 积己
	HRESULT hr = WindowsCreateStringReference(className, (unsigned int)wcslen(className), &hstrHeader, &hClassName);
	if (SUCCEEDED(hr))
		RoGetActivationFactory(hClassName, __uuidof(IGamepadStatics), (void**)&pGamepadStatics);

}

UGamepadManager::~UGamepadManager()
{
	if (pGamepad)
	{
		pGamepad->Release();
		pGamepad = nullptr;
	}
	if (pGamepadStatics) {
		pGamepadStatics->Release();
		pGamepadStatics = nullptr;
	}
	RoUninitialize();
}

void UGamepadManager::Update()
{
	if (!pGamepadStatics)
		return;

	if (pGamepad == nullptr) {
		ABI::Windows::Foundation::Collections::IVectorView<Gamepad*>* InGamepad = nullptr;
		if (SUCCEEDED(pGamepadStatics->get_Gamepads(&InGamepad)) && InGamepad)
		{
			unsigned int count = 0;
			InGamepad->get_Size(&count);
			if (count > 0)
			{
				InGamepad->GetAt(0, &pGamepad);
			}
			InGamepad ->Release();
		}
	}
	if (pGamepad)
	{
		HRESULT hr = pGamepad->GetCurrentReading(&currentReading);
		if (FAILED(hr))
		{
			pGamepad->Release();
			pGamepad = nullptr;

		}
	}

}

bool UGamepadManager::IsPressed(GamepadButtons btn) const
{
	if (!pGamepad)
		return false;
	return (currentReading.Buttons & btn) == btn;
}

float UGamepadManager::GetLeftThumbstickX() const
{
	if (!pGamepad) 
		return 0.0f;
	float val = (float)currentReading.LeftThumbstickX;

	if (val > -Deadzone && val < Deadzone)
		return 0.0f;

	return val;
}
