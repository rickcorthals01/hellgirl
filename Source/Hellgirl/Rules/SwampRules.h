#pragma once
#include "CoreMinimal.h"
#include "Math/RandomStream.h"

// Plans one room of a swamp run (World II). Pure data: the same seed and room always give the same plan.
// The room is a 125 x 28 m corridor ringed by dead trees, walked west to east: muddy grass, then shallow glowing soul
// water crossed by a rotten boardwalk, then mud with a light at the end that leads on. The mud holds only a few dead
// trees; everything else (boardwalk, islands, stumps, reeds, lily pads, zombie arms) is in the water. Walking on or
// near a risen zombie arm hurts, so the arms keep clear of the boardwalk. The last room is the Frog King's: a giant
// lily pad in the middle of the water.
namespace SwampRoom
{
constexpr int32 RoomsPerRun = 4;
constexpr float Half = 6250.f;          // half the corridor's length (X)
constexpr float HalfWidth = 1400.f;     // half its width (Y), inside the tree ring
constexpr float EdgeClear = 250.f;      // contents keep this far from the tree ring
constexpr float DoorClear = 650.f;      // the way in and the light stay clear
constexpr float WaterZ = 18.f;          // the water's surface over the (flat, invisible) floor: feet wade just under it
constexpr float ArmReach = 170.f;       // a risen arm hurts anyone this close
constexpr float ArmFromBoardwalk = 320.f; // arms keep this far from the boardwalk's line, so it stays a safe way across
constexpr float PlayerRadius = 40.f;
inline const FVector2D Start(-Half + 500.f, 0.f);
inline const FVector2D Exit(Half - 400.f, 0.f);
inline const FVector2D FrogSpot(0.f, 0.f);  // the giant lily pad's centre in the last room (the middle of the water)
constexpr float GiantLilyRadius = 300.f;

enum class EPiece : uint8
{
    SwampTreeA, SwampTreeB, SwampTreeC, ZombieArm, LilyPad, LilyPadSmall, GiantLily, Boardwalk, BoardwalkBroken,
    Reeds, DrownedStump, MudIsland, Count
};

// Footprint half extents in cm (local X, local Y at scale 1), whether it blocks, and whether it may stand in the mud.
struct FFootprint { FVector2D Half; bool bSolid; };
inline FFootprint Footprint(EPiece Piece)
{
    switch (Piece)
    {
    case EPiece::SwampTreeA: case EPiece::SwampTreeB: case EPiece::SwampTreeC: return {FVector2D(55.f, 55.f), true}; // the trunk
    case EPiece::ZombieArm: return {FVector2D(20.f, 20.f), false};
    case EPiece::LilyPad: return {FVector2D(55.f, 55.f), false};
    case EPiece::LilyPadSmall: return {FVector2D(32.f, 32.f), false};
    case EPiece::GiantLily: return {FVector2D(GiantLilyRadius, GiantLilyRadius), true};
    case EPiece::Boardwalk: case EPiece::BoardwalkBroken: return {FVector2D(150.f, 70.f), true};
    case EPiece::Reeds: return {FVector2D(30.f, 30.f), false};
    case EPiece::DrownedStump: return {FVector2D(50.f, 50.f), true};
    case EPiece::MudIsland: return {FVector2D(160.f, 110.f), true};
    default: return {FVector2D(30.f, 30.f), false};
    }
}
inline bool IsTree(EPiece P) { return P == EPiece::SwampTreeA || P == EPiece::SwampTreeB || P == EPiece::SwampTreeC; }

struct FItem
{
    EPiece Piece = EPiece::SwampTreeA;
    FVector2D P = FVector2D::ZeroVector;
    float Yaw = 0.f, Scale = 1.f;
    int32 Group = 0;   // boardwalk sections share one group (they join end to end)
    FVector2D Half() const { return Footprint(Piece).Half * Scale; }
    bool IsSolid() const { return Footprint(Piece).bSolid; }
};

struct FPlan
{
    int32 Seed = 0, Room = 1;
    bool bFrogKing = false;
    float WaterStart = -Half + 2500.f, WaterEnd = -Half + 10000.f;  // X where the water begins and ends
    TArray<FVector2D> Walkway;        // the boardwalk's line, west to east
    TArray<FItem> Items;
    TArray<FVector> Fireflies;        // swarms (pure decoration)
    bool InWater(FVector2D P, float Margin = 0.f) const { return P.X > WaterStart + Margin && P.X < WaterEnd - Margin; }
};

inline FVector2D Rotate(FVector2D V, float Yaw)
{
    const float A = FMath::DegreesToRadians(Yaw), C = FMath::Cos(A), S = FMath::Sin(A);
    return FVector2D(V.X * C - V.Y * S, V.X * S + V.Y * C);
}

inline float DistanceTo(const FItem& Item, FVector2D Point)
{
    if (Item.Piece == EPiece::GiantLily) return FMath::Max(0.f, FVector2D::Distance(Item.P, Point) - Item.Half().X);
    const FVector2D L = Rotate(Point - Item.P, -Item.Yaw), H = Item.Half();
    return FVector2D(FMath::Max(0.f, FMath::Abs(L.X) - H.X), FMath::Max(0.f, FMath::Abs(L.Y) - H.Y)).Size();
}

// Distance from P to the boardwalk's line.
inline float DistanceToWalkway(const FPlan& Plan, FVector2D P)
{
    float Best = MAX_flt;
    for (int32 I = 0; I + 1 < Plan.Walkway.Num(); ++I)
    {
        const FVector2D A = Plan.Walkway[I], B = Plan.Walkway[I + 1];
        const float T = FMath::Clamp(FVector2D::DotProduct(P - A, B - A) / FMath::Max(1.f, (B - A).SizeSquared()), 0.f, 1.f);
        Best = FMath::Min(Best, FVector2D::Distance(P, A + (B - A) * T));
    }
    return Best;
}

// Where the rules allow Item: inside the tree ring, away from the way in and the light; in the mud only trees; in the
// water nothing on the boardwalk (but the boardwalk itself); arms away from the boardwalk and each other; solid
// pieces apart from each other.
inline bool IsClear(const FPlan& Plan, const FItem& Item, FString* Why = nullptr)
{
    auto No = [&](const TCHAR* Reason) { if (Why) *Why = Reason; return false; };
    const float R = Item.Half().Size();
    if (FMath::Abs(Item.P.X) > Half - EdgeClear - R || FMath::Abs(Item.P.Y) > HalfWidth - EdgeClear - R * .7f) return No(TEXT("outside the corridor"));
    if (FVector2D::Distance(Item.P, Start) < DoorClear + R || FVector2D::Distance(Item.P, Exit) < DoorClear + R) return No(TEXT("at the way in or the light"));
    const bool Water = Plan.InWater(Item.P, R * .5f);
    if (Plan.bFrogKing && Item.Piece != EPiece::GiantLily && FVector2D::Distance(Item.P, FrogSpot) < GiantLilyRadius + R + 40.f) return No(TEXT("on the Frog King's lily pad"));
    if (!Water && !IsTree(Item.Piece)) return No(TEXT("not a tree, in the mud"));
    if (Water && Item.Piece != EPiece::Boardwalk && Item.Piece != EPiece::BoardwalkBroken)
    {
        if (DistanceToWalkway(Plan, Item.P) < R + 90.f) return No(TEXT("on the boardwalk"));
        if (Item.Piece == EPiece::ZombieArm && DistanceToWalkway(Plan, Item.P) < ArmFromBoardwalk) return No(TEXT("an arm by the boardwalk"));
    }
    for (const FItem& Other : Plan.Items)
    {
        if (&Other == &Item) continue;
        const float Apart = FVector2D::Distance(Item.P, Other.P);
        if (Item.Piece == EPiece::ZombieArm && Other.Piece == EPiece::ZombieArm && Apart < 480.f) return No(TEXT("arms too close"));
        if ((Item.Piece == EPiece::ZombieArm && Other.IsSolid()) || (Other.Piece == EPiece::ZombieArm && Item.IsSolid()))
            if (Apart < ArmReach + (Item.IsSolid() ? R : Other.Half().Size())) return No(TEXT("an arm against a solid piece"));
        if (Item.IsSolid() && Other.IsSolid() && (Item.Group == 0 || Item.Group != Other.Group) && Apart < R + Other.Half().Size() + 60.f)
            return No(TEXT("too close to another piece"));
        // Lily pads and reeds do not pile on each other or on solid pieces.
        if (!Item.IsSolid() && Item.Piece != EPiece::ZombieArm && Other.Piece != EPiece::ZombieArm && Apart < (R + Other.Half().Size()) * .8f)
            return No(TEXT("overlapping"));
    }
    return true;
}

// Squares Hellgirl can walk to from the way in without touching a solid piece or a risen arm's reach.
struct FWalkGrid
{
    static constexpr float Cell = 50.f;
    int32 NX = 0, NY = 0;
    TArray<uint8> Reached;
    int32 Index(FVector2D P) const
    {
        const int32 X = FMath::Clamp(FMath::FloorToInt((P.X + Half) / Cell), 0, NX - 1), Y = FMath::Clamp(FMath::FloorToInt((P.Y + HalfWidth) / Cell), 0, NY - 1);
        return Y * NX + X;
    }
    bool CanReach(FVector2D P) const { return Reached[Index(P)] != 0; }
    explicit FWalkGrid(const FPlan& Plan)
    {
        NX = FMath::CeilToInt(Half * 2.f / Cell);
        NY = FMath::CeilToInt(HalfWidth * 2.f / Cell);
        TArray<uint8> Blocked;
        Blocked.SetNumZeroed(NX * NY);
        for (const FItem& Item : Plan.Items)
        {
            // Walkable solids (boardwalk, islands, the giant lily) are stood on, not walked round.
            const bool Obstacle = Item.IsSolid() && Item.Piece != EPiece::Boardwalk && Item.Piece != EPiece::BoardwalkBroken
                && Item.Piece != EPiece::MudIsland && Item.Piece != EPiece::GiantLily;
            if (!Obstacle && Item.Piece != EPiece::ZombieArm) continue;
            const float Keep = Item.Piece == EPiece::ZombieArm ? ArmReach + PlayerRadius : PlayerRadius;
            const float R = Item.Half().Size() + Keep;
            for (int32 Y = FMath::Max(0, FMath::FloorToInt((Item.P.Y - R + HalfWidth) / Cell)); Y <= FMath::Min(NY - 1, FMath::FloorToInt((Item.P.Y + R + HalfWidth) / Cell)); ++Y)
                for (int32 X = FMath::Max(0, FMath::FloorToInt((Item.P.X - R + Half) / Cell)); X <= FMath::Min(NX - 1, FMath::FloorToInt((Item.P.X + R + Half) / Cell)); ++X)
                    if (DistanceTo(Item, FVector2D(-Half + (X + .5f) * Cell, -HalfWidth + (Y + .5f) * Cell)) < Keep) Blocked[Y * NX + X] = 1;
        }
        Reached.SetNumZeroed(NX * NY);
        TArray<int32> Queue = {Index(Start)};
        Reached[Queue[0]] = 1;
        for (int32 Head = 0; Head < Queue.Num(); ++Head)
        {
            const int32 X = Queue[Head] % NX, Y = Queue[Head] / NX;
            const int32 Next[4][2] = {{X + 1, Y}, {X - 1, Y}, {X, Y + 1}, {X, Y - 1}};
            for (const auto& Q : Next)
                if (Q[0] >= 0 && Q[0] < NX && Q[1] >= 0 && Q[1] < NY && !Blocked[Q[1] * NX + Q[0]] && !Reached[Q[1] * NX + Q[0]])
                {
                    Reached[Q[1] * NX + Q[0]] = 1;
                    Queue.Add(Q[1] * NX + Q[0]);
                }
        }
    }
};

inline FItem MakeItem(EPiece Piece, FVector2D P, float Yaw, float Scale = 1.f, int32 Group = 0)
{
    FItem I;
    I.Piece = Piece; I.P = P; I.Yaw = Yaw; I.Scale = Scale; I.Group = Group;
    return I;
}

inline bool TryAdd(FPlan& Plan, const FItem& Item)
{
    if (!IsClear(Plan, Item)) return false;
    Plan.Items.Add(Item);
    return true;
}

inline FPlan Make(int32 Seed, int32 Room)
{
    FRandomStream Dice(static_cast<int32>(HashCombine(GetTypeHash(Seed), GetTypeHash(Room * 4099 + 71))));
    FPlan Plan;
    Plan.Seed = Seed;
    Plan.Room = FMath::Clamp(Room, 1, RoomsPerRun);
    Plan.bFrogKing = Plan.Room == RoomsPerRun;
    Plan.WaterStart = -Half + 2500.f + Dice.FRandRange(-400.f, 400.f);
    Plan.WaterEnd = -Half + 10000.f + Dice.FRandRange(-400.f, 400.f);
    auto InWaterPoint = [&](float Margin) { return FVector2D(Dice.FRandRange(Plan.WaterStart + Margin, Plan.WaterEnd - Margin), Dice.FRandRange(-HalfWidth + 350.f, HalfWidth - 350.f)); };

    // The boardwalk's line: from the water's west edge to its east edge, wandering across; in the Frog King's room it
    // bends round his lily pad.
    float Y = Dice.FRandRange(-500.f, 500.f);
    for (float X = Plan.WaterStart - 150.f; ; X += Dice.FRandRange(550.f, 750.f))
    {
        X = FMath::Min(X, Plan.WaterEnd + 150.f);
        Y = FMath::Clamp(Y + Dice.FRandRange(-450.f, 450.f), -HalfWidth + 500.f, HalfWidth - 500.f);
        if (Plan.bFrogKing && FMath::Abs(X - FrogSpot.X) < GiantLilyRadius + 700.f) Y = (Y >= 0.f ? 1.f : -1.f) * (HalfWidth - 450.f);
        Plan.Walkway.Add(FVector2D(X, Y));
        if (X >= Plan.WaterEnd + 150.f) break;
    }
    // Sections laid end to end along it (every 290 cm, so the planks overlap a little), now and then a broken one.
    for (int32 I = 0; I + 1 < Plan.Walkway.Num(); ++I)
    {
        const FVector2D A = Plan.Walkway[I], B = Plan.Walkway[I + 1];
        const float Length = FVector2D::Distance(A, B), Yaw = FMath::RadiansToDegrees(FMath::Atan2(B.Y - A.Y, B.X - A.X));
        for (float T = 145.f; T < Length + 60.f; T += 290.f)
            Plan.Items.Add(MakeItem(Dice.FRand() < .22f ? EPiece::BoardwalkBroken : EPiece::Boardwalk, A + (B - A) / Length * FMath::Min(T, Length), Yaw, 1.f, 1));
    }
    if (Plan.bFrogKing) Plan.Items.Add(MakeItem(EPiece::GiantLily, FrogSpot, Dice.FRandRange(0.f, 360.f), 1.f, 2));

    // Mud: a few dead trees, nothing else.
    for (int32 Side = 0; Side < 2; ++Side)
    {
        const float From = Side ? Plan.WaterEnd : -Half, To = Side ? Half : Plan.WaterStart;
        const int32 Wanted = Dice.RandRange(2, 4);
        for (int32 Try = 0, Placed = 0; Placed < Wanted && Try < 80; ++Try)
        {
            const FVector2D P(Dice.FRandRange(From + 200.f, To - 200.f), Dice.FRandRange(-HalfWidth + 400.f, HalfWidth - 400.f));
            Placed += TryAdd(Plan, MakeItem(static_cast<EPiece>(Dice.RandRange(0, 2)), P, Dice.FRandRange(0.f, 360.f), Dice.FRandRange(.8f, 1.15f)));
        }
    }

    // The water: islands, drowned trees and stumps, reeds by the edges, zombie arms, and lily pads everywhere else.
    auto Scatter = [&](EPiece Piece, int32 Count, float MinScale, float MaxScale, float Margin)
    {
        for (int32 Try = 0, Placed = 0; Placed < Count && Try < Count * 40; ++Try)
            Placed += TryAdd(Plan, MakeItem(Piece, InWaterPoint(Margin), Dice.FRandRange(0.f, 360.f), Dice.FRandRange(MinScale, MaxScale)));
    };
    Scatter(EPiece::MudIsland, Dice.RandRange(2, 4), .8f, 1.3f, 300.f);
    Scatter(static_cast<EPiece>(Dice.RandRange(0, 2)), Dice.RandRange(2, 3), .7f, 1.f, 300.f);
    Scatter(EPiece::DrownedStump, Dice.RandRange(3, 6), .8f, 1.3f, 200.f);
    Scatter(EPiece::ZombieArm, Dice.RandRange(6, 9), .9f, 1.1f, 250.f);
    Scatter(EPiece::Reeds, Dice.RandRange(8, 16), .8f, 1.3f, 150.f);
    Scatter(EPiece::LilyPad, Plan.bFrogKing ? 70 : 45, .8f, 1.3f, 120.f);
    Scatter(EPiece::LilyPadSmall, Plan.bFrogKing ? 50 : 35, .8f, 1.3f, 100.f);

    // Fireflies: swarms drifting here and there along the corridor.
    const int32 Swarms = Dice.RandRange(5, 8);
    for (int32 I = 0; I < Swarms; ++I)
        Plan.Fireflies.Add(FVector(Dice.FRandRange(-Half + 600.f, Half - 600.f), Dice.FRandRange(-HalfWidth + 300.f, HalfWidth - 300.f), Dice.FRandRange(120.f, 260.f)));
    return Plan;
}

// Every rule a generated room must satisfy; the swamp check runs this over many seeds.
inline bool IsFair(const FPlan& Plan, FString* Why = nullptr)
{
    auto Fail = [&](const FString& Reason) { if (Why) *Why = Reason; return false; };
    if (Plan.WaterEnd - Plan.WaterStart < 6000.f) return Fail(TEXT("too little water"));
    int32 Arms = 0, Pads = 0, Walks = 0, Giant = 0;
    for (const FItem& Item : Plan.Items)
    {
        FString Reason;
        const bool Walkway = Item.Piece == EPiece::Boardwalk || Item.Piece == EPiece::BoardwalkBroken;
        if (!Walkway && !IsClear(Plan, Item, &Reason)) return Fail(FString::Printf(TEXT("piece %d at %s: %s"), static_cast<int32>(Item.Piece), *Item.P.ToString(), *Reason));
        if (!Plan.InWater(Item.P, -200.f) && !IsTree(Item.Piece)) return Fail(TEXT("something besides a tree in the mud"));
        Arms += Item.Piece == EPiece::ZombieArm;
        Pads += Item.Piece == EPiece::LilyPad || Item.Piece == EPiece::LilyPadSmall;
        Walks += Walkway;
        Giant += Item.Piece == EPiece::GiantLily;
    }
    if (Arms < 5) return Fail(FString::Printf(TEXT("only %d zombie arms"), Arms));
    if (Pads < 30) return Fail(FString::Printf(TEXT("only %d lily pads"), Pads));
    if (Walks < 20) return Fail(TEXT("the boardwalk is too short"));
    if (Giant != (Plan.bFrogKing ? 1 : 0)) return Fail(TEXT("the Frog King's lily pad is wrong"));
    const FWalkGrid Walk(Plan);
    if (!Walk.CanReach(Exit)) return Fail(TEXT("the light cannot be reached without passing an arm"));
    return true;
}
}
