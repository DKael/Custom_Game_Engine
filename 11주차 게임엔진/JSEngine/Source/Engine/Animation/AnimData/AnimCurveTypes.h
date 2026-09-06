#pragma once

#include "Core/CoreMinimal.h"

#include <algorithm>
#include <cctype>

enum class EAnimCurveType : uint8
{
    Attribute = 0,
    Material,
    MorphTarget,
    Unknown,
};

enum class EAnimCurveSourceKind : uint8
{
    ImportedCustomAttribute = 0,
    ImportedMaterial,
    ImportedMorphTarget,
    Unknown,
};

inline FString AnimationCurveTypeToString(EAnimCurveType CurveType)
{
    switch (CurveType)
    {
    case EAnimCurveType::Attribute:
        return "Attribute";
    case EAnimCurveType::Material:
        return "Material";
    case EAnimCurveType::MorphTarget:
        return "MorphTarget";
    default:
        return "Unknown";
    }
}

inline EAnimCurveType AnimationCurveTypeFromString(FString Value)
{
    std::transform(
        Value.begin(),
        Value.end(),
        Value.begin(),
        [](unsigned char Character)
        {
            return static_cast<char>(std::tolower(Character));
        });

    if (Value == "attribute")
    {
        return EAnimCurveType::Attribute;
    }
    if (Value == "material")
    {
        return EAnimCurveType::Material;
    }
    if (Value == "morphtarget")
    {
        return EAnimCurveType::MorphTarget;
    }

    return EAnimCurveType::Unknown;
}

inline FString AnimationCurveSourceKindToString(EAnimCurveSourceKind SourceKind)
{
    switch (SourceKind)
    {
    case EAnimCurveSourceKind::ImportedCustomAttribute:
        return "ImportedCustomAttribute";
    case EAnimCurveSourceKind::ImportedMaterial:
        return "ImportedMaterial";
    case EAnimCurveSourceKind::ImportedMorphTarget:
        return "ImportedMorphTarget";
    default:
        return "Unknown";
    }
}

inline EAnimCurveSourceKind AnimationCurveSourceKindFromString(FString Value)
{
    std::transform(
        Value.begin(),
        Value.end(),
        Value.begin(),
        [](unsigned char Character)
        {
            return static_cast<char>(std::tolower(Character));
        });

    if (Value == "importedcustomattribute")
    {
        return EAnimCurveSourceKind::ImportedCustomAttribute;
    }
    if (Value == "importedmaterial")
    {
        return EAnimCurveSourceKind::ImportedMaterial;
    }
    if (Value == "importedmorphtarget")
    {
        return EAnimCurveSourceKind::ImportedMorphTarget;
    }

    return EAnimCurveSourceKind::Unknown;
}
