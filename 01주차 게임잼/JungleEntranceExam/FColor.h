#pragma once

struct FColor
{
    float r, g, b, a;
    FColor(float _r = 0, float _g = 0, float _b = 0, float _a = 1.0f)
        : r(_r), g(_g), b(_b), a(_a) {
    }

    FColor operator+(float num) const
    {
        return { r + num, g + num, b + num, a };
    }
    FColor operator+(const FColor& c) const
    {
        return { r + c.r, g + c.g, b + c.b, a };
    }
    FColor operator-(float num) const
    {
        return { r - num, g - num, b - num, a };
    }
    FColor operator-(const FColor& c) const
    {
        return { r - c.r, g - c.g, b - c.b, a };
    }
    FColor operator*(float num) const
    {
        return { r * num, g * num, b * num, a };
    }
    FColor operator*(const FColor& c) const
    {
        return { r * c.r, g * c.g, b * c.b, a };
    }

};