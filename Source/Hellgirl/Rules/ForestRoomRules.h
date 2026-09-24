#pragma once
#include "CoreMinimal.h"
#include "Math/RandomStream.h"

// Plans one room of a forest run. Pure data: the same seed and room number always give the same plan,
// so a run can be replayed or a bug reproduced from its seed. The arena shape is fixed (a round clearing
// with the entrance to the west and the exit trail to the east); only its contents are rolled.
namespace ForestRoom
{
constexpr int32 RoomsPerRun = 4;          // rooms 1-3 are fights, room 4 is the Goblin Queen
constexpr float Radius = 2600.f;          // walkable clearing (the tree wall starts here)
constexpr float EdgeClear = 380.f;        // cover stays this far inside the tree wall
constexpr float CentreClear = 650.f;      // open fighting circle in the middle
constexpr float DoorClear = 700.f;        // nothing blocks the entrance or the exit
constexpr float CoverSpacing = 480.f;     // gap between two pieces of cover
constexpr float MinSpawnDistance = 1300.f; // waves never appear on top of the entrance
inline const FVector2D Start(-2150.f, 0.f);
inline const FVector2D Exit(2200.f, 0.f);
inline const FVector2D QueenSpot(900.f, 0.f);

enum class ECover : uint8 { Boulder, Log, Stump };
enum class EWave : uint8 { Trickle, Ambush, Elite };

struct FCover { ECover Type = ECover::Boulder; FVector2D Position = FVector2D::ZeroVector; float Yaw = 0.f; float Size = 1.f; };
struct FThorns { FVector2D Position = FVector2D::ZeroVector; float Radius = 300.f; };
struct FWave { EWave Style = EWave::Trickle; int32 Count = 3; FVector2D Position = FVector2D::ZeroVector; };

struct FPlan
{
    int32 Seed = 0, Room = 1;
    bool bBoss = false;
    TArray<FCover> Cover;
    TArray<FThorns> Thorns;
    TArray<FWave> Waves;
};

// Footprint radius of a piece of cover (logs are long, so they need more room).
inline float CoverReach(const FCover& C) { return (C.Type == ECover::Log ? 280.f : C.Type == ECover::Boulder ? 240.f : 90.f) * C.Size; }

// True when a circle of radius Keep at P respects every safety rule against what is already planned.
inline bool IsClear(const FPlan& Plan, FVector2D P, float Keep)
{
    if (P.Size() > Radius - EdgeClear - Keep || P.Size() < CentreClear + Keep) return false;
    if (FVector2D::Distance(P, Start) < DoorClear + Keep || FVector2D::Distance(P, Exit) < DoorClear + Keep) return false;
    if (Plan.bBoss && FVector2D::Distance(P, QueenSpot) < 500.f + Keep) return false;
    for (const FCover& C : Plan.Cover)
        if (FVector2D::Distance(P, C.Position) < CoverSpacing + CoverReach(C) + Keep) return false;
    for (const FThorns& T : Plan.Thorns)
        if (FVector2D::Distance(P, T.Position) < T.Radius + Keep + 150.f) return false;
    return true;
}

inline FPlan Make(int32 Seed, int32 Room)
{
    FRandomStream Dice(static_cast<int32>(HashCombine(GetTypeHash(Seed), GetTypeHash(Room * 7919 + 13))));
    FPlan Plan;
    Plan.Seed = Seed;
    Plan.Room = FMath::Clamp(Room, 1, RoomsPerRun);
    Plan.bBoss = Plan.Room == RoomsPerRun;
    auto RandomPoint = [&](float MinR, float MaxR)
    {
        const float A = Dice.FRandRange(0.f, 2.f * PI), D = Dice.FRandRange(MinR, MaxR);
        return FVector2D(FMath::Cos(A) * D, FMath::Sin(A) * D);
    };

    // Cover: boulders, fallen logs and stumps to fight around. The boss room stays more open.
    const int32 CoverWanted = Plan.bBoss ? Dice.RandRange(2, 4) : Dice.RandRange(4, 8);
    for (int32 Try = 0; Plan.Cover.Num() < CoverWanted && Try < 300; ++Try)
    {
        FCover C;
        C.Type = static_cast<ECover>(Dice.RandRange(0, 2));
        C.Size = Dice.FRandRange(.8f, 1.3f);
        C.Yaw = Dice.FRandRange(0.f, 360.f);
        C.Position = RandomPoint(CentreClear, Radius - EdgeClear);
        if (IsClear(Plan, C.Position, CoverReach(C))) Plan.Cover.Add(C);
    }

    // Thorn patches hurt Hellgirl while she stands in them. None in the first room or the boss room.
    const int32 ThornsWanted = Plan.bBoss || Plan.Room == 1 ? 0 : Dice.RandRange(1, Plan.Room);
    for (int32 Try = 0; Plan.Thorns.Num() < ThornsWanted && Try < 300; ++Try)
    {
        FThorns T;
        T.Radius = Dice.FRandRange(240.f, 400.f);
        T.Position = RandomPoint(CentreClear, Radius - EdgeClear);
        if (IsClear(Plan, T.Position, T.Radius)) Plan.Thorns.Add(T);
    }

    // Waves: more and bigger each room. Ambushes arrive all at once, elites are few but tougher.
    if (!Plan.bBoss)
    {
        const int32 WaveCount = Plan.Room == 1 ? 2 : Dice.RandRange(2, 3);
        int32 Budget = 6 + 5 * Plan.Room;
        for (int32 I = 0; I < WaveCount; ++I)
        {
            FWave W;
            const int32 Roll = Dice.RandRange(0, 99);
            W.Style = I == 0 || Roll < 55 ? EWave::Trickle : Roll < 82 ? EWave::Ambush : EWave::Elite;
            W.Count = W.Style == EWave::Elite ? FMath::Min(Plan.Room, 2)
                : W.Style == EWave::Ambush ? 2 + Plan.Room + Dice.RandRange(0, 1)
                : 2 + Plan.Room + Dice.RandRange(0, 2);
            // Leave at least two enemies for each later wave.
            W.Count = FMath::Clamp(W.Count, 1, Budget - 2 * (WaveCount - 1 - I));
            Budget -= W.Count;
            // Spawn on the far side of the clearing, never behind the entrance.
            bool Free = false;
            for (int32 Try = 0; Try < 60 && !Free; ++Try)
            {
                const float A = FMath::DegreesToRadians(Dice.FRandRange(-120.f, 120.f)), D = Dice.FRandRange(1300.f, 1900.f);
                W.Position = FVector2D(FMath::Cos(A) * D, FMath::Sin(A) * D);
                Free = FVector2D::Distance(W.Position, Start) >= MinSpawnDistance;
                for (const FCover& C : Plan.Cover) Free &= FVector2D::Distance(W.Position, C.Position) > CoverReach(C) + 250.f;
            }
            if (!Free) W.Position = FVector2D(0.f, I % 2 ? 1500.f : -1500.f); // in the open side lanes
            Plan.Waves.Add(W);
        }
    }
    return Plan;
}

// Every rule a generated room must satisfy; the forest run check runs this over many seeds.
inline bool IsFair(const FPlan& Plan, FString* Why = nullptr)
{
    auto Fail = [&](const TCHAR* Reason) { if (Why) *Why = Reason; return false; };
    if (Plan.bBoss != (Plan.Room == RoomsPerRun)) return Fail(TEXT("boss room mismatch"));
    if (Plan.bBoss ? !Plan.Waves.IsEmpty() : (Plan.Waves.Num() < 2 || Plan.Waves.Num() > 3)) return Fail(TEXT("wave count"));
    if (Plan.Cover.Num() < 2) return Fail(TEXT("too little cover"));
    FPlan Empty; Empty.bBoss = Plan.bBoss;
    for (int32 I = 0; I < Plan.Cover.Num(); ++I)
    {
        Empty.Cover = Plan.Cover; Empty.Cover.RemoveAt(I); Empty.Thorns = Plan.Thorns;
        if (!IsClear(Empty, Plan.Cover[I].Position, CoverReach(Plan.Cover[I]))) return Fail(TEXT("cover breaks a spacing rule"));
    }
    for (int32 I = 0; I < Plan.Thorns.Num(); ++I)
    {
        Empty.Cover = Plan.Cover; Empty.Thorns = Plan.Thorns; Empty.Thorns.RemoveAt(I);
        if (!IsClear(Empty, Plan.Thorns[I].Position, Plan.Thorns[I].Radius)) return Fail(TEXT("thorns break a spacing rule"));
    }
    int32 Enemies = 0;
    for (const FWave& W : Plan.Waves)
    {
        if (W.Count < 1 || FVector2D::Distance(W.Position, Start) < MinSpawnDistance || W.Position.Size() > Radius - 300.f)
            return Fail(TEXT("wave placement"));
        Enemies += W.Count;
    }
    if (Enemies > 6 + 5 * Plan.Room) return Fail(TEXT("too many enemies"));
    return true;
}
}
