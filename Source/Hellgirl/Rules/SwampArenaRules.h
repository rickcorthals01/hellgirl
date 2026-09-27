#pragma once
#include "CoreMinimal.h"
#include "Math/RandomStream.h"
#include "Rules/SwampRules.h"

// World II, Stage III: the swamp's boss arena, from the user's sketch "Swamp boss arena.png" (2026-09-27). Fixed, not
// randomised. A 50 x 50 m square of mud ringed by dead trees, crossed by three streams of soul water running from the
// south-west up to the north-east (a thin one, a middle one widening to the east, a wide one in the south). Hellgirl
// arrives over a boardwalk across the southern stream (its far end broken). The crypt entrance in the north-west
// corner is the way out. Zombie arms stand in the streams, four big dark lily pads float where the upper two streams
// bend, two drowned stumps stand in the water and one dead tree on the mud. Like the corridor: the mud holds nothing
// but that tree (and the crypt); the water can hold anything.
// World axes: +X is north (up the sketch), +Y is east (right). The sketch's pixels map onto the square by Sketch().
namespace SwampArena
{
using SwampRoom::EPiece;
using SwampRoom::FItem;
constexpr float Half = 2500.f;             // half the square's side, inside the tree ring
constexpr float ArmReach = SwampRoom::ArmReach;

// The sketch's pixel (in the 2.1x enlargement of the drawing) to the world.
inline FVector2D Sketch(float SX, float SY) { return FVector2D((570.f - SY) * 5.75f, (SX - 580.f) * 5.435f); }
constexpr float SketchWidth = 5.6f;        // a stream's width in sketch pixels to cm

struct FStreamPoint { FVector2D P; float Width; };
// Each stream's centre line from west to east (running on under the tree ring at both ends), with its width.
inline const TArray<TArray<FStreamPoint>>& Streams()
{
    static const TArray<TArray<FStreamPoint>> All = []
    {
        auto Line = [](std::initializer_list<FVector> Points)
        {
            TArray<FStreamPoint> Out;
            for (const FVector& V : Points) Out.Add({Sketch(static_cast<float>(V.X), static_cast<float>(V.Y)), static_cast<float>(V.Z) * SketchWidth});
            return Out;
        };
        return TArray<TArray<FStreamPoint>>{
            // The thin one, from the west up to the north-east corner.
            Line({{40, 600, 45}, {190, 555, 50}, {240, 562, 45}, {290, 605, 45}, {440, 610, 50}, {530, 540, 55}, {600, 400, 55},
                  {680, 290, 55}, {760, 225, 55}, {880, 180, 50}, {1120, 140, 45}}),
            // The middle one, widening to the east.
            Line({{40, 740, 70}, {215, 668, 75}, {300, 718, 70}, {450, 700, 85}, {560, 620, 95}, {650, 500, 100}, {760, 390, 120},
                  {880, 330, 170}, {1120, 340, 200}}),
            // The wide one in the south, under the boardwalk.
            Line({{40, 910, 95}, {300, 895, 100}, {450, 865, 90}, {560, 860, 100}, {680, 810, 110}, {780, 745, 100}, {900, 680, 85},
                  {1120, 630, 80}})};
    }();
    return All;
}

// How far P is inside the water (negative: outside, on the mud), and the nearest point on a stream's centre line.
inline float Depth(FVector2D P, FVector2D* Nearest = nullptr)
{
    float Best = -MAX_flt;
    for (const TArray<FStreamPoint>& Stream : Streams())
        for (int32 I = 0; I + 1 < Stream.Num(); ++I)
        {
            const FVector2D A = Stream[I].P, B = Stream[I + 1].P;
            const float T = FMath::Clamp(FVector2D::DotProduct(P - A, B - A) / FMath::Max(1.f, (B - A).SizeSquared()), 0.f, 1.f);
            const FVector2D On = A + (B - A) * T;
            const float Inside = FMath::Lerp(Stream[I].Width, Stream[I + 1].Width, T) * .5f - FVector2D::Distance(P, On);
            if (Inside > Best) { Best = Inside; if (Nearest) *Nearest = On; }
        }
    return Best;
}
inline bool InWater(FVector2D P, float Margin = 0.f) { return Depth(P) > Margin; }

// Hellgirl arrives at the south end of the boardwalk, looking north; the crypt's doorway is the way out.
inline const FVector2D Start(-2150.f, Sketch(555, 0).Y);
inline const FVector2D Crypt = Sketch(316, 320);
inline FVector2D CryptFacing() { return (FVector2D(-300.f, 300.f) - Crypt).GetSafeNormal(); }  // toward the arena
inline FVector2D Exit() { return Crypt + CryptFacing() * 330.f; }                             // at the foot of its steps
constexpr float CryptScale = 1.3f;
// Where Stage III's waves come from: the east mud between the lower streams, the south-west mud between the lower
// streams, the north-west mud by the crypt (the ambush); then where the Rat Queen leaps in, in front of the crypt.
constexpr int32 WavePoints = 4;
inline FVector2D WavePoint(int32 Wave)
{
    switch (Wave)
    {
    case 0: return Sketch(720, 600);
    case 1: return Sketch(200, 785);
    case 2: return Sketch(420, 420);
    default: return Crypt + CryptFacing() * 700.f;
    }
}

struct FPlan
{
    TArray<FItem> Items;
    TArray<FVector> Fireflies;
    TArray<FVector2D> Walkway;   // the boardwalk's line, south to north
};

inline FItem Item(EPiece Piece, FVector2D P, float Yaw, float Scale, int32 Group = 0)
{
    FItem I; I.Piece = Piece; I.P = P; I.Yaw = Yaw; I.Scale = Scale; I.Group = Group; return I;
}

inline FPlan Make()
{
    FPlan Plan;
    FRandomStream Dice(2709);
    // Pieces drawn at the water's edge are nudged into the stream.
    auto Wet = [](FVector2D P, float Margin)
    {
        FVector2D Centre;
        for (int32 Step = 0; Step < 20 && Depth(P, &Centre) < Margin; ++Step) P = FMath::Lerp(P, Centre, .35f);
        return P;
    };
    // The boardwalk: from the south mud north across the wide stream; its last two sections broken and askew.
    const float WalkY = Start.Y;
    Plan.Walkway = {FVector2D(Start.X - 120.f, WalkY), FVector2D(-1300.f, WalkY)};
    int32 Section = 0;
    for (float X = Start.X - 100.f; X < -1250.f; X += 290.f, ++Section)
    {
        const bool Broken = X > -1700.f;
        Plan.Items.Add(Item(Broken ? EPiece::BoardwalkBroken : EPiece::Boardwalk, FVector2D(X, WalkY + (Broken ? (Section % 2 ? 45.f : -35.f) : 0.f)),
            Broken ? (Section % 2 ? 9.f : -7.f) : 0.f, 1.f, 1));
    }
    // Zombie arms, where the sketch has them.
    for (const FVector2D& S : {FVector2D(185, 510), FVector2D(440, 550), FVector2D(357, 690), FVector2D(440, 830), FVector2D(822, 735),
                               FVector2D(870, 690), FVector2D(905, 260), FVector2D(965, 330), FVector2D(868, 380)})
        Plan.Items.Add(Item(EPiece::ZombieArm, Wet(Sketch(static_cast<float>(S.X), static_cast<float>(S.Y)), 60.f), Dice.FRandRange(0.f, 360.f), Dice.FRandRange(.9f, 1.1f)));
    // Four big dark lily pads where the upper streams bend: wide enough to stand and fight on.
    for (const FVector2D& S : {FVector2D(563, 468), FVector2D(502, 532), FVector2D(507, 605), FVector2D(636, 593)})
        Plan.Items.Add(Item(EPiece::GiantLily, Wet(Sketch(static_cast<float>(S.X), static_cast<float>(S.Y)), 40.f), Dice.FRandRange(0.f, 360.f), .62f, 2));
    // Two drowned stumps (a big one in the middle stream) and the one dead tree on the mud.
    Plan.Items.Add(Item(EPiece::DrownedStump, Wet(Sketch(718, 265), 40.f), 30.f, 1.1f));
    Plan.Items.Add(Item(EPiece::DrownedStump, Wet(Sketch(258, 730), 60.f), 200.f, 1.9f));
    Plan.Items.Add(Item(EPiece::SwampTreeB, Sketch(877, 547), 60.f, 1.2f));
    // Dressing in the streams only: reeds near the banks and small lily pads, kept off the boardwalk, the arms and the
    // big pads.
    auto Clear = [&](FVector2D P, float Radius)
    {
        if (FMath::Abs(P.X) > Half - 250.f || FMath::Abs(P.Y) > Half - 250.f) return false;
        if (FMath::Abs(P.Y - WalkY) < 180.f + Radius && P.X < -1150.f) return false;
        for (const FItem& Other : Plan.Items)
        {
            const float Apart = FVector2D::Distance(P, Other.P);
            if (Other.Piece == EPiece::ZombieArm && Apart < ArmReach + Radius) return false;
            if (Other.Piece != EPiece::ZombieArm && Apart < Other.Half().Size() + Radius) return false;
        }
        return true;
    };
    auto Scatter = [&](EPiece Piece, int32 Count, float MinDepth, float MaxDepth, float MinScale, float MaxScale)
    {
        for (int32 Try = 0, Placed = 0; Placed < Count && Try < Count * 60; ++Try)
        {
            const FVector2D P(Dice.FRandRange(-Half, Half), Dice.FRandRange(-Half, Half));
            const float D = Depth(P);
            const float Scale = Dice.FRandRange(MinScale, MaxScale);
            if (D < MinDepth || D > MaxDepth || !Clear(P, SwampRoom::Footprint(Piece).Half.Size() * Scale)) continue;
            Plan.Items.Add(Item(Piece, P, Dice.FRandRange(0.f, 360.f), Scale));
            ++Placed;
        }
    };
    Scatter(EPiece::Reeds, 16, 20.f, 90.f, .8f, 1.3f);
    Scatter(EPiece::LilyPad, 26, 60.f, 10000.f, .8f, 1.3f);
    Scatter(EPiece::LilyPadSmall, 22, 40.f, 10000.f, .8f, 1.3f);
    for (const FVector2D& S : {FVector2D(300, 450), FVector2D(700, 420), FVector2D(850, 850), FVector2D(250, 850), FVector2D(620, 250)})
        Plan.Fireflies.Add(FVector(Sketch(static_cast<float>(S.X), static_cast<float>(S.Y)), Dice.FRandRange(140.f, 240.f)));
    return Plan;
}
}
