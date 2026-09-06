#include "Core/EnginePCH.h"
#include "Editor/Utility/EditorLuaScriptUtils.h"

#include "Component/Script/LuaScriptPathUtils.h"
#include "Component/Script/ScriptComponent.h"
#include "Engine/Core/Paths.h"
#include "Engine/GameFramework/Actor.h"
#include "Engine/GameFramework/World.h"
#include "Engine/Runtime/Engine.h"

#include <filesystem>
#include <shellapi.h>

namespace
{
    constexpr const char* ScriptTemplatePath = "Asset/Scripts/template.lua";

    UScriptComponent* FindScriptComponent(AActor* Actor)
    {
        if (Actor == nullptr)
        {
            return nullptr;
        }

        for (UActorComponent* Component : Actor->GetComponents())
        {
            if (UScriptComponent* ScriptComponent = Cast<UScriptComponent>(Component))
            {
                return ScriptComponent;
            }
        }

        return nullptr;
    }

    bool AssignScriptToActor(UScriptComponent* ScriptComponent, const FString& ScriptPath)
    {
        if (ScriptComponent == nullptr)
        {
            return false;
        }

        ScriptComponent->SetScriptPath(ScriptPath);
        return ScriptComponent->ReloadScript();
    }

    bool CopyTemplateLuaIfNeeded(const FString& TargetPath, bool& bOutCreated)
    {
        bOutCreated = false;

        const std::filesystem::path TargetAbsolutePath(FPaths::ToAbsolute(FPaths::ToWide(TargetPath)));
        std::error_code ErrorCode;
        if (std::filesystem::exists(TargetAbsolutePath, ErrorCode))
        {
            return true;
        }

        const std::filesystem::path TemplateAbsolutePath(
            FPaths::ToAbsolute(FPaths::ToWide(FString(ScriptTemplatePath))));
        if (!std::filesystem::exists(TemplateAbsolutePath, ErrorCode))
        {
            UE_LOG("[Lua] Missing actor type script template: %s\n", ScriptTemplatePath);
            return false;
        }

        std::filesystem::create_directories(TargetAbsolutePath.parent_path(), ErrorCode);
        if (ErrorCode)
        {
            UE_LOG("[Lua] Failed to create actor type script directory: %s\n",
                FPaths::ToUtf8(TargetAbsolutePath.parent_path().generic_wstring()).c_str());
            return false;
        }

        std::filesystem::copy_file(TemplateAbsolutePath, TargetAbsolutePath, std::filesystem::copy_options::none, ErrorCode);
        if (ErrorCode)
        {
            UE_LOG("[Lua] Failed to create actor type script '%s' from template '%s'.\n",
                TargetPath.c_str(),
                ScriptTemplatePath);
            return false;
        }

        bOutCreated = true;
        return true;
    }
}

namespace EditorLuaScriptUtils
{
    bool CreateActorTypeScript(UScriptComponent* ScriptComponent)
    {
        if (ScriptComponent == nullptr)
        {
            UE_LOG("[Lua] Create Type Script failed: ScriptComponent is null.\n");
            return false;
        }

        AActor* Owner = ScriptComponent->GetOwner();
        if (Owner == nullptr)
        {
            UE_LOG("[Lua] Create Type Script failed: ScriptComponent has no owner actor.\n");
            return false;
        }

        const FString ActorTypeName = LuaScriptPathUtils::GetActorScriptTypeName(Owner);
        const FString TargetScriptPath = LuaScriptPathUtils::GetActorTypeScriptPath(Owner);

        bool bCreatedScriptFile = false;
        if (!CopyTemplateLuaIfNeeded(TargetScriptPath, bCreatedScriptFile))
        {
            return false;
        }

        if (bCreatedScriptFile)
        {
            UE_LOG("[Lua] Created shared actor-type script '%s' from template for type '%s'.\n",
                TargetScriptPath.c_str(),
                ActorTypeName.c_str());
        }
        else
        {
            UE_LOG("[Lua] Reusing existing shared actor-type script '%s' for type '%s'.\n",
                TargetScriptPath.c_str(),
                ActorTypeName.c_str());
        }

        if (!AssignScriptToActor(ScriptComponent, TargetScriptPath))
        {
            UE_LOG("[Lua] Failed to assign shared actor-type script '%s' to actor '%s'.\n",
                TargetScriptPath.c_str(),
                Owner->GetFName().ToString().c_str());
            return false;
        }

        if (GEngine != nullptr)
        {
            GEngine->GetLuaScriptSubsystem().RefreshAvailableScriptPaths();
            GEngine->GetLuaScriptSubsystem().SetScriptHotReloadEnabled(true);
        }

        UE_LOG("[Lua] Assigned shared actor-type script '%s' to actor '%s'.\n",
            TargetScriptPath.c_str(),
            Owner->GetFName().ToString().c_str());
        return true;
    }

