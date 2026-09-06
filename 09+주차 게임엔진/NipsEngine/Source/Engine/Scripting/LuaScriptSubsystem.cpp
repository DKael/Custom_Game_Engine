#include "Core/EnginePCH.h"
#include "Scripting/LuaScriptSubsystem.h"

#include "Engine/Scripting/LuaBinder.h"
#include "Engine/Component/Script/ScriptComponent.h"
#include "Engine/Core/Paths.h"
#include "Engine/GameFramework/Actor.h"
#include <algorithm>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <set>

namespace
{
    bool ReadFileToStringByWidePath(const FWString& FilePath, FString& OutSource)
    {
        std::ifstream File(std::filesystem::path(FilePath), std::ios::binary);
        if (!File.is_open())
        {
            return false;
        }

        File.seekg(0, std::ios::end);
        const std::streamsize Size = File.tellg();
        File.seekg(0, std::ios::beg);

        if (Size < 0)
        {
            return false;
        }

        OutSource.resize(static_cast<size_t>(Size));
        if (Size == 0)
        {
            return true;
        }

        File.read(OutSource.data(), Size);
        return static_cast<std::streamsize>(File.gcount()) == Size;
    }

    void StripUtf8Bom(FString& Source)
    {
        if (Source.size() >= 3 &&
            static_cast<unsigned char>(Source[0]) == 0xEF &&
            static_cast<unsigned char>(Source[1]) == 0xBB &&
            static_cast<unsigned char>(Source[2]) == 0xBF)
        {
            Source.erase(0, 3);
        }
    }

    // Bind commonly used script globals into an already created environment.
    void PopulateInstanceEnvironment(FLuaScriptInstance& Instance, AActor* Owner, UScriptComponent* OwnerComponent)
    {
        UActorComponent* BaseComponent = OwnerComponent;
        Instance.Env["obj"] = Owner;
        Instance.Env["Self"] = Owner;
        Instance.Env["Owner"] = Owner;
        Instance.Env["script"] = BaseComponent;
        Instance.Env["Component"] = BaseComponent;
        Instance.Env["StartCoroutine"] = [OwnerComponent](const sol::function& Function)
        {
            if (OwnerComponent && UObject::IsValid(OwnerComponent))
            {
                OwnerComponent->StartCoroutine(Function);
            }
        };
        Instance.Env["DestroySelf"] = [Owner]()
        {
            if (Owner && UObject::IsValid(Owner))
            {
                Owner->Destroy();
            }
        };
    }

    // Recreate environment and then bind globals.
    void RecreateAndPopulateInstanceEnvironment(FLuaScriptInstance& Instance, sol::state& Lua, AActor* Owner, UScriptComponent* OwnerComponent)
    {
        Instance.Env = sol::environment(Lua, sol::create, Lua.globals());
        PopulateInstanceEnvironment(Instance, Owner, OwnerComponent);
    }
}

void FLuaScriptSubsystem::LogFunctionError(const FString& FunctionName, const FString& ScriptPath, const char* ErrorMessage) const
{
    UE_LOG("[Lua Function Error] %s (%s) : %s\n",
        FunctionName.c_str(),
        ScriptPath.c_str(),
        ErrorMessage);
}

void FLuaScriptSubsystem::Initialize()
{
    // Shared VM boot. Per-actor isolation is handled by per-instance environments.
    Lua.open_libraries(
        sol::lib::base,
        sol::lib::package,
        sol::lib::math,
        sol::lib::table,
        sol::lib::string,
        sol::lib::coroutine);

    BindEngineTypes();
    BindGlobalFunctions();
    RebuildScriptPathCache();
}

void FLuaScriptSubsystem::Shutdown()
{
    SetScriptHotReloadEnabled(false);
    // Drop runtime references owned by subsystem.
    ScriptInstances.clear();
    AvailableScriptPaths.clear();
    bScriptPathCacheInitialized = false;
}

