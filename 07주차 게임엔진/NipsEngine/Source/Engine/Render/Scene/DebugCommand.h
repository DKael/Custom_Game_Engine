#pragma once

#include "Core/CoreMinimal.h"

enum class EDebugShapeType : uint8
{
    Line,
    AABB,
    OBB,
    Sphere,
    Cone
};

struct FDebugLine
{
    FVector  Start;
    FVector  End;
    FVector4 Color;
};

struct FDebugSphere
{
    FVector  Center;
    float    Radius;
    FVector4 Color;
    int32    SegmentCount;
};

struct FDebugCone
{
    FVector  Apex;
    FVector  Direction; // normalize
    float    Height;
    float    Angle; // radians (half-angle)
    FVector4 Color;
    int32    SegmentCount;
};

struct FDebugRenderCommand
{
    EDebugShapeType Type;

    union
    {
        FDebugLine   Line;
        FDebugSphere Sphere;
        FDebugCone   Cone;
    };
};

namespace DebugCmd
{
    inline FDebugRenderCommand MakeLine(const FVector& Start, const FVector& End, const FVector4& Color)
    {
        FDebugRenderCommand Cmd{};
        Cmd.Type = EDebugShapeType::Line;
        Cmd.Line = {Start, End, Color};
        return Cmd;
    }

	inline void MakeArrow(TArray<FDebugRenderCommand>& OutCmds, const FVector& Start, const FVector& Direction, float Length, float HeadLength, const FVector4& Color)
	{
            FVector Dir = Direction.GetSafeNormal();
            FVector End = Start + Dir * Length;

            // 1. 몸통
            OutCmds.push_back(MakeLine(Start, End, Color));

            // 2. basis
            FVector Up = fabs(Dir.Z) < 0.999f ? FVector(0, 0, 1) : FVector(0, 1, 0);
            FVector Right = Up.CrossProduct(Dir).GetSafeNormal();
            FVector Forward = Dir.CrossProduct(Right).GetSafeNormal();

            float   HeadRadius = HeadLength * tanf(15.f);
            FVector Base = End - Dir * HeadLength;

            FVector P1 = Base + Right * HeadRadius;
            FVector P2 = Base - Right * HeadRadius;
            FVector P3 = Base + Forward * HeadRadius;
            FVector P4 = Base - Forward * HeadRadius;

            OutCmds.push_back(MakeLine(End, P1, Color));
            OutCmds.push_back(MakeLine(End, P2, Color));
            OutCmds.push_back(MakeLine(End, P3, Color));
            OutCmds.push_back(MakeLine(End, P4, Color));
	}

    inline FDebugRenderCommand MakeCone(const FVector& Apex, const FVector& Dir, float Height, float Angle,
                                        const FVector4& Color, int32 SegmentCount = 16)
    {
        FDebugRenderCommand Cmd{};
        Cmd.Type = EDebugShapeType::Cone;
        Cmd.Cone = {Apex, Dir, Height, Angle, Color, SegmentCount};
        return Cmd;
    }

	inline FDebugRenderCommand MakeSphere(const FVector& Center, float Radius, const FVector4& Color,
                                          int32 SegmentCount = 64) 
	{
            FDebugRenderCommand Cmd{};
            Cmd.Type = EDebugShapeType::Sphere;
            Cmd.Sphere = {Center, Radius, Color, SegmentCount};
            return Cmd;
	}
} // namespace DebugCmd