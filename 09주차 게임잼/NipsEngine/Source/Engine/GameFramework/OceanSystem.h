#pragma once

#include "Core/Singleton.h"
#include "Render/Common/WaterRenderingCommon.h"

class AActor;

using FOceanWaterProfile = FWaterSurfaceProfile;

class FOceanSystem : public TSingleton<FOceanSystem>
{
    friend class TSingleton<FOceanSystem>;

public:
    const FOceanWaterProfile& GetWaterProfile() const { return WaterProfile; }
    void SetWaterProfile(const FOceanWaterProfile& InProfile);

    uint64 GetWaterProfileRevision() const { return WaterProfileRevision; }

    void RegisterGlobalOceanActor(const AActor* Actor);
    void UnregisterGlobalOceanActor(const AActor* Actor);
    uint32 GetRegisteredActorCount() const { return static_cast<uint32>(RegisteredOceanActors.size()); }

private:
    FOceanSystem() = default;
    ~FOceanSystem() = default;

private:
    FOceanWaterProfile WaterProfile = {};
    uint64 WaterProfileRevision = 1;
    TArray<const AActor*> RegisteredOceanActors;
};