void FLuaScriptSubsystem::Tick()
{
    if (!bScriptHotReloadEnabled)
    {
        return;
    }

    const TArray<FWString> ChangedFiles = ScriptFileWatcher.DequeueChangedFiles();
    if (ChangedFiles.empty())
    {
        return;
    }

    TArray<FWString> ChangedLuaFiles;
    ChangedLuaFiles.reserve(ChangedFiles.size());
    for (const FWString& ChangedFile : ChangedFiles)
    {
        std::filesystem::path ChangedPath(ChangedFile);
        FWString Extension = ChangedPath.extension().wstring();
        std::transform(Extension.begin(), Extension.end(), Extension.begin(), towlower);
        if (Extension == L".lua")
        {
            ChangedLuaFiles.push_back(ChangedFile);
        }
    }

    if (ChangedLuaFiles.empty())
    {
        return;
    }

    ReloadChangedScripts(ChangedLuaFiles);
}

void FLuaScriptSubsystem::SetScriptHotReloadEnabled(bool bEnabled)
{
    if (bEnabled)
    {
        if (bScriptHotReloadEnabled)
        {
            return;
        }

        bScriptHotReloadEnabled = StartScriptFileWatcher();
        return;
    }

    if (!bScriptHotReloadEnabled)
    {
        return;
    }

    ScriptFileWatcher.Stop();
    bScriptHotReloadEnabled = false;
}

bool FLuaScriptSubsystem::CanInvoke(const std::shared_ptr<FLuaScriptInstance>& Instance) const
{
    if (!Instance || !Instance->bLoaded)
    {
        return false;
    }

    if (Instance->Owner && !UObject::IsValid(Instance->Owner))
    {
        return false;
    }

    if (Instance->OwnerComponent && !UObject::IsValid(Instance->OwnerComponent))
    {
        return false;
    }

    return true;
}

FWString FLuaScriptSubsystem::ResolveScriptPathWide(const FString& ScriptPath) const
{
    return FPaths::ToAbsolute(FPaths::ToWide(FPaths::Normalize(ScriptPath)));
}

FWString FLuaScriptSubsystem::MakeScriptPathKey(const FString& ScriptPath) const
{
    return MakeScriptPathKey(FPaths::ToWide(FPaths::Normalize(ScriptPath)));
}

FWString FLuaScriptSubsystem::MakeScriptPathKey(const FWString& ScriptPath) const
{
    FWString NormalizedPath = std::filesystem::path(FPaths::ToAbsolute(ScriptPath)).lexically_normal().generic_wstring();
    std::transform(NormalizedPath.begin(), NormalizedPath.end(), NormalizedPath.begin(), towlower);
    return NormalizedPath;
}

bool FLuaScriptSubsystem::ShouldPassOwnerAsFirstArgument(
    const std::shared_ptr<FLuaScriptInstance>& Instance,
    const FString& FunctionName,
    const sol::object& FuncObject,
    size_t ExplicitArgCount)
{
    if (!Instance)
    {
        return true;
    }

    if (const auto CacheIt = Instance->CallbackUsesOwnerArgument.find(FunctionName);
        CacheIt != Instance->CallbackUsesOwnerArgument.end())
    {
        return CacheIt->second;
    }

    bool bPassOwnerAsFirstArgument = true;
    lua_State* LuaState = Instance->Env.lua_state();
    if (LuaState != nullptr)
    {
        sol::stack::push(LuaState, FuncObject);
        if (lua_isfunction(LuaState, -1) && !lua_iscfunction(LuaState, -1))
        {
            lua_Debug DebugInfo = {};
            if (lua_getinfo(LuaState, ">u", &DebugInfo) != 0
                && DebugInfo.isvararg == 0
                && DebugInfo.nparams <= static_cast<decltype(DebugInfo.nparams)>(ExplicitArgCount))
            {
                bPassOwnerAsFirstArgument = false;
            }
        }
        else
        {
            lua_pop(LuaState, 1);
        }
    }

    Instance->CallbackUsesOwnerArgument[FunctionName] = bPassOwnerAsFirstArgument;
    return bPassOwnerAsFirstArgument;
}

void FLuaScriptSubsystem::BindEngineTypes()
{
    LuaBinder::BindEngineTypes(Lua);
}

void FLuaScriptSubsystem::BindGlobalFunctions()
{
    LuaBinder::BindGlobalFunctions(Lua);
}

