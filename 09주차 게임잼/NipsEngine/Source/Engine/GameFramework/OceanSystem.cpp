#include "GameFramework/OceanSystem.h"

#include <algorithm>

void FOceanSystem::SetWaterProfile(const FOceanWaterProfile& InProfile)
{
    WaterProfile = InProfile;
    ++WaterProfileRevision;
}

void FOceanSystem::RegisterGlobalOceanActor(const AActor* Actor)
{
    if (Actor == nullptr)
    {
        return;
    }

    auto It = std::find(RegisteredOceanActors.begin(), RegisteredOceanActors.end(), Actor);
    if (It == RegisteredOceanActors.end())
    {
        RegisteredOceanActors.push_back(Actor);
    }
}

void FOceanSystem::UnregisterGlobalOceanActor(const AActor* Actor)
{
    if (Actor == nullptr)
    {
        return;
    }

    auto It = std::remove(RegisteredOceanActors.begin(), RegisteredOceanActors.end(), Actor);
    RegisteredOceanActors.erase(It, RegisteredOceanActors.end());
}

