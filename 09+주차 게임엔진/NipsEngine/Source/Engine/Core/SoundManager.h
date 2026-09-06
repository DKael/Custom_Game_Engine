#pragma once
#include "Core/Singleton.h"
#include "Core/Containers/String.h"
#include <fmod.hpp>
#include <unordered_map>


constexpr const char* SoundBGMPath = "Asset/Sound/BGM/";
constexpr const char* SoundSFXPath = "Asset/Sound/SFX/";

class FSoundManager : public TSingleton<FSoundManager>
{
	friend class TSingleton<FSoundManager>;
public:
	void Initialize();
	void Release();
	void Update();

	// 파일명만 전달 (예: "menu.wav", "jump.wav")
	void PlayBGM(const FString& FileName, float Volume = 0.1f);
	void PlaySFX(const FString& FileName, float Volume = 0.1f, bool bLoop = false);
	void StopSFX(const FString& FileName);
	void StopBGM();
	void StopAll();
	void SetBGMVolume(float Volume);
	void SetSFXVolume(float Volume);

private:
	FSoundManager() = default;

	// Asset/Sound/ 하위를 순회하며 사운드 파일을 자동으로 로드
	// BGM/ 폴더 → 스트리밍 + 루프, SFX/ 폴더 → 메모리 로드
	void ScanAndLoad();

	FMOD::System*       System     = nullptr;
	FMOD::ChannelGroup* BGMGroup   = nullptr;
	FMOD::ChannelGroup* SFXGroup   = nullptr;
	FMOD::Channel*      BGMChannel = nullptr;

	TMap<FString, FMOD::Sound*> Sounds;
	TMap<FString, FMOD::Channel*> LoopingSFXChannels;
};