bool FLuaScriptSubsystem::StartScriptFileWatcher()
{
    const std::filesystem::path ScriptRoot = std::filesystem::path(FPaths::AssetDirectoryPath()) / L"Scripts";
    std::error_code ErrorCode;
    if (!std::filesystem::exists(ScriptRoot, ErrorCode))
    {
        return false;
    }

    if (ScriptFileWatcher.Start(ScriptRoot.generic_wstring(), true))
    {
        return true;
    }

    UE_LOG("[LuaHotReload] Failed to start script file watcher: %s\n",
        FPaths::ToUtf8(ScriptRoot.generic_wstring()).c_str());
    return false;
}

std::shared_ptr<FLuaScriptInstance> FLuaScriptSubsystem::CreateScriptInstance(
    AActor* Owner,
    UScriptComponent* OwnerComponent,
    const FString& ScriptPath)
{
    // One component owns one environment; globals are isolated between actors.
    auto Instance = std::make_shared<FLuaScriptInstance>(Lua);

    Instance->Owner = Owner;
    Instance->OwnerComponent = OwnerComponent;
    Instance->ScriptPath = FPaths::Normalize(ScriptPath);

    // Env was created in FLuaScriptInstance constructor; only bind globals here.
    PopulateInstanceEnvironment(*Instance, Owner, OwnerComponent);

    ScriptInstances.push_back(Instance);
    LoadScript(Instance);
    return Instance;
}

bool FLuaScriptSubsystem::LoadScript(std::shared_ptr<FLuaScriptInstance> Instance)
{
    if (!Instance)
    {
        return false;
    }

    Instance->bLoaded = false;
    Instance->CallbackUsesOwnerArgument.clear();

    // Resolve to absolute path to avoid working-directory sensitivity.
    const FWString ResolvedScriptPathWide = ResolveScriptPathWide(Instance->ScriptPath);
    const FString ResolvedScriptPathUtf8 = FPaths::ToUtf8(ResolvedScriptPathWide);
    Instance->ResolvedScriptPathKey = MakeScriptPathKey(ResolvedScriptPathWide);

    FString ScriptSource;
    if (!ReadFileToStringByWidePath(ResolvedScriptPathWide, ScriptSource))
    {
        UE_LOG("[Lua Load Error] Failed to open script file: %s\n", ResolvedScriptPathUtf8.c_str());
        return false;
    }

    StripUtf8Bom(ScriptSource);

    sol::load_result LoadedScript = Lua.load(ScriptSource, ResolvedScriptPathUtf8);

    if (!LoadedScript.valid())
    {
        sol::error Error = LoadedScript;
        UE_LOG("[Lua Load Error] %s : %s\n", ResolvedScriptPathUtf8.c_str(), Error.what());
        return false;
    }

    sol::protected_function ScriptFunc = LoadedScript;
    sol::set_environment(Instance->Env, ScriptFunc);

    sol::protected_function_result Result = ScriptFunc();
    if (!Result.valid())
    {
        sol::error Error = Result;
        UE_LOG("[Lua Runtime Error] %s : %s\n", ResolvedScriptPathUtf8.c_str(), Error.what());
        return false;
    }

    Instance->bLoaded = true;
    return true;
}

bool FLuaScriptSubsystem::ReloadScript(std::shared_ptr<FLuaScriptInstance> Instance)
{
    if (!Instance)
    {
        return false;
    }

    AActor* Owner = Instance->Owner;
    UScriptComponent* OwnerComponent = Instance->OwnerComponent;
    FString ScriptPath = Instance->ScriptPath;

    // Recreate the environment completely so stale Lua globals/upvalues do not survive reload.
    RecreateAndPopulateInstanceEnvironment(*Instance, Lua, Owner, OwnerComponent);

    Instance->ScriptPath = ScriptPath;
    Instance->ResolvedScriptPathKey.clear();
    Instance->CallbackUsesOwnerArgument.clear();
    Instance->bLoaded = false;
    return LoadScript(Instance);
}

void FLuaScriptSubsystem::DestroyScriptInstance(const std::shared_ptr<FLuaScriptInstance>& Instance)
{
    auto It = std::remove(ScriptInstances.begin(), ScriptInstances.end(), Instance);
    ScriptInstances.erase(It, ScriptInstances.end());
}

void FLuaScriptSubsystem::ReloadAllScripts()
{
    for (const std::shared_ptr<FLuaScriptInstance>& Instance : ScriptInstances)
    {
        if (!Instance || !Instance->OwnerComponent || !UObject::IsValid(Instance->OwnerComponent))
        {
            continue;
        }

        Instance->OwnerComponent->ReloadScript();
    }
}

