#pragma once

#include "Core/CoreTypes.h"

struct FFrameRate
{
    int32 Numerator = 30;
    int32 Denominator = 1;

    constexpr FFrameRate() = default;

    constexpr FFrameRate(int32 InNumerator, int32 InDenominator = 1)
        : Numerator(InNumerator)
        , Denominator(InDenominator > 0 ? InDenominator : 1)
    {
    }

    double AsDecimal() const
    {
        return Denominator > 0
            ? static_cast<double>(Numerator) / static_cast<double>(Denominator)
            : 0.0;
    }
};
