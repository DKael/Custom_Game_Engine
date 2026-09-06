#pragma once

#include "Core/CoreMinimal.h"
#include "Animation/AnimNotifySemanticFieldNames.h"

enum class EAnimNotifyPayloadFieldType : uint8
{
    String = 0,
    Name,
    Float,
    Int,
    Bool,
};

enum class EAnimNotifyPayloadFieldEditorHint : uint8
{
    Default = 0,
    SocketPicker,
    ComponentPicker,
    AssetPicker,
};

enum class EAnimNotifyPayloadAssetKind : uint8
{
    None = 0,
    SoundCue,
    Vfx,
    CameraShake,
};

struct FAnimNotifyPayloadFieldDefinition
{
    FString Key;
    EAnimNotifySemanticFieldId SemanticId = EAnimNotifySemanticFieldId::None;
    TArray<FString> LegacyKeys;
    FString Label;
    EAnimNotifyPayloadFieldType Type = EAnimNotifyPayloadFieldType::String;
    EAnimNotifyPayloadFieldEditorHint EditorHint = EAnimNotifyPayloadFieldEditorHint::Default;
    EAnimNotifyPayloadAssetKind AssetKind = EAnimNotifyPayloadAssetKind::None;
    bool bRequired = false;
    FString DefaultValue;
    FString HelpText;
};

struct FAnimNotifyPayloadSchema
{
    FString NotifyClassName;
    FString SectionLabel;
    FString PayloadExample;
    TArray<FAnimNotifyPayloadFieldDefinition> Fields;
};

const FAnimNotifyPayloadSchema* FindAnimNotifyPayloadSchema(const FString& NotifyClassName);
bool HasAnimNotifyPayloadSchema(const FString& NotifyClassName);
const char* GetAnimNotifyPayloadExample(const FString& NotifyClassName);
TArray<FString> GetAnimNotifyPayloadFieldLookupKeys(const FAnimNotifyPayloadFieldDefinition& Field);
const FAnimNotifyPayloadFieldDefinition* FindAnimNotifyPayloadFieldBySemanticId(
    const FAnimNotifyPayloadSchema& Schema,
    EAnimNotifySemanticFieldId SemanticId);