void FLuaScriptSubsystem::ReloadChangedScripts(const TArray<FWString>& ChangedScriptPaths)
{
    if (ChangedScriptPaths.empty())
    {
        return;
    }

    std::set<FWString> DirtyScriptKeys;
    for (const FWString& ChangedPath : ChangedScriptPaths)
    {
        DirtyScriptKeys.insert(MakeScriptPathKey(ChangedPath));
    }

    TArray<std::pair<UScriptComponent*, FWString>> ComponentsToReload;
    for (const std::shared_ptr<FLuaScriptInstance>& Instance : ScriptInstances)
    {
        if (!Instance || !Instance->OwnerComponent || !UObject::IsValid(Instance->OwnerComponent))
        {
            continue;
        }

        const FWString ScriptPathKey = !Instance->ResolvedScriptPathKey.empty()
            ? Instance->ResolvedScriptPathKey
            : MakeScriptPathKey(Instance->ScriptPath);
        if (DirtyScriptKeys.find(ScriptPathKey) == DirtyScriptKeys.end())
        {
            continue;
        }

        const bool bAlreadyQueued = std::any_of(
            ComponentsToReload.begin(),
            ComponentsToReload.end(),
            [OwnerComponent = Instance->OwnerComponent](const auto& Entry)
            {
                return Entry.first == OwnerComponent;
            });
        if (!bAlreadyQueued)
        {
            ComponentsToReload.emplace_back(Instance->OwnerComponent, ScriptPathKey);
        }
    }

    TMap<FWString, int32> ReloadCountByScriptKey;
    for (const auto& ReloadTarget : ComponentsToReload)
    {
        if (ReloadTarget.first->ReloadScript())
        {
            ++ReloadCountByScriptKey[ReloadTarget.second];
        }
    }

    for (const auto& ReloadEntry : ReloadCountByScriptKey)
    {
        UE_LOG("[LuaHotReload] Reloaded %d actor instance(s) for '%s'.\n",
            ReloadEntry.second,
            FPaths::ToRelativeString(ReloadEntry.first).c_str());
    }
}

const TArray<FString>& FLuaScriptSubsystem::GetAvailableScriptPaths(bool bForceRefresh)
{
    if (bForceRefresh || !bScriptPathCacheInitialized)
    {
        RebuildScriptPathCache();
    }

    return AvailableScriptPaths;
}

void FLuaScriptSubsystem::RefreshAvailableScriptPaths()
{
    RebuildScriptPathCache();
}

void FLuaScriptSubsystem::RebuildScriptPathCache()
{
    AvailableScriptPaths.clear();
    bScriptPathCacheInitialized = true;

    const std::filesystem::path ScriptRoot = std::filesystem::path(FPaths::AssetDirectoryPath()) / L"Scripts";
    std::error_code Ec;
    if (!std::filesystem::exists(ScriptRoot, Ec))
    {
        return;
    }

    // Editor dropdown cache: keep normalized relative paths under Asset/Scripts.
    for (auto It = std::filesystem::recursive_directory_iterator(ScriptRoot, Ec);
        !Ec && It != std::filesystem::recursive_directory_iterator();
        It.increment(Ec))
    {
        if (!It->is_regular_file(Ec))
        {
            continue;
        }

        FWString Extension = It->path().extension().wstring();
        std::transform(Extension.begin(), Extension.end(), Extension.begin(), towlower);
        if (Extension != L".lua")
        {
            continue;
        }

        const FString RelativePath = FPaths::ToRelativeString(It->path().generic_wstring());
        AvailableScriptPaths.push_back(FPaths::Normalize(RelativePath));
    }

    std::sort(AvailableScriptPaths.begin(), AvailableScriptPaths.end());
    AvailableScriptPaths.erase(
        std::unique(AvailableScriptPaths.begin(), AvailableScriptPaths.end()),
        AvailableScriptPaths.end());
}

bool FLuaScriptSubsystem::HasFunction(std::shared_ptr<FLuaScriptInstance> Instance, const FString& FunctionName) const
{
    if (!CanInvoke(Instance))
    {
        return false;
    }

    sol::object FuncObject = Instance->Env[FunctionName];
    return FuncObject.valid() && FuncObject.get_type() == sol::type::function;
}
