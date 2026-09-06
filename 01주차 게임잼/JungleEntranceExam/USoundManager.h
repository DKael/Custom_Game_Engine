#pragma once
#include <string>
#include <map>

#include "fmod.hpp"

class USoundManager
{
public:
    static USoundManager& GetInstance() {
        if (instance == nullptr) {
            instance = new USoundManager();
        }
        return *instance;
    }

    void Init();
    void Update();
    void Release();

    void LoadSound(const std::string& name, const std::string& filepath, bool bLoop = false);

    void Play(const std::string& name);
    void Stop(const std::string& name); 
    void StopAll();                    
    void StopSFX();
    bool IsPlaying(const std::string& name);
private:
    USoundManager() = default;
    ~USoundManager() = default;

    static USoundManager* instance;

    FMOD::System* pSystem = nullptr;

    std::map<std::string, FMOD::Sound*> SoundMap; 
    std::map<std::string, FMOD::Channel*> ChannelMap;
};

