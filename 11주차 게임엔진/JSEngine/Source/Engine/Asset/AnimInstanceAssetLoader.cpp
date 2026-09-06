#include "Asset/AnimInstanceAssetLoader.h"

#include "Animation/AnimInstanceAsset.h"
#include "Core/Logging/Log.h"
#include "Core/Paths.h"
#include "Object/Object.h"
#include "SimpleJSON/json.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>

namespace
{
    FString ToLower(FString Value)
    {
        std::transform(Value.begin(), Value.end(), Value.begin(), [](unsigned char Ch) { return static_cast<char>(std::tolower(Ch)); });
        return Value;
    }

    bool IsAnimInstanceAssetPath(const FString& Path)
    {
        const std::filesystem::path FsPath(FPaths::ToWide(FPaths::Normalize(Path)));
        return ToLower(FPaths::ToUtf8(FsPath.extension().wstring())) == ".animinstance";
    }

    FString ConditionTypeToString(EAnimTransitionConditionType Type)
    {
        switch (Type)
        {
        case EAnimTransitionConditionType::BoolEquals: return "BoolEquals";
        case EAnimTransitionConditionType::FloatGreater: return "FloatGreater";
        case EAnimTransitionConditionType::FloatLessEqual: return "FloatLessEqual";
        case EAnimTransitionConditionType::IntEquals: return "IntEquals";
        case EAnimTransitionConditionType::Trigger: return "Trigger";
        case EAnimTransitionConditionType::StateFinished: return "StateFinished";
        default: return "BoolEquals";
        }
    }

    EAnimTransitionConditionType ConditionTypeFromString(const FString& Text)
    {
        if (Text == "FloatGreater") return EAnimTransitionConditionType::FloatGreater;
        if (Text == "FloatLessEqual") return EAnimTransitionConditionType::FloatLessEqual;
        if (Text == "IntEquals") return EAnimTransitionConditionType::IntEquals;
        if (Text == "Trigger") return EAnimTransitionConditionType::Trigger;
        if (Text == "StateFinished") return EAnimTransitionConditionType::StateFinished;
        return EAnimTransitionConditionType::BoolEquals;
    }

    FString ParameterTypeToString(EAnimStateParameterType Type)
    {
        switch (Type)
        {
        case EAnimStateParameterType::Bool: return "Bool";
        case EAnimStateParameterType::Int: return "Int";
        case EAnimStateParameterType::Trigger: return "Trigger";
        case EAnimStateParameterType::Float:
        default: return "Float";
        }
    }

    EAnimStateParameterType ParameterTypeFromString(const FString& Text)
    {
        if (Text == "Bool") return EAnimStateParameterType::Bool;
        if (Text == "Int") return EAnimStateParameterType::Int;
        if (Text == "Trigger") return EAnimStateParameterType::Trigger;
        return EAnimStateParameterType::Float;
    }
}

UAnimInstanceAsset* FAnimInstanceAssetLoader::Load(const FString& Path) const
{
    const FString NormalizedPath = FPaths::Normalize(Path);
    if (NormalizedPath.empty() || !IsAnimInstanceAssetPath(NormalizedPath))
    {
        return nullptr;
    }

    std::ifstream File(FPaths::ToWide(NormalizedPath));
    if (!File.is_open())
    {
        UE_LOG_ERROR("[AnimInstanceAssetLoader] Failed to open anim instance asset: %s", NormalizedPath.c_str());
        return nullptr;
    }

    FString Source((std::istreambuf_iterator<char>(File)), std::istreambuf_iterator<char>());
    json::JSON Root = json::JSON::Load(Source);
    if (Root.JSONType() != json::JSON::Class::Object)
    {
        UE_LOG_ERROR("[AnimInstanceAssetLoader] Invalid anim instance asset json: %s", NormalizedPath.c_str());
        return nullptr;
    }

    UAnimInstanceAsset* Asset = UObjectManager::Get().CreateObject<UAnimInstanceAsset>();
    Asset->SetAssetPath(NormalizedPath);
    Asset->EntryState = FName(Root["EntryState"].ToString());

    json::JSON Params = Root["Parameters"];
    if (Params.JSONType() == json::JSON::Class::Array)
    {
        for (int32 Index = 0; Index < static_cast<int32>(Params.length()); ++Index)
        {
            json::JSON Item = Params[Index];
            FAnimInstanceParameterAssetData Param;
            Param.Name = FName(Item["Name"].ToString());
            Param.Type = ParameterTypeFromString(Item["Type"].ToString());
            Param.BoolDefault = Item["BoolDefault"].ToBool();
            Param.FloatDefault = static_cast<float>(Item["FloatDefault"].ToFloat());
            Param.IntDefault = Item["IntDefault"].ToInt();
            Asset->Parameters.push_back(Param);
        }
    }

    json::JSON States = Root["States"];
    if (States.JSONType() == json::JSON::Class::Array)
    {
        for (int32 Index = 0; Index < static_cast<int32>(States.length()); ++Index)
        {
            json::JSON Item = States[Index];
            FAnimInstanceStateAssetData State;
            State.Name = FName(Item["Name"].ToString());
            State.AnimSequencePath = Item["AnimSequencePath"].ToString();
            State.bLoop = Item["Loop"].ToBool();
            State.PlayRate = static_cast<float>(Item["PlayRate"].ToFloat());
            State.EditorNodePosition.X = static_cast<float>(Item["EditorX"].ToFloat());
            State.EditorNodePosition.Y = static_cast<float>(Item["EditorY"].ToFloat());
            Asset->States.push_back(State);
        }
    }

    json::JSON Transitions = Root["Transitions"];
    if (Transitions.JSONType() == json::JSON::Class::Array)
    {
        for (int32 Index = 0; Index < static_cast<int32>(Transitions.length()); ++Index)
        {
            json::JSON Item = Transitions[Index];
            FAnimInstanceTransitionAssetData Transition;
            Transition.FromState = FName(Item["FromState"].ToString());
            Transition.ToState = FName(Item["ToState"].ToString());
            Transition.ParameterName = FName(Item["ParameterName"].ToString());
            Transition.ConditionType = ConditionTypeFromString(Item["ConditionType"].ToString());
            Transition.ExpectedBool = Item["ExpectedBool"].ToBool();
            Transition.CompareFloat = static_cast<float>(Item["CompareFloat"].ToFloat());
            Transition.ExpectedInt = Item["ExpectedInt"].ToInt();
            Transition.BlendSpeed = static_cast<float>(Item["BlendSpeed"].ToFloat());
            Transition.Priority = Item["Priority"].ToInt();
            Asset->Transitions.push_back(Transition);
        }
    }

    return Asset;
}

