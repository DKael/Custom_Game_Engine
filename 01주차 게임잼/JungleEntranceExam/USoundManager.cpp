#include "USoundManager.h"
#include <iostream>
#include <Windows.h>

// 정적 변수 초기화
USoundManager* USoundManager::instance = nullptr;

void USoundManager::Init()
{
    FMOD::System_Create(&pSystem);

    pSystem->init(32, FMOD_INIT_NORMAL, nullptr);

    USoundManager::GetInstance().LoadSound("BGM", "bgm.wav", true);
    USoundManager::GetInstance().LoadSound("Hit", "hit.wav", false);
    USoundManager::GetInstance().LoadSound("Brick", "brick.wav", false);
    USoundManager::GetInstance().LoadSound("Victory", "victory.wav", false);
    USoundManager::GetInstance().LoadSound("Start", "start.ogg", false);
    USoundManager::GetInstance().LoadSound("Damage", "damage.mp3", false);
    USoundManager::GetInstance().LoadSound("GameOver", "gameOver.wav", false);

    USoundManager::GetInstance().Play("BGM");
}

void USoundManager::Update()
{
    if (pSystem != nullptr) {
        pSystem->update();
    }
}

void USoundManager::Release()
{
    // 로드해둔 모든 사운드(CD) 메모리 해제
    for (auto& pair : SoundMap) {
        pair.second->release();
    }
    SoundMap.clear();

    // 시스템 종료
    if (pSystem != nullptr) {
        pSystem->close();
        pSystem->release();
        pSystem = nullptr;
    }

    if (instance != nullptr)
    {
        delete instance;
        instance = nullptr;
    }
}

void USoundManager::LoadSound(const std::string& name, const std::string& filepath, bool bLoop)
{
    if (SoundMap.find(name) != SoundMap.end()) return;

    FMOD::Sound* newSound = nullptr;
    FMOD_MODE mode = bLoop ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF;

    FMOD_RESULT result = pSystem->createSound(filepath.c_str(), mode, nullptr, &newSound);

    if (result == FMOD_OK)
    {
        SoundMap[name] = newSound;
    }
    else
    {
        char errorMsg[256];
        sprintf_s(errorMsg, "[사운드 에러] %s 파일 로드 실패! (에러코드: %d)\n", filepath.c_str(), result);
        OutputDebugStringA(errorMsg);
    }
}

void USoundManager::Play(const std::string& name)
{
    auto iter = SoundMap.find(name);
    if (iter != SoundMap.end())
    {
        FMOD::Channel* pChannel = nullptr;
        pSystem->playSound(iter->second, nullptr, false, &pChannel);

        // 받은 리모컨을 보관함에 이름표를 붙여서 저장!
        ChannelMap[name] = pChannel;
    }
}

// 특정 소리 하나만 끄기 
void USoundManager::Stop(const std::string& name)
{
    auto iter = ChannelMap.find(name);
    if (iter != ChannelMap.end())
    {
        iter->second->stop();
    }
}

void USoundManager::StopAll()
{
    FMOD::ChannelGroup* masterGroup = nullptr;
    pSystem->getMasterChannelGroup(&masterGroup);

    if (masterGroup != nullptr) {
        masterGroup->stop();
    }
}

void USoundManager::StopSFX()
{
    for (auto& pair : ChannelMap)
    {
        if (pair.first != "BGM")
        {
            pair.second->stop();
        }
    }
}

bool USoundManager::IsPlaying(const std::string& name)
{
    auto iter = ChannelMap.find(name);
    if (iter != ChannelMap.end())
    {
        bool bPlaying = false;

        FMOD_RESULT result = iter->second->isPlaying(&bPlaying);

        if (result == FMOD_OK) {
            return bPlaying;
        }
    }

    return false;
}
