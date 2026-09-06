#pragma once

#include "Core/CoreTypes.h"
#include "Reflection/Field.h"

struct FEnumValue
{
    const char* Name = nullptr;
    int64 Value = 0;
};

class UEnum : public UField
{
public:
    UEnum() = default;
    UEnum(
        const char* InName,
        const char* const* InNames,
        const FEnumValue* InValues,
        uint32 InValueCount)
        : UField(InName)
        , Names(InNames)
        , Values(InValues)
        , ValueCount(InValueCount)
    {
    }

    const char* const* GetNames() const { return Names; }
    const FEnumValue* GetValues() const { return Values; }
    uint32 GetValueCount() const { return ValueCount; }

private:
    const char* const* Names = nullptr;
    const FEnumValue* Values = nullptr;
    uint32 ValueCount = 0;
};
