#include "ShaderManager.h"

#include "Render/FontBatcher.h"
#include "Render/SubUVBatcher.h"
#include "Render/Resource/RenderResources.h"
#include "Core/Paths.h"
#include "Editor/UI/EditorConsoleWidget.h"
#include "VertexLayouts.h"
#include "Render/Resource/Shader.h"
#include "Render/Common/ViewTypes.h"

#include <algorithm>
#include <cctype>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>

namespace
{
    std::string TrimCopy(const std::string& Value)
    {
        size_t Start = 0;
        while (Start < Value.size() && std::isspace(static_cast<unsigned char>(Value[Start])) != 0)
        {
            ++Start;
        }

        size_t End = Value.size();
        while (End > Start && std::isspace(static_cast<unsigned char>(Value[End - 1])) != 0)
        {
            --End;
        }

        return Value.substr(Start, End - Start);
    }
} // namespace

FShader* FShaderManager::GetShader(const FShaderKey& Key)
{
    auto It = ShaderMap.find(Key);
    if (It != ShaderMap.end())
        return It->second.get();
    if (CachedDevice != nullptr)
    {
        return CreateShader(CachedDevice, Key);
    }

    return nullptr;
}

void FShaderManager::PreloadShaders(ID3D11Device* Device)
{
    CachedDevice = Device;
    for (int view = 0; view < static_cast<int>(EViewMode::Count); ++view)
    {
        for (int bNormal = 0; bNormal <= 1; ++bNormal)
        {
            for (int type = 0; type < static_cast<int>(EOpaqueType::Count); ++type)
            {
				for (int bLightCull = 0; bLightCull <= 1; ++bLightCull)
				{
                                    FShaderKey Key;
                                    Key.SetViewMode(view);
                                    Key.SetNormalMap(bNormal != 0);
                                    Key.SetOpaqueType(type);
                                    Key.SetLightCullMode(bLightCull);
                                    if (!ShaderMap.contains(Key))
                                    {
                                        CreateShader(Device, Key);
                                    }
				}
            }
        }
    }
}

void FShaderManager::ProcessHotReloads(ID3D11Device* Device, const std::vector<std::wstring>& ChangedFiles,
                                       FRenderResources& Resources, FFontBatcher& FontBatcher,
                                       FSubUVBatcher& SubUVBatcher)
{
    const auto Now = std::chrono::steady_clock::now();

    for (const std::wstring& FilePath : ChangedFiles)
    {
        const size_t DotIndex = FilePath.find_last_of(L'.');
        if (DotIndex == std::wstring::npos)
        {
            continue;
        }

        std::wstring Extension = FilePath.substr(DotIndex);
        std::transform(Extension.begin(), Extension.end(), Extension.begin(),
                       [](wchar_t Character) { return static_cast<wchar_t>(towlower(Character)); });
        if (Extension != L".hlsl")
        {
            continue;
        }

        PendingShaderFiles[NormalizePath(FilePath)] = Now;
    }

    if (PendingShaderFiles.empty())
    {
        return;
    }

    std::vector<std::wstring> ReadyFiles;
    ReadyFiles.reserve(PendingShaderFiles.size());

    for (const auto& Entry : PendingShaderFiles)
    {
        const auto Elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(Now - Entry.second);
        if (Elapsed.count() >= ShaderReloadDebounceMs)
        {
            ReadyFiles.push_back(Entry.first);
        }
    }

    if (ReadyFiles.empty())
    {
        return;
    }

    std::set<std::wstring> ReadyDirtyFiles;
    for (const std::wstring& ReadyFile : ReadyFiles)
    {
        PendingShaderFiles.erase(ReadyFile);
        ReadyDirtyFiles.insert(ReadyFile);
    }

    ReloadShaders(Device, ReadyDirtyFiles, Resources, FontBatcher, SubUVBatcher);
}

