#pragma once

#include "Core/CoreMinimal.h"
#include "Object/FName.h"

// The payload parser owns only raw key/value parsing, typed primitive access,
// mutation helpers, and canonical serialization. It intentionally does not know
// semantic field meaning or notify-specific interpretation rules.
class FAnimNotifyPayloadParser
{
public:
    FAnimNotifyPayloadParser() = default;
    explicit FAnimNotifyPayloadParser(const FString& Payload);

    void Parse(const FString& Payload);
    // Exposes the raw parsed map for editor/schema tooling. Runtime and
    // validation code should usually prefer semantic lookup helpers instead of
    // depending on literal payload keys directly.
    const TMap<FString, FString>& GetValues() const { return Values; }

    // Raw string lookup helpers do not apply notify-specific defaults.
    bool HasValue(const FString& Key) const;
    bool HasAnyValue(const TArray<FString>& Keys) const;
    FString GetString(const FString& Key, const FString& DefaultValue = FString()) const;
    FString GetStringAny(const TArray<FString>& Keys, const FString& DefaultValue = FString()) const;

    // Typed accessors only parse primitive representations already present in
    // the payload text. Higher-level semantic defaults belong in payload views
    // or runtime/editor consumers, not in the parser.
    FName GetName(const FString& Key, const FName& DefaultValue = FName::None) const;
    FName GetNameAny(const TArray<FString>& Keys, const FName& DefaultValue = FName::None) const;
    bool TryGetFloat(const FString& Key, float& OutValue) const;
    bool TryGetFloatAny(const TArray<FString>& Keys, float& OutValue) const;
    float GetFloat(const FString& Key, float DefaultValue) const;
    float GetFloatAny(const TArray<FString>& Keys, float DefaultValue) const;
    bool TryGetInt(const FString& Key, int32& OutValue) const;
    bool TryGetIntAny(const TArray<FString>& Keys, int32& OutValue) const;
    int32 GetInt(const FString& Key, int32 DefaultValue) const;
    int32 GetIntAny(const TArray<FString>& Keys, int32 DefaultValue) const;
    bool TryGetBool(const FString& Key, bool& OutValue) const;
    bool TryGetBoolAny(const TArray<FString>& Keys, bool& OutValue) const;
    bool GetBool(const FString& Key, bool DefaultValue) const;
    bool GetBoolAny(const TArray<FString>& Keys, bool DefaultValue) const;

    // Mutation helpers update raw payload entries only. Callers decide which
    // canonical key to write and when to clear legacy aliases.
    void SetString(const FString& Key, const FString& Value);
    void SetName(const FString& Key, const FName& Value);
    void SetFloat(const FString& Key, float Value);
    void SetInt(const FString& Key, int32 Value);
    void SetBool(const FString& Key, bool Value);
    void RemoveValue(const FString& Key);
    void RemoveAny(const TArray<FString>& Keys);

    // Serialization always emits normalized key/value text. The parser preserves
    // original keys only long enough to support tolerant reads while editing.
    FString SerializeCanonical() const;

private:
    static FString NormalizeKey(const FString& Key);
    static FString Trim(const FString& Value);

private:
    TMap<FString, FString> Values;
    TMap<FString, FString> OriginalKeys;
};
