#pragma once

#include <cmath>
#include "Core/Containers/Array.h"
#include "Math/Vector.h"
#include "Serialization/Archive.h"

struct FRawAnimSequenceTrack
{
    TArray<FVector> PosKeys;
    TArray<FQuat> RotKeys;
    TArray<FVector> ScaleKeys;

    // Serializer.
    friend FArchive& operator<<(FArchive& Ar, FRawAnimSequenceTrack& T)
    {
        Ar << T.PosKeys;
        Ar << T.RotKeys;
        Ar << T.ScaleKeys;

        return Ar;
    }

    bool ContainsNaN() const
    {
        for (const FVector& Key : PosKeys)
        {
            if (std::isnan(Key.X) || std::isnan(Key.Y) || std::isnan(Key.Z))
                return true;
        }

        for (const FQuat& Key : RotKeys)
        {
            if (std::isnan(Key.X) || std::isnan(Key.Y) || std::isnan(Key.Z) || std::isnan(Key.W))
                return true;
        }

        for (const FVector& Key : ScaleKeys)
        {
            if (std::isnan(Key.X) || std::isnan(Key.Y) || std::isnan(Key.Z))
                return true;
        }

        return false;
    }

    bool Serialize(FArchive& Ar)
    {
        Ar << *this;

        return true;
    }

    static const uint32 SingleKeySize = sizeof(FVector) + sizeof(FQuat) + sizeof(FVector);
};