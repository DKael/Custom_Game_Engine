#pragma once

#include "Reflection/Struct.h"

class UClass : public UStruct
{
public:
    UClass() = default;
    UClass(
        const char* InName,
        const UClass* InSuperClass,
        size_t InSize,
        const FProperty* const* InProperties,
        uint32 InPropertyCount)
        : UStruct(InName, InSize, InProperties, InPropertyCount)
        , SuperClass(InSuperClass)
    {
    }

    const UClass* GetSuperClass() const { return SuperClass; }

    bool IsA(const UClass* Other) const
    {
        for (const UClass* Class = this; Class; Class = Class->GetSuperClass())
        {
            if (Class == Other)
            {
                return true;
            }
        }
        return false;
    }

    void AppendProperties(void* ObjectValue, TArray<FPropertyDescriptor>& OutProps) const;
    void SerializeProperties(FArchive& Ar, UObject* ObjectValue) const;

private:
    const UClass* SuperClass = nullptr;
};