void FShaderManager::ReloadShaders(ID3D11Device* Device, const std::set<std::wstring>& DirtyFiles,
                                   FRenderResources& Resources, FFontBatcher& FontBatcher, FSubUVBatcher& SubUVBatcher)
{
    if (Device == nullptr || DirtyFiles.empty())
    {
        return;
    }

    std::vector<FShader*> ReloadableShaders;
    CollectReloadableShaders(Resources, FontBatcher, SubUVBatcher, ReloadableShaders);
    std::unordered_set<FShader*>                            UniqueReloadableShaders;
    std::unordered_map<std::wstring, std::vector<FShader*>> AffectedShadersBySource;

    std::unordered_map<std::wstring, std::unordered_set<std::wstring>> DependencyCache;
    struct FReloadSourceStats
    {
        size_t SuccessCount = 0;
        size_t FailureCount = 0;
    };
    std::unordered_map<std::wstring, FReloadSourceStats> ReloadStatsBySource;
    std::unordered_set<std::string>                      LoggedFailureMessages;
    size_t                                               AffectedShaderCount = 0;

    for (FShader* Shader : ReloadableShaders)
    {
        if (Shader == nullptr || !UniqueReloadableShaders.insert(Shader).second || !Shader->IsReloadable())
        {
            continue;
        }

        const std::wstring               ShaderSourcePath = NormalizePath(Shader->GetFilePath());
        std::unordered_set<std::wstring> Dependencies;
        CollectShaderDependencies(ShaderSourcePath, Dependencies, DependencyCache);

        bool bAffected = DirtyFiles.contains(ShaderSourcePath);
        if (!bAffected)
        {
            for (const std::wstring& DirtyFile : DirtyFiles)
            {
                if (Dependencies.contains(DirtyFile))
                {
                    bAffected = true;
                    break;
                }
            }
        }

        if (!bAffected)
        {
            continue;
        }

        ++AffectedShaderCount;
        AffectedShadersBySource[ShaderSourcePath].push_back(Shader);
    }

    if (AffectedShaderCount == 0)
    {
        UE_LOG("[ShaderHotReload] Detected %zu dirty shader file(s), but no dependent shaders were reloaded.",
               DirtyFiles.size());
        return;
    }

    for (auto& Entry : AffectedShadersBySource)
    {
        const std::wstring&          ShaderSourcePath = Entry.first;
        const std::vector<FShader*>& ShadersForSource = Entry.second;

        std::vector<FShaderCompiledState> PendingCompiledStates;
        PendingCompiledStates.resize(ShadersForSource.size());

        bool bSourceFailed = false;
        for (size_t ShaderIndex = 0; ShaderIndex < ShadersForSource.size(); ++ShaderIndex)
        {
            std::string FailureMessage;
            if (!ShadersForSource[ShaderIndex]->PrepareReload(Device, PendingCompiledStates[ShaderIndex],
                                                              &FailureMessage, false))
            {
                bSourceFailed = true;
                ReloadStatsBySource[ShaderSourcePath].FailureCount = ShadersForSource.size();

                const std::string FailureLogKey = FPaths::ToUtf8(ShaderSourcePath) + "|" + FailureMessage;
                if (LoggedFailureMessages.insert(FailureLogKey).second)
                {
                    UE_LOG("[Shader] %s - %s", FPaths::ToUtf8(ShaderSourcePath).c_str(), FailureMessage.c_str());
                }
                break;
            }
        }

        if (bSourceFailed)
        {
            continue;
        }

        for (size_t ShaderIndex = 0; ShaderIndex < ShadersForSource.size(); ++ShaderIndex)
        {
            ShadersForSource[ShaderIndex]->CommitReload(std::move(PendingCompiledStates[ShaderIndex]));
        }

        ReloadStatsBySource[ShaderSourcePath].SuccessCount = ShadersForSource.size();
    }

    size_t FullyReloadedSourceCount = 0;
    size_t FailedSourceCount = 0;

    for (const auto& Entry : ReloadStatsBySource)
    {
        const FReloadSourceStats& Stats = Entry.second;
        if (Stats.SuccessCount > 0 && Stats.FailureCount == 0)
        {
            ++FullyReloadedSourceCount;
        }
        else if (Stats.FailureCount > 0)
        {
            ++FailedSourceCount;
        }
    }

    UE_LOG("[ShaderHotReload] Detected %zu dirty shader file(s). %zu shader variant(s) affected, %zu source(s) "
           "reloaded, %zu source(s) failed.",
           DirtyFiles.size(), AffectedShaderCount, FullyReloadedSourceCount, FailedSourceCount);

    for (const auto& Entry : ReloadStatsBySource)
    {
        const FReloadSourceStats& Stats = Entry.second;
        const std::string         SourcePath = FPaths::ToUtf8(Entry.first);
        if (Stats.SuccessCount > 0 && Stats.FailureCount == 0)
        {
            UE_LOG("[ShaderHotReload] %s reloaded (%zu variant(s))", SourcePath.c_str(), Stats.SuccessCount);
        }
        else if (Stats.FailureCount > 0)
        {
            UE_LOG("[ShaderHotReload] %s reload failed (%zu variant(s))", SourcePath.c_str(), Stats.FailureCount);
        }
    }
}

void FShaderManager::CollectReloadableShaders(FRenderResources& Resources, FFontBatcher& FontBatcher,
                                              FSubUVBatcher& SubUVBatcher, std::vector<FShader*>& OutShaders)
{
    OutShaders.push_back(&Resources.PrimitiveShader);
    OutShaders.push_back(&Resources.GizmoShader);
    OutShaders.push_back(&Resources.EditorShader);
    OutShaders.push_back(&Resources.GridShader);
    OutShaders.push_back(&Resources.AxisShader);
    OutShaders.push_back(&Resources.SelectionMaskShader);
    OutShaders.push_back(&Resources.OutlineShader);
    OutShaders.push_back(&Resources.UberLitShader);
    OutShaders.push_back(&Resources.DecalShader);
    OutShaders.push_back(&Resources.FireBallShader);
    OutShaders.push_back(&Resources.DepthVisualizerShader);
    OutShaders.push_back(&Resources.FogShader);
    OutShaders.push_back(&Resources.LightHitmapOverlayShader);
    OutShaders.push_back(&Resources.FXAAShader);
    OutShaders.push_back(FontBatcher.GetShader());
    OutShaders.push_back(SubUVBatcher.GetShader());

    for (auto& Entry : ShaderMap)
    {
        if (Entry.second)
        {
            OutShaders.push_back(Entry.second.get());
        }
    }
}