    bool ApplyScriptToSameTypeActors(UScriptComponent* ScriptComponent, int32& OutUpdatedActors, int32& OutSkippedActors)
    {
        OutUpdatedActors = 0;
        OutSkippedActors = 0;

        if (ScriptComponent == nullptr)
        {
            UE_LOG("[Lua] Apply To Same Type failed: ScriptComponent is null.\n");
            return false;
        }

        AActor* Owner = ScriptComponent->GetOwner();
        if (Owner == nullptr)
        {
            UE_LOG("[Lua] Apply To Same Type failed: ScriptComponent has no owner actor.\n");
            return false;
        }

        const FString NormalizedScriptPath = FPaths::Normalize(ScriptComponent->GetScriptPath());
        if (NormalizedScriptPath.empty())
        {
            UE_LOG("[Lua] Apply To Same Type skipped for actor '%s': ScriptPath is empty.\n",
                Owner->GetFName().ToString().c_str());
            return false;
        }

        std::error_code ErrorCode;
        const std::filesystem::path AbsoluteScriptPath(FPaths::ToAbsolute(FPaths::ToWide(NormalizedScriptPath)));
        if (!std::filesystem::exists(AbsoluteScriptPath, ErrorCode))
        {
            UE_LOG("[Lua] Apply To Same Type skipped for actor '%s': script file does not exist (%s).\n",
                Owner->GetFName().ToString().c_str(),
                NormalizedScriptPath.c_str());
            return false;
        }

        UWorld* World = Owner->GetFocusedWorld();
        if (World == nullptr)
        {
            UE_LOG("[Lua] Apply To Same Type failed: actor '%s' is not assigned to a world.\n",
                Owner->GetFName().ToString().c_str());
            return false;
        }

        const FString ActorTypeName = LuaScriptPathUtils::GetActorScriptTypeName(Owner);
        for (AActor* CandidateActor : World->GetActors())
        {
            if (CandidateActor == nullptr || LuaScriptPathUtils::GetActorScriptTypeName(CandidateActor) != ActorTypeName)
            {
                continue;
            }

            UScriptComponent* TargetScriptComponent = FindScriptComponent(CandidateActor);
            if (TargetScriptComponent == nullptr)
            {
                ++OutSkippedActors;
                continue;
            }

            const FString PreviousScriptPath = TargetScriptComponent->GetScriptPath();
            if (!PreviousScriptPath.empty() && FPaths::Normalize(PreviousScriptPath) != NormalizedScriptPath)
            {
                UE_LOG("[Lua] Overwriting script path on actor '%s': '%s' -> '%s'.\n",
                    CandidateActor->GetFName().ToString().c_str(),
                    PreviousScriptPath.c_str(),
                    NormalizedScriptPath.c_str());
            }

            if (AssignScriptToActor(TargetScriptComponent, NormalizedScriptPath))
            {
                ++OutUpdatedActors;
            }
            else
            {
                UE_LOG("[Lua] Failed to apply script '%s' to actor '%s'.\n",
                    NormalizedScriptPath.c_str(),
                    CandidateActor->GetFName().ToString().c_str());
            }
        }

        UE_LOG("[Lua] Apply To Same Type: type='%s', script='%s', updated=%d, skipped_without_script_component=%d.\n",
            ActorTypeName.c_str(),
            NormalizedScriptPath.c_str(),
            OutUpdatedActors,
            OutSkippedActors);
        return OutUpdatedActors > 0 || OutSkippedActors > 0;
    }

    bool OpenScriptInExternalEditor(const FString& ScriptPath)
    {
        if (ScriptPath.empty())
        {
            MessageBoxW(
                nullptr,
                L"No script file is assigned.",
                L"Warning",
                MB_OK | MB_ICONWARNING);
            return false;
        }

        const FWString AbsoluteScriptPath = FPaths::ToAbsolute(FPaths::ToWide(FPaths::Normalize(ScriptPath)));
        if (!std::filesystem::exists(std::filesystem::path(AbsoluteScriptPath)))
        {
            MessageBoxW(
                nullptr,
                L"The script file could not be found.",
                L"Warning",
                MB_OK | MB_ICONWARNING);
            return false;
        }

        HINSTANCE Result = ShellExecuteW(
            nullptr,
            L"open",
            AbsoluteScriptPath.c_str(),
            nullptr,
            nullptr,
            SW_SHOWNORMAL);

        if (reinterpret_cast<INT_PTR>(Result) <= 32)
        {
            MessageBoxW(
                nullptr,
                L"Failed to open the script file.",
                L"Error",
                MB_OK | MB_ICONERROR);
            return false;
        }

        return true;
    }
}
