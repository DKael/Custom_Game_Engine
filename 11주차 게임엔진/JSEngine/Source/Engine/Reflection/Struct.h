#pragma once

#include "Core/Containers/Array.h"
#include "Reflection/Field.h"
#include "Reflection/Property.h"

class UObject;
struct FArchive;

class UStruct : public UField
{
public:
    UStruct() = default;
    UStruct(
        const char* InName,
        size_t InSize,
        const FProperty* const* InProperties,
        uint32 InPropertyCount)
        : UField(InName)
        , Size(InSize)
        , Properties(InProperties)
        , PropertyCount(InPropertyCount)
    {
    }

    size_t GetSize() const { return Size; }
    const FProperty* const* GetProperties() const { return Properties; }
    uint32 GetPropertyCount() const { return PropertyCount; }

    void AppendProperties(void* StructValue, TArray<FPropertyDescriptor>& OutProps) const;
    void SerializeProperties(FArchive& Ar, UObject* OwnerObject, void* StructValue) const;

private:
    size_t Size = 0;
    const FProperty* const* Properties = nullptr;
    uint32 PropertyCount = 0;
};

class UScriptStruct : public UStruct
{
public:
    UScriptStruct() = default;
    UScriptStruct(
        const char* InName,
        size_t InSize,
        const FProperty* const* InProperties,
        uint32 InPropertyCount)
        : UStruct(InName, InSize, InProperties, InPropertyCount)
    {
    }
};
