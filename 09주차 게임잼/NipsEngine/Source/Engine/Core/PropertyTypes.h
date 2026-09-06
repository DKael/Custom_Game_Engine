#pragma once

#include "Core/CoreTypes.h"
#include "Math/Color.h"
#include "Math/Vector.h"
#include "Math/Vector4.h"

// Property type used by the editor's automatic widget mapping.
enum class EPropertyType : uint8
{
    Bool,
    Int,
    Float,
    Vec3,
    Vec4,
    String,
    Name,              // FName-based identifier (resource keys, etc.)
    SceneComponentRef, // Address of USceneComponent* variable
    Vec3Array,         // TArray<FVector>* variable-size vector array
    Enum,
    Color,
};

// Descriptor for an editable property exposed by a component.
struct FPropertyDescriptor
{
    const char*   Name;
    EPropertyType Type;
    void*         ValuePtr;

    // Float range hint (used by DragFloat-like widgets).
    float Min   = 0.0f;
    float Max   = 0.0f;
    float Speed = 0.1f;

    // Enum metadata.
    const char** EnumNames = nullptr;
    uint32       EnumCount = 0;
};

constexpr uint32 PropertyNameIdConstexpr(const char* Name, uint32 Hash = 2166136261u)
{
    return (*Name == '\0')
        ? Hash
        : PropertyNameIdConstexpr(Name + 1, (Hash ^ static_cast<uint8>(*Name)) * 16777619u);
}

inline uint32 PropertyNameId(const char* Name)
{
    if (Name == nullptr)
    {
        return PropertyNameIdConstexpr("");
    }

    uint32 Hash = 2166136261u;
    while (*Name != '\0')
    {
        Hash ^= static_cast<uint8>(*Name);
        Hash *= 16777619u;
        ++Name;
    }
    return Hash;
}

/**
 * Returns the storage size of the given property type.
 * Returning 0 means the property requires special-case handling.
 */
inline SIZE_T GetPropertySize(EPropertyType Type)
{
    switch (Type)
    {
    case EPropertyType::Bool:   return sizeof(bool);
    case EPropertyType::Int:    return sizeof(int32);
    case EPropertyType::Enum:   return sizeof(int32);
    case EPropertyType::Float:  return sizeof(float);
    case EPropertyType::Vec3:   return sizeof(FVector);
    case EPropertyType::Color:  return sizeof(FColor);
    case EPropertyType::Vec4:   return sizeof(FVector4);
    case EPropertyType::String: return 0; // ValuePtr-based custom handling
    case EPropertyType::Name:   return 0; // ValuePtr-based custom handling
    case EPropertyType::SceneComponentRef: return 0; // Handled by duplication/resolve path
    default: return 0;
    }
}
