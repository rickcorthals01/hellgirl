#pragma once
#include "CoreMinimal.h"
#include "Rules/FrozenMazeLayout.h"

// World IV's Frozen Maze: the Deprived's icy cave, one big maze. The layout is designed in Tools/Maze and exported to
// Rules/FrozenMazeLayout.h; this turns it into the world. One grid square is 8 m; the map is centred on the origin with
// +X east and +Y south (row 0 is the north edge). Built in Levels/FrozenMaze.cpp and Levels/FrozenMazeScenery.cpp.
namespace FrozenMaze
{
using FrozenMazeLayout::FCell;
using FrozenMazeLayout::Columns;
using FrozenMazeLayout::Rows;

inline constexpr float Cell = 800.f;
inline constexpr float WallHalf = 75.f;       // half the thickness of the walls' blockers (the ice is about 1.3 m through)
inline constexpr float WallHeight = 1150.f;   // the ice columns reach 9 to 14 m: nobody sees over them
inline constexpr float OuterScale = 2.5f;     // the outer wall rises to the cave roof
inline constexpr float RoofHeight = 2800.f;

inline FVector2D Corner(float Column, float Row) { return FVector2D((Column - Columns * .5f) * Cell, (Row - Rows * .5f) * Cell); }
inline FVector2D Centre(int32 X, int32 Y) { return Corner(X + .5f, Y + .5f); }
inline FVector2D Centre(FCell C) { return Centre(C.X, C.Y); }
inline bool Inside(int32 X, int32 Y) { return X >= 0 && Y >= 0 && X < Columns && Y < Rows; }
inline int32 Index(int32 X, int32 Y) { return X + Y * Columns; }
inline FCell CellAt(FVector2D P) { return {FMath::FloorToInt(P.X / Cell + Columns * .5f), FMath::FloorToInt(P.Y / Cell + Rows * .5f)}; }
inline bool WallEast(int32 X, int32 Y)
{
    if (X >= Columns - 1) return true;
    const TCHAR C = FrozenMazeLayout::Walls[Y][X];
    return C == '1' || C == '3';
}
inline bool WallSouth(int32 X, int32 Y)
{
    if (Y >= Rows - 1) return true;
    const TCHAR C = FrozenMazeLayout::Walls[Y][X];
    return C == '2' || C == '3';
}
// The four steps (east, west, south, north).
inline constexpr int32 StepX[4] = {1, -1, 0, 0}, StepY[4] = {0, 0, 1, -1};
// Whether she can walk from a cell into the next one (DX, DY one step).
inline bool Open(int32 X, int32 Y, int32 DX, int32 DY)
{
    if (!Inside(X, Y) || !Inside(X + DX, Y + DY)) return false;
    if (DX == 1) return !WallEast(X, Y);
    if (DX == -1) return !WallEast(X - 1, Y);
    if (DY == 1) return !WallSouth(X, Y);
    return !WallSouth(X, Y - 1);
}
inline int32 OpenSides(int32 X, int32 Y)
{
    int32 Count = 0;
    for (int32 D = 0; D < 4; ++D) Count += Open(X, Y, StepX[D], StepY[D]);
    return Count;
}

// Rooms (cells X0..X1-1, Y0..Y1-1): open inside, with one door: the room's cell at the door and the step out.
struct FRoom { const TCHAR* Name; int32 X0, Y0, X1, Y1; FCell Door; int32 OutX, OutY; };
inline constexpr FRoom SpawnRooms[4] = {
    {TEXT("S1"), 0, 0, 5, 3, {1, 2}, 0, 1}, {TEXT("S2"), 32, 0, 38, 3, {32, 1}, -1, 0},
    {TEXT("S3"), 0, 22, 5, 26, {4, 23}, 1, 0}, {TEXT("S4"), 33, 20, 38, 26, {33, 22}, -1, 0}};
inline constexpr FRoom VortexRoom = {TEXT("Vortex"), 14, 7, 24, 15, {23, 7}, 0, -1};
inline bool InRoom(const FRoom& R, int32 X, int32 Y) { return X >= R.X0 && X < R.X1 && Y >= R.Y0 && Y < R.Y1; }
inline bool InAnyRoom(int32 X, int32 Y)
{
    for (const FRoom& R : SpawnRooms) if (InRoom(R, X, Y)) return true;
    return InRoom(VortexRoom, X, Y);
}
inline FVector2D RoomCentre(const FRoom& R) { return Corner((R.X0 + R.X1) * .5f, (R.Y0 + R.Y1) * .5f); }
inline FCell OutsideDoor(const FRoom& R) { return {R.Door.X + R.OutX, R.Door.Y + R.OutY}; }
// The vortex hangs over the middle of its room, above the frozen lake.
inline FVector2D VortexCentre() { return RoomCentre(VortexRoom); }
inline constexpr float VortexReach = 380.f;   // stepping this close takes her through
inline constexpr float LakeRadius = 2400.f;
// Hellgirl arrives in the middle of her spawn room (one of four, picked per run), facing its door.
inline const FRoom& SpawnRoom(int32 Room) { return SpawnRooms[FMath::Clamp(Room, 0, 3)]; }
inline FVector2D SpawnPoint(int32 Room) { return RoomCentre(SpawnRoom(Room)); }
inline float SpawnYaw(int32 Room)
{
    const FRoom& R = SpawnRoom(Room);
    const FVector2D To = Centre(R.Door) - SpawnPoint(Room);
    return FMath::RadiansToDegrees(FMath::Atan2(To.Y, To.X));
}

// Waterfalls pouring down the outer wall into pools: where along it (in columns), and which wall.
struct FFall { const TCHAR* Name; float X0, X1; bool bNorth; };
inline constexpr FFall Falls[] = {{TEXT("W1"), 7.875f, 9.625f, true}, {TEXT("W2"), 14.f, 15.125f, true}, {TEXT("W3"), 22.625f, 25.25f, true},
    {TEXT("W4"), 7.75f, 10.f, false}, {TEXT("W5"), 24.5f, 29.75f, false}};

// Steps from the nearest of the given cells to every cell (-1 where it cannot be reached).
inline TArray<int32> Walk(TArrayView<const FCell> From)
{
    TArray<int32> Steps;
    Steps.Init(-1, Columns * Rows);
    TArray<FCell> Queue;
    for (const FCell& C : From) if (Inside(C.X, C.Y) && Steps[Index(C.X, C.Y)] < 0) { Steps[Index(C.X, C.Y)] = 0; Queue.Add(C); }
    for (int32 Head = 0; Head < Queue.Num(); ++Head)
    {
        const FCell C = Queue[Head];
        for (int32 D = 0; D < 4; ++D)
        {
            const FCell N{C.X + StepX[D], C.Y + StepY[D]};
            if (!Open(C.X, C.Y, StepX[D], StepY[D]) || Steps[Index(N.X, N.Y)] >= 0) continue;
            Steps[Index(N.X, N.Y)] = Steps[Index(C.X, C.Y)] + 1;
            Queue.Add(N);
        }
    }
    return Steps;
}

// Icicle traps: she shakes one loose by walking under it; its shadow grows on the ice for a moment, then it falls.
inline constexpr float TrapTrigger = 330.f, TrapWarning = .9f, TrapFall = .7f, TrapHitRadius = 240.f, TrapDamage = 18.f, TrapRegrow = 20.f;
inline constexpr float BridgeHeight = 1050.f;   // the ice bridge across the corridor that the big icicle hangs from
// Frozen remains: an attack this close, in front of her, smashes one; the Souls frozen inside drop out.
inline constexpr float RemainsReach = 300.f;
inline constexpr int32 RemainsSouls = 12;
}