bool FAnimInstanceAssetLoader::Save(const FString& Path, const UAnimInstanceAsset* Asset) const
{
    if (!Asset)
    {
        return false;
    }

    const FString NormalizedPath = FPaths::Normalize(Path);
    if (NormalizedPath.empty() || !IsAnimInstanceAssetPath(NormalizedPath))
    {
        return false;
    }

    json::JSON Root = json::JSON::Make(json::JSON::Class::Object);
    Root["Type"] = "UAnimInstanceAsset";
    Root["EntryState"] = Asset->EntryState.ToString();

    json::JSON Params = json::JSON::Make(json::JSON::Class::Array);
    for (const FAnimInstanceParameterAssetData& Param : Asset->Parameters)
    {
        json::JSON Item = json::JSON::Make(json::JSON::Class::Object);
        Item["Name"] = Param.Name.ToString();
        Item["Type"] = ParameterTypeToString(Param.Type);
        Item["BoolDefault"] = Param.BoolDefault;
        Item["FloatDefault"] = Param.FloatDefault;
        Item["IntDefault"] = Param.IntDefault;
        Params.append(Item);
    }
    Root["Parameters"] = Params;

    json::JSON States = json::JSON::Make(json::JSON::Class::Array);
    for (const FAnimInstanceStateAssetData& State : Asset->States)
    {
        json::JSON Item = json::JSON::Make(json::JSON::Class::Object);
        Item["Name"] = State.Name.ToString();
        Item["AnimSequencePath"] = State.AnimSequencePath;
        Item["Loop"] = State.bLoop;
        Item["PlayRate"] = State.PlayRate;
        Item["EditorX"] = State.EditorNodePosition.X;
        Item["EditorY"] = State.EditorNodePosition.Y;
        States.append(Item);
    }
    Root["States"] = States;

    json::JSON Transitions = json::JSON::Make(json::JSON::Class::Array);
    for (const FAnimInstanceTransitionAssetData& Transition : Asset->Transitions)
    {
        json::JSON Item = json::JSON::Make(json::JSON::Class::Object);
        Item["FromState"] = Transition.FromState.ToString();
        Item["ToState"] = Transition.ToState.ToString();
        Item["ParameterName"] = Transition.ParameterName.ToString();
        Item["ConditionType"] = ConditionTypeToString(Transition.ConditionType);
        Item["ExpectedBool"] = Transition.ExpectedBool;
        Item["CompareFloat"] = Transition.CompareFloat;
        Item["ExpectedInt"] = Transition.ExpectedInt;
        Item["BlendSpeed"] = Transition.BlendSpeed;
        Item["Priority"] = Transition.Priority;
        Transitions.append(Item);
    }
    Root["Transitions"] = Transitions;

    std::filesystem::path FilePath(FPaths::ToWide(NormalizedPath));
    std::error_code Ec;
    std::filesystem::create_directories(FilePath.parent_path(), Ec);
    std::ofstream OutFile(FilePath);
    if (!OutFile.is_open())
    {
        UE_LOG_ERROR("[AnimInstanceAssetLoader] Failed to open anim instance asset for writing: %s", NormalizedPath.c_str());
        return false;
    }

    OutFile << Root.dump(4);
    return true;
}

bool FAnimInstanceAssetLoader::SupportsExtension(const FString& Extension) const
{
    return Extension == ".animinstance" || Extension == "animinstance" || Extension == ".AnimInstance" || Extension == "AnimInstance";
}

FString FAnimInstanceAssetLoader::GetLoaderName() const
{
    return "FAnimInstanceAssetLoader";
}
