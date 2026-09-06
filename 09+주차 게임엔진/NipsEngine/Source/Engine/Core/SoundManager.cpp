#include "SoundManager.h"
#include "Core/Paths.h"
#include <filesystem>

void FSoundManager::Initialize()
{
	FMOD::System_Create(&System);
	System->init(512, FMOD_INIT_NORMAL, nullptr);
	System->createChannelGroup("BGM", &BGMGroup);
	System->createChannelGroup("SFX", &SFXGroup);

	ScanAndLoad();
}

void FSoundManager::ScanAndLoad()
{
	namespace fs = std::filesystem;
	const fs::path SoundRoot = fs::path(FPaths::RootDir()) / L"Asset" / L"Sound";

	if (!fs::exists(SoundRoot) || !fs::is_directory(SoundRoot))
		return;

	for (const auto& Entry : fs::recursive_directory_iterator(SoundRoot))
	{
		if (!Entry.is_regular_file()) continue;

		const fs::path& FilePath = Entry.path();
		const FWString Ext = FilePath.extension().wstring();

		if (Ext != L".wav" && Ext != L".mp3" && Ext != L".ogg") continue;

		const FString RelPath = FPaths::ToRelativeString(FilePath.generic_wstring());
		const FString AbsPath = FPaths::ToAbsoluteString(FPaths::ToWide(RelPath));

		// generic_wstring()은 항상 / 구분자를 사용하므로 /BGM/ 검사로 충분
		const bool bIsBGM = FilePath.generic_wstring().find(L"/BGM/") != FWString::npos;

		const FMOD_MODE Mode = bIsBGM
			? (FMOD_LOOP_NORMAL | FMOD_CREATESTREAM)
			: FMOD_DEFAULT;

		FMOD::Sound* Sound = nullptr;
		if (System->createSound(AbsPath.c_str(), Mode, nullptr, &Sound) == FMOD_OK)
		{
			Sounds[RelPath] = Sound;
		}
	}
}

void FSoundManager::Release()
{
	for (auto& [Key, Channel] : LoopingSFXChannels)
	{
		if (Channel) Channel->stop();
	}
	LoopingSFXChannels.clear();

	for (auto& [Key, Sound] : Sounds)
	{
		if (Sound) Sound->release();
	}
	Sounds.clear();

	if (BGMGroup)  { BGMGroup->release();    BGMGroup   = nullptr; }
	if (SFXGroup)  { SFXGroup->release();    SFXGroup   = nullptr; }
	if (System)    { System->close(); System->release(); System = nullptr; }
}

void FSoundManager::PlayBGM(const FString& FileName, float Volume)
{
	auto It = Sounds.find(SoundBGMPath + FileName);
	if (It == Sounds.end()) return;

	if (BGMChannel) BGMChannel->stop();

	System->playSound(It->second, BGMGroup, false, &BGMChannel);
	if (BGMChannel) BGMChannel->setVolume(Volume);
}

void FSoundManager::PlaySFX(const FString& FileName, float Volume, bool bLoop)
{
	const FString SoundKey = SoundSFXPath + FileName;
	auto It = Sounds.find(SoundKey);
	if (It == Sounds.end()) return;

	if (bLoop)
	{
		FMOD::Channel* ExistingChannel = nullptr;
		auto ChannelIt = LoopingSFXChannels.find(SoundKey);
		if (ChannelIt != LoopingSFXChannels.end())
		{
			ExistingChannel = ChannelIt->second;
			bool bIsPlaying = false;
			if (ExistingChannel && ExistingChannel->isPlaying(&bIsPlaying) == FMOD_OK && bIsPlaying)
			{
				ExistingChannel->setVolume(Volume);
				return;
			}
		}

		FMOD::Channel* LoopChannel = nullptr;
		System->playSound(It->second, SFXGroup, true, &LoopChannel);
		if (LoopChannel)
		{
			LoopChannel->setMode(FMOD_LOOP_NORMAL);
			LoopChannel->setVolume(Volume);
			LoopChannel->setPaused(false);
			LoopingSFXChannels[SoundKey] = LoopChannel;
		}
		return;
	}

	FMOD::Channel* Ch = nullptr;
	System->playSound(It->second, SFXGroup, false, &Ch);
	if (Ch)
	{
		Ch->setVolume(Volume);
		LoopingSFXChannels[SoundKey] = Ch;
	}
}

void FSoundManager::StopSFX(const FString& FileName)
{
	const FString SoundKey = SoundSFXPath + FileName;
	auto It = LoopingSFXChannels.find(SoundKey);
	if (It == LoopingSFXChannels.end())
	{
		return;
	}

	if (It->second)
	{
		It->second->stop();
	}
	LoopingSFXChannels.erase(It);
}

void FSoundManager::StopBGM()
{
	if (BGMChannel) BGMChannel->stop();
}

void FSoundManager::StopAll()
{
	if (BGMGroup) BGMGroup->stop();
	if (SFXGroup) SFXGroup->stop();
	LoopingSFXChannels.clear();
}

void FSoundManager::SetBGMVolume(float Volume)
{
	if (BGMGroup) BGMGroup->setVolume(Volume);
}

void FSoundManager::SetSFXVolume(float Volume)
{
	if (SFXGroup) SFXGroup->setVolume(Volume);
}

void FSoundManager::Update()
{
	for (auto It = LoopingSFXChannels.begin(); It != LoopingSFXChannels.end();)
	{
		bool bIsPlaying = false;
		FMOD::Channel* Channel = It->second;
		if (!Channel || Channel->isPlaying(&bIsPlaying) != FMOD_OK || !bIsPlaying)
		{
			It = LoopingSFXChannels.erase(It);
			continue;
		}

		++It;
	}

	if (System) System->update();
}
