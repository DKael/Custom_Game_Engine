#pragma once

#include "Core/Containers/Array.h"
#include "Core/Containers/String.h"

namespace ActorTags
{
    inline constexpr const char* Untagged = "Untagged";

    // --- Drift Salvage 게임 전용 Tag ---
    // 회수 가능: Trash / Resource / Recyclable / Premium
    // 회수 불가/위험: Rock / Hazard
    // 특수: Boat / Lighthouse
    inline constexpr const char* Boat       = "Boat";       // 플레이어 배
    inline constexpr const char* Trash      = "Trash";      // 가벼운 가치 없는 쓰레기
    inline constexpr const char* Resource   = "Resource";   // 일반 회수 자원
    inline constexpr const char* Recyclable = "Recyclable"; // 재활용 고가치
    inline constexpr const char* Premium    = "Premium";    // RAM 등 고가치 희귀
    inline constexpr const char* Hazard     = "Hazard";     // 폭발물 (HP 감소)
    inline constexpr const char* Rock       = "Rock";       // 암초/바위 (Block + HP)
    inline constexpr const char* Lighthouse = "Lighthouse"; // 도착 지점

    inline constexpr const char* Tire       = "Tire";

    inline const TArray<FString>& GetBuiltinTags()
    {
        static const TArray<FString> Tags =
        {
            Untagged,
            "Respawn",
            "Finish",
            "EditorOnly",
            "MainCamera",
            "Player",
            "GameController",
            "Coin",
            "Boss",
            "Enemy",
            "Collectible",
            "Environment",
            // Drift Salvage
            Boat,
            Trash,
            Resource,
            Recyclable,
            Premium,
            Hazard,
            Rock,
            Lighthouse,
            Tire,
        };

        return Tags;
    }

    inline FString Normalize(const FString& Tag)
    {
        return Tag.empty() ? FString(Untagged) : Tag;
    }
}
