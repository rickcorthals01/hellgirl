#pragma once
#include "CoreMinimal.h"

// The Frozen Maze's boss room (design: Developer idea folder lol\Boss room design plan.png, drawn by Tools/Maze/boss_plan.ps1
// from the user's sketch): a wide open oval ice cavern beyond the vortex. The vortex's charred imprint lies in the middle,
// four huge stalagmites stand round it under stalactites, frozen bodies hold Souls, and purple lightning crawls along the
// walls and arcs across the roof. The boss (to be decided) waits across the imprint from where she arrives.
// Built in Levels/MazeBossRoom.cpp. +X east, +Y south, centred on the origin.
namespace MazeBoss
{
inline constexpr float HalfX = 5000.f, HalfY = 3500.f;   // the floor: an oval 100 x 70 m
inline constexpr float RoofHeight = 4500.f;
inline constexpr float ImprintX = 1700.f, ImprintY = 1000.f; // half the charred imprint's size (34 x 20 m)
inline const FVector2D Start(-2200.f, 0.f);                  // she steps out of the vortex here, facing east
inline const FVector2D BossSpot(2800.f, 0.f);                // where the boss will wait (kept clear)
// Stalagmites (M1-M4, from the sketch), each under a stalactite; how big (1 = the kit's 7.5 m spire).
struct FSpire { FVector2D P; float Scale; };
inline const FSpire Stalagmites[] = {{FVector2D(-2955.f, -1672.f), 3.2f}, {FVector2D(-1464.f, -1833.f), 2.6f},
    {FVector2D(2007.f, -1329.f), 3.0f}, {FVector2D(2407.f, 1879.f), 3.3f}};
// Frozen bodies (R1-R6, from the sketch).
inline const FVector2D Bodies[] = {FVector2D(-2754.f, -596.f), FVector2D(-62.f, -1741.f), FVector2D(2407.f, -550.f), FVector2D(2585.f, 687.f),
    FVector2D(-1598.f, 1237.f), FVector2D(-3021.f, 2108.f)};
// Whether a point is on the floor, Margin inside the wall.
inline bool OnFloor(FVector2D P, float Margin = 0.f)
{
    return FMath::Square(P.X / (HalfX - Margin)) + FMath::Square(P.Y / (HalfY - Margin)) <= 1.f;
}
// A point on the wall's line (the edge of the floor) at an angle (radians), Scale 1 on the edge.
inline FVector2D Edge(float Angle, float Scale = 1.f) { return FVector2D(FMath::Cos(Angle) * HalfX, FMath::Sin(Angle) * HalfY) * Scale; }
// Lightning: each bolt flashes for a moment, then waits a few seconds before it strikes again somewhere nearby.
inline constexpr float BoltFlash = .28f, BoltWaitMin = 1.2f, BoltWaitMax = 4.5f;
}