void FShaderManager::CollectShaderDependencies(
    const std::wstring& ShaderFilePath, std::unordered_set<std::wstring>& OutDependencies,
    std::unordered_map<std::wstring, std::unordered_set<std::wstring>>& Cache)
{
    std::unordered_set<std::wstring> ActiveStack;

    const auto CollectRecursive = [this, &Cache](const auto& Self, const std::wstring& CurrentShaderFilePath,
                                                 std::unordered_set<std::wstring>& CurrentDependencies,
                                                 std::unordered_set<std::wstring>& CurrentActiveStack) -> void
    {
        const std::wstring NormalizedShaderPath = NormalizePath(CurrentShaderFilePath);
        auto               CachedIt = Cache.find(NormalizedShaderPath);
        if (CachedIt != Cache.end())
        {
            CurrentDependencies.insert(CachedIt->second.begin(), CachedIt->second.end());
            return;
        }

        if (!CurrentActiveStack.insert(NormalizedShaderPath).second)
        {
            return;
        }

        std::unordered_set<std::wstring> LocalDependencies;

        std::ifstream File{std::filesystem::path(CurrentShaderFilePath)};
        if (File.is_open())
        {
            const std::filesystem::path ParentDirectory = std::filesystem::path(CurrentShaderFilePath).parent_path();

            std::string Line;
            while (std::getline(File, Line))
            {
                const std::string TrimmedLine = TrimCopy(Line);
                if (!TrimmedLine.starts_with("#include"))
                {
                    continue;
                }

                const size_t FirstQuote = TrimmedLine.find('"');
                const size_t LastQuote = TrimmedLine.find_last_of('"');
                if (FirstQuote == std::string::npos || LastQuote == std::string::npos || FirstQuote == LastQuote)
                {
                    continue;
                }

                const std::string  IncludePathUtf8 = TrimmedLine.substr(FirstQuote + 1, LastQuote - FirstQuote - 1);
                const std::wstring IncludePathWide = FPaths::ToWide(IncludePathUtf8);
                const std::wstring IncludeFullPath = NormalizePath(
                    (ParentDirectory / std::filesystem::path(IncludePathWide)).lexically_normal().generic_wstring());

                if (LocalDependencies.insert(IncludeFullPath).second)
                {
                    Self(Self, IncludeFullPath, LocalDependencies, CurrentActiveStack);
                }
            }
        }

        CurrentActiveStack.erase(NormalizedShaderPath);
        Cache.emplace(NormalizedShaderPath, LocalDependencies);
        CurrentDependencies.insert(LocalDependencies.begin(), LocalDependencies.end());
    };

    CollectRecursive(CollectRecursive, ShaderFilePath, OutDependencies, ActiveStack);
}

std::wstring FShaderManager::NormalizePath(const std::wstring& InPath) const
{
    std::wstring Result = std::filesystem::path(InPath).lexically_normal().generic_wstring();
    std::transform(Result.begin(), Result.end(), Result.begin(),
                   [](wchar_t Character) { return static_cast<wchar_t>(towlower(Character)); });
    return Result;
}

FShader* FShaderManager::CreateShader(ID3D11Device* Device, const FShaderKey& Key)
{
    std::unique_ptr<FShader> Shader = std::make_unique<FShader>();

    static const char* ViewModeTable[] = {"0", "1", "2",  "3",  "4",  "5",  "6",  "7",
                                          "8", "9", "10", "11", "12", "13", "14", "15"};

    static const char* OpaqueTypeTable[] = {"0", "1"};

    uint32 ViewModeIndex = Key.Bits & VIEWMODE_MASK;
    uint32 OpaqueTypeIndex = (Key.Bits & OPAQUE_TYPE_MASK) >> OPAQUE_TYPE_SHIFT;
    bool   bNormalMap = (Key.Bits & NORMALMAP_BIT);
    bool   bLightCullMode = (Key.Bits & LIGHTCULLING_BIT);

    D3D_SHADER_MACRO Defines[] = {{"VIEW_MODE", ViewModeTable[ViewModeIndex]},
                                  {"USE_NORMALMAP", bNormalMap ? "1" : "0"},
                                  {"OPAQUETYPE", OpaqueTypeTable[OpaqueTypeIndex]},
                                  {"LIGHT_CULLING_FLAG", bLightCullMode ? "1" : "0"},
                                  {nullptr, nullptr}};

    if (!Shader->Create(Device, L"Shaders/UberLit.hlsl", "VS", "PS", VertexLayouts::NormalVertexInputLayout,
                        ARRAYSIZE(VertexLayouts::NormalVertexInputLayout), Defines))
    {
        return nullptr;
    }

    FShader* Result = Shader.get();
    ShaderMap.emplace(Key, std::move(Shader));

    return Result;
}
