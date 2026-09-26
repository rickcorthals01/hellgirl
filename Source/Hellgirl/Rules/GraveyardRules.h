#pragma once
#include "CoreMinimal.h"
#include "Math/RandomStream.h"

// Plans one room of a graveyard run (World IV). Pure data: the same seed and room always give the same plan.
// The yard's shape is fixed: a 110 m square inside a railed stone wall, split into four quarters by two crossing
// gravel paths, entered through an iron gate in the west wall and left through a crypt in the east wall. Each room
// rolls which theme fills each quarter, what stands where in it, the landmark at the crossing, the mist and the moon.
namespace GraveRoom
{
constexpr int32 RoomsPerRun = 4;
constexpr float Half = 5500.f;           // inner face of the boundary wall
constexpr float PathHalf = 300.f;        // the two crossing gravel paths are 6 m wide
constexpr float PathClear = 120.f;       // nothing solid this close to a path's edge
constexpr float PlazaRadius = 950.f;     // the open crossing; only its landmark stands in it
constexpr float EdgeClear = 350.f;       // gap between the contents and the wall
constexpr float ClearingRadius = 750.f;  // each quarter keeps one open patch to fight in
constexpr float DoorClear = 800.f;       // the gate and the crypt stay clear
constexpr float Gap = 70.f;              // between solid pieces of different groups
constexpr float PlayerRadius = 40.f;     // for the reachability test
inline const FVector2D Start(-4900.f, 0.f);
inline const FVector2D Exit(Half - 250.f, 0.f);

enum class ETheme : uint8 { OldGraves, Mausoleums, RoseGarden, OpenGraves, Crypts, Overgrown, Count };
enum class ELandmark : uint8 { DeadOak, Well, Angel, Count };
enum class EPiece : uint8
{
    HeadstoneRound, HeadstoneSmall, HeadstoneGothic, HeadstoneCross, HeadstoneCeltic, HeadstoneBroken, Obelisk,
    GraveMound, GraveLedger, OpenGrave, Sarcophagus, MausoleumGothic, MausoleumDome,
    RoseBush, RoseBushSmall, RosesLaid, RoseArch, Hedge, DeadOak, Well, StatueMourner, StatueAngel,
    Candles, GhostLantern, Bones, BrokenColumn, DeadTree, Count
};

inline const TCHAR* ThemeName(ETheme T)
{
    static const TCHAR* Names[] = {TEXT("Old graves"), TEXT("Mausoleums"), TEXT("Rose garden"), TEXT("Open graves"), TEXT("Crypts"), TEXT("Overgrown")};
    return Names[static_cast<int32>(T) % static_cast<int32>(ETheme::Count)];
}

// Footprint half extents in cm (local X, local Y at scale 1) and whether the piece blocks movement.
struct FFootprint { FVector2D Half; bool bSolid; };
inline FFootprint Footprint(EPiece Piece)
{
    switch (Piece)
    {
    case EPiece::HeadstoneRound: case EPiece::HeadstoneGothic: return {FVector2D(40.f, 20.f), true};
    case EPiece::HeadstoneSmall: return {FVector2D(34.f, 19.f), true};
    case EPiece::HeadstoneCross: case EPiece::HeadstoneCeltic: return {FVector2D(31.f, 24.f), true};
    case EPiece::HeadstoneBroken: return {FVector2D(42.f, 42.f), true};
    case EPiece::Obelisk: return {FVector2D(48.f, 48.f), true};
    case EPiece::GraveMound: return {FVector2D(55.f, 110.f), false};
    case EPiece::GraveLedger: return {FVector2D(50.f, 105.f), false};
    case EPiece::OpenGrave: return {FVector2D(90.f, 140.f), true};      // the pit and its rim; the pile lies beside it
    case EPiece::Sarcophagus: return {FVector2D(57.f, 117.f), true};
    case EPiece::MausoleumGothic: return {FVector2D(182.f, 330.f), true};
    case EPiece::MausoleumDome: return {FVector2D(207.f, 300.f), true};
    case EPiece::RoseBush: return {FVector2D(60.f, 60.f), false};
    case EPiece::RoseBushSmall: return {FVector2D(45.f, 45.f), false};
    case EPiece::RosesLaid: return {FVector2D(22.f, 30.f), false};
    case EPiece::RoseArch: return {FVector2D(125.f, 40.f), false};
    case EPiece::Hedge: return {FVector2D(146.f, 36.f), true};     // a little short of the mesh, so sections can touch
    case EPiece::DeadOak: return {FVector2D(130.f, 130.f), true};
    case EPiece::Well: return {FVector2D(128.f, 128.f), true};
    case EPiece::StatueMourner: return {FVector2D(56.f, 56.f), true};
    case EPiece::StatueAngel: return {FVector2D(60.f, 60.f), true};
    case EPiece::BrokenColumn: return {FVector2D(55.f, 110.f), true};
    case EPiece::DeadTree: return {FVector2D(45.f, 45.f), false};
    default: return {FVector2D(25.f, 25.f), false};                     // candles, lanterns, bones
    }
}

struct FItem
{
    EPiece Piece = EPiece::HeadstoneRound;
    FVector2D P = FVector2D::ZeroVector;
    float Yaw = 0.f, Pitch = 0.f, Roll = 0.f, Scale = 1.f;
    int32 Group = 0;  // pieces of one structure (a row of graves, a garden) are spaced by their generator
    FVector2D Half() const { return Footprint(Piece).Half * Scale; }
    bool IsSolid() const { return Footprint(Piece).bSolid; }
};

struct FPlan
{
    int32 Seed = 0, Room = 1;
    ETheme Themes[4] = {};           // NW, NE, SW, SE
    FVector2D Clearings[4] = {};
    ELandmark Landmark = ELandmark::DeadOak;
    float Mist = .6f;                // 0..1
    float MoonYaw = 0.f;             // degrees around the east
    TArray<FItem> Items;
};

// The kit's front is local +Y in Unreal (Blender's -Y). The yaw that turns a piece's front toward Direction.
inline float FaceYaw(FVector2D Direction) { return FMath::RadiansToDegrees(FMath::Atan2(Direction.Y, Direction.X)) - 90.f; }

inline FVector2D Rotate(FVector2D V, float Yaw)
{
    const float A = FMath::DegreesToRadians(Yaw), C = FMath::Cos(A), S = FMath::Sin(A);
    return FVector2D(V.X * C - V.Y * S, V.X * S + V.Y * C);
}

// Distance from a point to an item's footprint (0 inside it).
inline float DistanceTo(const FItem& Item, FVector2D Point)
{
    const FVector2D L = Rotate(Point - Item.P, -Item.Yaw), H = Item.Half();
    return FVector2D(FMath::Max(0.f, FMath::Abs(L.X) - H.X), FMath::Max(0.f, FMath::Abs(L.Y) - H.Y)).Size();
}

inline void Corners(const FItem& Item, FVector2D Out[4])
{
    const FVector2D H = Item.Half();
    const FVector2D Local[4] = {FVector2D(-H.X, -H.Y), FVector2D(H.X, -H.Y), FVector2D(H.X, H.Y), FVector2D(-H.X, H.Y)};
    for (int32 I = 0; I < 4; ++I) Out[I] = Item.P + Rotate(Local[I], Item.Yaw);
}

// Two footprints, each grown by Pad, overlap (separating-axis test on the four edge directions).
inline bool Overlap(const FItem& A, const FItem& B, float Pad)
{
    if (FVector2D::Distance(A.P, B.P) > A.Half().Size() + B.Half().Size() + Pad * 2.f) return false;
    FVector2D CA[4], CB[4];
    Corners(A, CA); Corners(B, CB);
    const FVector2D Axes[4] = {Rotate(FVector2D(1, 0), A.Yaw), Rotate(FVector2D(0, 1), A.Yaw), Rotate(FVector2D(1, 0), B.Yaw), Rotate(FVector2D(0, 1), B.Yaw)};
    for (const FVector2D& Axis : Axes)
    {
        float MinA = MAX_flt, MaxA = -MAX_flt, MinB = MAX_flt, MaxB = -MAX_flt;
        for (int32 I = 0; I < 4; ++I)
        {
            const float PA = FVector2D::DotProduct(CA[I], Axis), PB = FVector2D::DotProduct(CB[I], Axis);
            MinA = FMath::Min(MinA, PA); MaxA = FMath::Max(MaxA, PA); MinB = FMath::Min(MinB, PB); MaxB = FMath::Max(MaxB, PB);
        }
        if (MaxA + Pad < MinB - Pad || MaxB + Pad < MinA - Pad) return false;
    }
    return true;
}

// Where the rules allow Item: inside the yard, off the paths, out of the plaza (unless it is the landmark), away
// from the gate, the crypt and the quarters' clearings, and (if solid) apart from solid pieces of other groups.
inline bool IsClear(const FPlan& Plan, const FItem& Item, FString* Why = nullptr)
{
    auto No = [&](const TCHAR* Reason) { if (Why) *Why = Reason; return false; };
    const bool Solid = Item.IsSolid();
    const bool Landmark = Item.Group < 0;
    FVector2D C[4];
    Corners(Item, C);
    for (const FVector2D& P : C)
    {
        if (FMath::Abs(P.X) > Half - EdgeClear || FMath::Abs(P.Y) > Half - EdgeClear) return No(TEXT("outside the yard"));
        if (!Landmark && (FMath::Abs(P.X) < PathHalf + (Solid ? PathClear : 20.f) || FMath::Abs(P.Y) < PathHalf + (Solid ? PathClear : 20.f)))
            return No(TEXT("on a path"));
        if (!Landmark && (FMath::Sign(P.X) != FMath::Sign(C[0].X) || FMath::Sign(P.Y) != FMath::Sign(C[0].Y))) return No(TEXT("across a path"));
    }
    if (!Landmark && DistanceTo(Item, FVector2D::ZeroVector) < PlazaRadius) return No(TEXT("in the plaza"));
    if (DistanceTo(Item, Start) < DoorClear || DistanceTo(Item, Exit) < DoorClear) return No(TEXT("at a door"));
    if (Solid)
    {
        for (const FVector2D& Clearing : Plan.Clearings)
            if (!Clearing.IsZero() && DistanceTo(Item, Clearing) < ClearingRadius) return No(TEXT("in a clearing"));
        for (const FItem& Other : Plan.Items)
            if (&Other != &Item && Other.IsSolid() && (Other.Group != Item.Group ? Overlap(Item, Other, Gap * .5f) : Overlap(Item, Other, 0.f)))
                return No(TEXT("too close to another piece"));
    }
    return true;
}

// The squares of the yard Hellgirl can walk to from the gate (PlayerRadius around every solid piece is blocked).
struct FWalkGrid
{
    static constexpr float Cell = 50.f;
    int32 N = 0;
    TArray<uint8> Reached;
    int32 Index(FVector2D P) const
    {
        const int32 X = FMath::Clamp(FMath::FloorToInt((P.X + Half) / Cell), 0, N - 1), Y = FMath::Clamp(FMath::FloorToInt((P.Y + Half) / Cell), 0, N - 1);
        return Y * N + X;
    }
    bool CanReach(FVector2D P) const { return Reached[Index(P)] != 0; }
    explicit FWalkGrid(const FPlan& Plan)
    {
        N = FMath::CeilToInt(Half * 2.f / Cell);
        TArray<uint8> Blocked;
        Blocked.SetNumZeroed(N * N);
        for (const FItem& Item : Plan.Items)
        {
            if (!Item.IsSolid()) continue;
            const float R = Item.Half().Size() + PlayerRadius;
            const int32 X0 = FMath::Max(0, FMath::FloorToInt((Item.P.X - R + Half) / Cell)), X1 = FMath::Min(N - 1, FMath::FloorToInt((Item.P.X + R + Half) / Cell));
            const int32 Y0 = FMath::Max(0, FMath::FloorToInt((Item.P.Y - R + Half) / Cell)), Y1 = FMath::Min(N - 1, FMath::FloorToInt((Item.P.Y + R + Half) / Cell));
            for (int32 Y = Y0; Y <= Y1; ++Y)
                for (int32 X = X0; X <= X1; ++X)
                    if (DistanceTo(Item, FVector2D(-Half + (X + .5f) * Cell, -Half + (Y + .5f) * Cell)) < PlayerRadius) Blocked[Y * N + X] = 1;
        }
        Reached.SetNumZeroed(N * N);
        TArray<int32> Queue;
        Queue.Add(Index(Start));
        Reached[Queue[0]] = 1;
        for (int32 Head = 0; Head < Queue.Num(); ++Head)
        {
            const int32 X = Queue[Head] % N, Y = Queue[Head] / N;
            const int32 Next[4][2] = {{X + 1, Y}, {X - 1, Y}, {X, Y + 1}, {X, Y - 1}};
            for (const auto& Q : Next)
                if (Q[0] >= 0 && Q[0] < N && Q[1] >= 0 && Q[1] < N && !Blocked[Q[1] * N + Q[0]] && !Reached[Q[1] * N + Q[0]])
                {
                    Reached[Q[1] * N + Q[0]] = 1;
                    Queue.Add(Q[1] * N + Q[0]);
                }
        }
    }
};

// One quarter of the yard: SX/SY give its side (+1 east/north); Inner/Outer bound it on both axes.
struct FQuarter
{
    int32 Index = 0;
    float SX = 1.f, SY = 1.f;
    static constexpr float Inner = PathHalf + PathClear + 40.f, Outer = Half - EdgeClear - 40.f;
    FVector2D At(float U, float V) const { return FVector2D(SX * FMath::Lerp(Inner, Outer, U), SY * FMath::Lerp(Inner, Outer, V)); }
    // The yaw that turns a piece's front toward the nearer of the two paths from P.
    float FacePath(FVector2D P) const
    {
        return FMath::Abs(P.Y) < FMath::Abs(P.X) ? FaceYaw(FVector2D(0.f, -SY)) : FaceYaw(FVector2D(-SX, 0.f));
    }
};

// Adds Item if the rules allow it. Returns whether it was added.
inline bool TryAdd(FPlan& Plan, const FItem& Item)
{
    if (!IsClear(Plan, Item)) return false;
    Plan.Items.Add(Item);
    return true;
}

inline FItem MakeItem(EPiece Piece, FVector2D P, float Yaw, int32 Group, float Scale = 1.f, float Pitch = 0.f, float Roll = 0.f)
{
    FItem I;
    I.Piece = Piece; I.P = P; I.Yaw = Yaw; I.Group = Group; I.Scale = Scale; I.Pitch = Pitch; I.Roll = Roll;
    return I;
}

inline EPiece RandomHeadstone(FRandomStream& Dice, bool Old)
{
    const int32 R = Dice.RandRange(0, 99);
    if (Old && R < 18) return EPiece::HeadstoneBroken;
    return R < 40 ? EPiece::HeadstoneRound : R < 58 ? EPiece::HeadstoneSmall : R < 74 ? EPiece::HeadstoneGothic : R < 90 ? EPiece::HeadstoneCross : EPiece::HeadstoneCeltic;
}

// A block of graves in rows facing the nearer path: a headstone at the head of each grave, a mound (or a ledger
// stone, or an open pit) behind it, now and then roses or candles. OpenShare of the graves are freshly dug.
inline void GravePlot(FPlan& Plan, FRandomStream& Dice, const FQuarter& Q, int32& Group, float OpenShare, bool Old)
{
    for (int32 Try = 0; Try < 12; ++Try)
    {
        const int32 Columns = Dice.RandRange(4, 8), Rows = Dice.RandRange(2, 5);
        // Open pits are wider and longer than mounds, so plots with them are spaced out more.
        const float Across = OpenShare > 0.f ? Dice.FRandRange(235.f, 255.f) : Dice.FRandRange(185.f, 215.f);
        const float Deep = OpenShare > 0.f ? Dice.FRandRange(410.f, 440.f) : Dice.FRandRange(340.f, 380.f);
        const FVector2D Corner = Q.At(Dice.FRandRange(.02f, .75f), Dice.FRandRange(.02f, .75f));
        const float Yaw = Q.FacePath(Corner);
        // Rows run across the facing direction; each row lies one grave further from the path.
        const FVector2D Front = Rotate(FVector2D(0.f, 1.f), Yaw), Side = Rotate(FVector2D(1.f, 0.f), Yaw);
        const FVector2D Away = -Front;
        const int32 Before = Plan.Items.Num();
        const int32 G = ++Group;
        int32 Placed = 0;
        for (int32 Row = 0; Row < Rows; ++Row)
            for (int32 Col = 0; Col < Columns; ++Col)
            {
                if (Dice.FRand() < .18f) continue; // gaps in the rows
                const FVector2D Head = Corner + Side * (Col * Across + Dice.FRandRange(-12.f, 12.f)) + Away * (Row * Deep);
                const bool Open = Dice.FRand() < OpenShare;
                FItem Grave = MakeItem(Open ? EPiece::OpenGrave : Dice.FRand() < .15f ? EPiece::GraveLedger : EPiece::GraveMound,
                    Head + Away * (Open ? 180.f : 125.f), Yaw + Dice.FRandRange(-3.f, 3.f), G);
                const float Lean = Old ? 7.f : 3.f;
                FItem Stone = MakeItem(RandomHeadstone(Dice, Old), Head, Yaw + Dice.FRandRange(-5.f, 5.f), G, Dice.FRandRange(.9f, 1.12f),
                    Dice.FRandRange(-Lean, Lean), Dice.FRandRange(-Lean, Lean));
                if (!IsClear(Plan, Stone) || !IsClear(Plan, Grave)) continue;
                Plan.Items.Add(Stone);
                Plan.Items.Add(Grave);
                ++Placed;
                if (Open && Dice.FRand() < .35f) TryAdd(Plan, MakeItem(EPiece::Bones, Grave.P + Side * 150.f + Away * Dice.FRandRange(-60.f, 60.f), Dice.FRandRange(0.f, 360.f), G));
                else if (!Open && Dice.FRand() < .12f) TryAdd(Plan, MakeItem(EPiece::RosesLaid, Grave.P + Front * 30.f, Yaw + Dice.FRandRange(-20.f, 20.f), G));
                if (Dice.FRand() < .08f) TryAdd(Plan, MakeItem(EPiece::Candles, Head + Front * 45.f + Side * 25.f, Dice.FRandRange(0.f, 360.f), G));
            }
        if (Placed >= Columns) return;
        Plan.Items.SetNum(Before); // too few fitted: try the plot somewhere else
    }
}

inline void Scatter(FPlan& Plan, FRandomStream& Dice, const FQuarter& Q, int32& Group, EPiece Piece, int32 Count, float Scale = 1.f, bool FacePath = false)
{
    for (int32 Try = 0, Placed = 0; Placed < Count && Try < Count * 25; ++Try)
    {
        const FVector2D P = Q.At(Dice.FRand(), Dice.FRand());
        const float Yaw = FacePath ? Q.FacePath(P) + Dice.FRandRange(-8.f, 8.f) : Dice.FRandRange(0.f, 360.f);
        Placed += TryAdd(Plan, MakeItem(Piece, P, Yaw, ++Group, Scale * Dice.FRandRange(.9f, 1.1f)));
    }
}

inline void FillTheme(FPlan& Plan, FRandomStream& Dice, const FQuarter& Q, ETheme Theme, int32& Group)
{
    switch (Theme)
    {
    case ETheme::OldGraves:
    {
        const int32 Plots = Dice.RandRange(3, 4);
        for (int32 I = 0; I < Plots; ++I) GravePlot(Plan, Dice, Q, Group, 0.f, true);
        Scatter(Plan, Dice, Q, Group, EPiece::Obelisk, Dice.RandRange(1, 2), Dice.FRandRange(.9f, 1.25f));
        Scatter(Plan, Dice, Q, Group, EPiece::GhostLantern, 1, 1.f, true);
        break;
    }
    case ETheme::Mausoleums:
    {
        const int32 Count = Dice.RandRange(2, 3);
        for (int32 Try = 0, Placed = 0; Placed < Count && Try < 80; ++Try)
        {
            const FVector2D P = Q.At(Dice.FRandRange(.1f, .9f), Dice.FRandRange(.1f, .9f));
            const float Yaw = Q.FacePath(P);
            const int32 G = ++Group;
            const FItem House = MakeItem(Dice.FRand() < .5f ? EPiece::MausoleumGothic : EPiece::MausoleumDome, P, Yaw, G, Dice.FRandRange(.95f, 1.15f));
            if (!TryAdd(Plan, House)) continue;
            ++Placed;
            const FVector2D Front = Rotate(FVector2D(0.f, 1.f), Yaw), Side = Rotate(FVector2D(1.f, 0.f), Yaw);
            for (float S : {-1.f, 1.f})
                TryAdd(Plan, MakeItem(Dice.FRand() < .6f ? EPiece::GhostLantern : EPiece::StatueMourner, P + Front * (House.Half().Y + 90.f) + Side * S * 230.f, Yaw, G));
        }
        Scatter(Plan, Dice, Q, Group, EPiece::Sarcophagus, Dice.RandRange(1, 3), 1.f, true);
        for (int32 I = 0; I < 10; ++I) Scatter(Plan, Dice, Q, Group, RandomHeadstone(Dice, false), 1, 1.f, true);
        break;
    }
    case ETheme::RoseGarden:
    {
        // A square of hedges around roses and an angel, open toward the path through a rose arch.
        for (int32 Try = 0; Try < 50; ++Try)
        {
            // Odd, so the middle section can be left out as the opening; smaller gardens once the big ones do not fit.
            const int32 Sections = Try < 15 ? (Dice.RandBool() ? 7 : 9) : Try < 35 ? 7 : 5;
            const float Side = Sections * 300.f;
            const FVector2D Centre = Q.At(Dice.FRandRange(.3f, .7f), Dice.FRandRange(.3f, .7f));
            // The garden and the quarter's clearing stay apart (the clearing is for fighting, not inside the hedges).
            if (FVector2D::Distance(Centre, Plan.Clearings[Q.Index]) < Side * .72f + ClearingRadius) continue;
            const float Yaw = Q.FacePath(Centre);
            const int32 G = ++Group, Before = Plan.Items.Num();
            const FVector2D Front = Rotate(FVector2D(0.f, 1.f), Yaw), Across = Rotate(FVector2D(1.f, 0.f), Yaw);
            bool Fits = true;
            for (int32 Edge = 0; Edge < 4 && Fits; ++Edge)
            {
                const FVector2D Out = Edge == 0 ? Front : Edge == 1 ? -Front : Edge == 2 ? Across : -Across;
                const FVector2D Along = FVector2D(-Out.Y, Out.X);
                for (int32 K = 0; K < Sections && Fits; ++K)
                {
                    const float Offset = (K - (Sections - 1) * .5f) * 300.f;
                    if (FMath::Abs(Offset) < 10.f && Edge < 2) continue; // openings front and back
                    Fits = TryAdd(Plan, MakeItem(EPiece::Hedge, Centre + Out * (Side * .5f + 40.f) + Along * Offset,
                        FMath::RadiansToDegrees(FMath::Atan2(Along.Y, Along.X)), G));
                }
            }
            if (!Fits) { Plan.Items.SetNum(Before); continue; }
            TryAdd(Plan, MakeItem(EPiece::RoseArch, Centre + Front * (Side * .5f + 40.f), Yaw, G));
            TryAdd(Plan, MakeItem(EPiece::StatueAngel, Centre, Yaw, G, 1.1f));
            for (int32 K = 0; K < 6; ++K)
            {
                const float A = K * PI / 3.f + .5f;
                TryAdd(Plan, MakeItem(EPiece::RoseBush, Centre + FVector2D(FMath::Cos(A), FMath::Sin(A)) * 280.f, Dice.FRandRange(0.f, 360.f), G, Dice.FRandRange(.9f, 1.2f)));
            }
            // A bed of roses all along the inside of the hedges (not in the openings), and two by the arch.
            for (int32 Edge = 0; Edge < 4; ++Edge)
            {
                const FVector2D Out = Edge == 0 ? Front : Edge == 1 ? -Front : Edge == 2 ? Across : -Across;
                for (float T = -Side * .5f + 150.f; T <= Side * .5f - 150.f; T += Dice.FRandRange(150.f, 190.f))
                {
                    if (Edge < 2 && FMath::Abs(T) < 260.f) continue;
                    TryAdd(Plan, MakeItem(Dice.FRand() < .7f ? EPiece::RoseBush : EPiece::RoseBushSmall, Centre + Out * (Side * .5f - 100.f) + FVector2D(-Out.Y, Out.X) * T,
                        Dice.FRandRange(0.f, 360.f), G, Dice.FRandRange(1.15f, 1.55f)));
                }
            }
            for (float S : {-1.f, 1.f})
                TryAdd(Plan, MakeItem(EPiece::RoseBush, Centre + Front * (Side * .5f + 170.f) + Across * S * 190.f, Dice.FRandRange(0.f, 360.f), G, 1.4f));
            for (int32 K = 0; K < 6; ++K)
            {
                const FVector2D P = Centre + Across * Dice.FRandRange(-.3f, .3f) * Side + Front * Dice.FRandRange(-.3f, .3f) * Side;
                if (TryAdd(Plan, MakeItem(RandomHeadstone(Dice, false), P, Yaw, G)))
                    TryAdd(Plan, MakeItem(EPiece::RosesLaid, P + Front * 50.f, Yaw, G));
            }
            break;
        }
        Scatter(Plan, Dice, Q, Group, EPiece::RoseBushSmall, Dice.RandRange(3, 6));
        Scatter(Plan, Dice, Q, Group, EPiece::GhostLantern, 1, 1.f, true);
        break;
    }
    case ETheme::OpenGraves:
    {
        const int32 Plots = Dice.RandRange(2, 3);
        for (int32 I = 0; I < Plots; ++I) GravePlot(Plan, Dice, Q, Group, .45f, false);
        Scatter(Plan, Dice, Q, Group, EPiece::GhostLantern, 2, 1.f, true);
        Scatter(Plan, Dice, Q, Group, EPiece::Bones, Dice.RandRange(2, 4));
        break;
    }
    case ETheme::Crypts:
    {
        // Sarcophagi in a grid facing the path, mourners at its corners, candles among them.
        for (int32 Try = 0; Try < 40; ++Try)
        {
            // A smaller grid once the bigger ones do not fit.
            const int32 Columns = Try < 20 ? Dice.RandRange(2, 4) : 2, Rows = Try < 20 ? Dice.RandRange(2, 3) : 2;
            const FVector2D Corner = Q.At(Dice.FRandRange(.05f, .6f), Dice.FRandRange(.05f, .6f));
            const float Yaw = Q.FacePath(Corner);
            const FVector2D Front = Rotate(FVector2D(0.f, 1.f), Yaw), Side = Rotate(FVector2D(1.f, 0.f), Yaw);
            const int32 G = ++Group, Before = Plan.Items.Num();
            bool Fits = true;
            for (int32 R = 0; R < Rows && Fits; ++R)
                for (int32 C = 0; C < Columns && Fits; ++C)
                    Fits = TryAdd(Plan, MakeItem(EPiece::Sarcophagus, Corner + Side * (C * 420.f) - Front * (R * 520.f), Yaw, G));
            if (!Fits) { Plan.Items.SetNum(Before); continue; }
            for (int32 R : {0, Rows - 1})
                for (int32 C : {0, Columns - 1})
                    TryAdd(Plan, MakeItem(EPiece::StatueMourner, Corner + Side * (C * 420.f + (C ? 260.f : -260.f)) - Front * (R * 520.f), Yaw, G));
            for (int32 K = 0; K < Rows * Columns; ++K)
                if (Dice.FRand() < .5f)
                    TryAdd(Plan, MakeItem(EPiece::Candles, Corner + Side * ((K % Columns) * 420.f + 90.f) - Front * ((K / Columns) * 520.f - 150.f), Dice.FRandRange(0.f, 360.f), G));
            break;
        }
        Scatter(Plan, Dice, Q, Group, EPiece::GraveLedger, Dice.RandRange(3, 6), 1.f, true);
        Scatter(Plan, Dice, Q, Group, EPiece::Obelisk, 1, 1.1f);
        Scatter(Plan, Dice, Q, Group, EPiece::GhostLantern, 1, 1.f, true);
        break;
    }
    default: // Overgrown: dead trees, toppled and broken stones, a ruined column or two, wild roses.
        Scatter(Plan, Dice, Q, Group, EPiece::DeadTree, Dice.RandRange(5, 8), 1.1f);
        for (int32 I = 0, Tries = 0; I < 14 && Tries < 200; ++Tries)
        {
            const FVector2D P = Q.At(Dice.FRand(), Dice.FRand());
            I += TryAdd(Plan, MakeItem(RandomHeadstone(Dice, true), P, Q.FacePath(P) + Dice.FRandRange(-25.f, 25.f), ++Group,
                Dice.FRandRange(.9f, 1.1f), Dice.FRandRange(-12.f, 12.f), Dice.FRandRange(-12.f, 12.f)));
        }
        Scatter(Plan, Dice, Q, Group, EPiece::BrokenColumn, Dice.RandRange(2, 3));
        Scatter(Plan, Dice, Q, Group, EPiece::RoseBushSmall, Dice.RandRange(3, 5), 1.2f);
        break;
    }
}

// A quarter's theme, then (every quarter is a graveyard first) extra plots of plain graves and lone headstones
// around it: fewest in the rose garden and the overgrown corner.
inline void FillQuarter(FPlan& Plan, FRandomStream& Dice, const FQuarter& Q, ETheme Theme, int32& Group)
{
    FillTheme(Plan, Dice, Q, Theme, Group);
    const bool Sparse = Theme == ETheme::RoseGarden || Theme == ETheme::Overgrown;
    const int32 Extra = Sparse ? 1 : 2;
    for (int32 I = 0; I < Extra; ++I) GravePlot(Plan, Dice, Q, Group, Theme == ETheme::OpenGraves ? .3f : 0.f, Theme == ETheme::OldGraves || Theme == ETheme::Overgrown);
    for (int32 I = 0; I < (Sparse ? 3 : 6); ++I) Scatter(Plan, Dice, Q, Group, RandomHeadstone(Dice, Theme == ETheme::Overgrown), 1, 1.f, true);
}

inline FPlan Make(int32 Seed, int32 Room)
{
    FRandomStream Dice(static_cast<int32>(HashCombine(GetTypeHash(Seed), GetTypeHash(Room * 6271 + 29))));
    FPlan Plan;
    Plan.Seed = Seed;
    Plan.Room = FMath::Clamp(Room, 1, RoomsPerRun);
    // Four different themes for the four quarters.
    TArray<ETheme> Pool;
    for (int32 T = 0; T < static_cast<int32>(ETheme::Count); ++T) Pool.Add(static_cast<ETheme>(T));
    for (int32 I = 0; I < 4; ++I) { const int32 Pick = Dice.RandRange(0, Pool.Num() - 1); Plan.Themes[I] = Pool[Pick]; Pool.RemoveAt(Pick); }
    Plan.Landmark = static_cast<ELandmark>(Dice.RandRange(0, static_cast<int32>(ELandmark::Count) - 1));
    Plan.Mist = Dice.FRandRange(.35f, 1.f);
    Plan.MoonYaw = Dice.FRandRange(-14.f, 14.f);

    FQuarter Quarters[4];
    for (int32 I = 0; I < 4; ++I)
    {
        Quarters[I].Index = I;
        Quarters[I].SX = (I == 1 || I == 3) ? 1.f : -1.f;
        Quarters[I].SY = I < 2 ? 1.f : -1.f;
        // The quarter's open patch, kept free of anything solid.
        Plan.Clearings[I] = Quarters[I].At(Dice.FRandRange(.2f, .8f), Dice.FRandRange(.2f, .8f));
    }
    // The landmark at the crossing (the only solid thing in the plaza).
    const EPiece Mark = Plan.Landmark == ELandmark::DeadOak ? EPiece::DeadOak : Plan.Landmark == ELandmark::Well ? EPiece::Well : EPiece::StatueAngel;
    Plan.Items.Add(MakeItem(Mark, FVector2D::ZeroVector, FaceYaw(FVector2D(-1.f, 0.f)), -1, Mark == EPiece::StatueAngel ? 1.35f : 1.f));
    int32 Group = 0;
    for (int32 I = 0; I < 4; ++I) FillQuarter(Plan, Dice, Quarters[I], Plan.Themes[I], Group);
    return Plan;
}

// Every rule a generated room must satisfy; the graveyard check runs this over many seeds.
inline bool IsFair(const FPlan& Plan, FString* Why = nullptr)
{
    auto Fail = [&](const FString& Reason) { if (Why) *Why = Reason; return false; };
    for (int32 I = 0; I < 4; ++I)
        for (int32 J = I + 1; J < 4; ++J)
            if (Plan.Themes[I] == Plan.Themes[J]) return Fail(TEXT("two quarters share a theme"));
    int32 Solid[4] = {};
    for (const FItem& Item : Plan.Items)
    {
        FString Reason;
        if (!IsClear(Plan, Item, &Reason)) return Fail(FString::Printf(TEXT("piece %d at %s: %s"), static_cast<int32>(Item.Piece), *Item.P.ToString(), *Reason));
        if (Item.IsSolid() && Item.Group >= 0) ++Solid[(Item.P.X > 0 ? 1 : 0) + (Item.P.Y > 0 ? 0 : 2)];
    }
    for (int32 I = 0; I < 4; ++I)
        if (Solid[I] < 6) return Fail(FString::Printf(TEXT("quarter %d (%s) is nearly empty (%d solid pieces)"), I, ThemeName(Plan.Themes[I]), Solid[I]));
    if (Plan.Items.Num() > 900) return Fail(TEXT("too many pieces"));
    const FWalkGrid Walk(Plan);
    if (!Walk.CanReach(Exit)) return Fail(TEXT("the crypt cannot be reached"));
    for (int32 I = 0; I < 4; ++I)
        if (!Walk.CanReach(Plan.Clearings[I])) return Fail(FString::Printf(TEXT("clearing %d cannot be reached"), I));
    return true;
}
}
