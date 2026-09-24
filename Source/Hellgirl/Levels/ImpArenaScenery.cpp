// World II dressing from Inferno_World_Free: a cracked infernal floor, braziers, broken columns, giant bones and
// spikes around the rim, colossal knight statues in the lava sea and the Commander's hoard. Added on top of the
// code-built cavern in MapVisuals.cpp (BuildImpArenaDetails), which keeps its own tagged pieces.
#include "Levels/ArenaGameMode.h"
#include "Levels/ForestArt.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"

using namespace ForestArt;

void AArenaGameMode::BuildImpArenaInferno()
{
    UWorld* World = GetWorld();
    FRandomStream Random(66613);
    auto Polar = [](float Degrees, float R) { const float A = FMath::DegreesToRadians(Degrees); return FVector2D(FMath::Cos(A) * R, FMath::Sin(A) * R); };
    auto Facing = [](FVector2D P) { return FMath::RadiansToDegrees(FMath::Atan2(-P.Y, -P.X)); }; // toward the centre

    // The fighting floor: cracked, ember-lit stone instead of a flat colour.
    Disc(World, FVector(0, 0, .8f), 5150.f, 380.f, LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Pack_Bonus/Materials/M_Pack_Bonus_Stone_2.M_Pack_Bonus_Stone_2")));

    // Braziers between the five wave rifts light the inner ring.
    for (int32 I = 0; I < 5; ++I)
    {
        const FVector2D P = Polar(36.f + I * 72.f, 2150.f);
        Stand(World, Inferno(TEXT("Props/SM_Brazier_004")), P, 0.f, Facing(P), .9f, true);
        Fire(World, FVector(P.X, P.Y, 150.f), .6f, TEXT("NS_Stylish_Fire_3"));
        PointGlow(World, FVector(P.X, P.Y, 230.f), FLinearColor(1.f, .35f, .08f), 7000.f, 1100.f);
    }
    // A ring of columns, some broken, to fight around.
    const TCHAR* Columns[] = {TEXT("Decorations/SM_ColumnBig_001"), TEXT("Decorations/SM_ColumnBigBroken_001"), TEXT("Decorations/SM_ColumnBigBroken_002")};
    for (int32 I = 0; I < 10; ++I)
    {
        const FVector2D P = Polar(18.f + I * 36.f, 3700.f + Random.FRandRange(-150.f, 150.f));
        Stand(World, Inferno(Columns[I % 3]), P, 0.f, Random.FRandRange(0.f, 360.f), Random.FRandRange(.65f, .85f), true, 10.f);
    }
    // The rim: giant ribs leaning over the edge, spikes, bones and heaps of rubble.
    for (int32 I = 0; I < 7; ++I)
    {
        const FVector2D P = Polar(I * 51.4f + 12.f, 4800.f);
        if (auto* Rib = Stand(World, Inferno(TEXT("Decorations/SM_Bone_003")), P, 0.f, Facing(P) + 90.f, Random.FRandRange(.75f, 1.f), true, 30.f))
            Rib->AddActorLocalRotation(FRotator(0.f, 0.f, Random.FRandRange(-12.f, 12.f)));
    }
    for (int32 I = 0; I < 14; ++I)
    {
        const FVector2D P = Polar(Random.FRandRange(0.f, 360.f), Random.FRandRange(4300.f, 5050.f));
        Stand(World, Inferno(TEXT("Props/SM_Spike_001_008")), P, 0.f, Random.FRandRange(0.f, 360.f), Random.FRandRange(1.f, 1.8f), true, 5.f);
    }
    const TCHAR* Litter[] = {TEXT("Decorations/SM_Bone_004"), TEXT("Decorations/SM_Bone_006"), TEXT("Decorations/SM_Mound_005"), TEXT("Decorations/SM_Mound_008"), TEXT("Rocks/SM_StoneSmall_001"), TEXT("Rocks/SM_Stone_003")};
    for (int32 I = 0; I < 26; ++I)
    {
        const FVector2D P = Polar(Random.FRandRange(0.f, 360.f), Random.FRandRange(3900.f, 5100.f));
        Stand(World, Inferno(Litter[I % 6]), P, 0.f, Random.FRandRange(0.f, 360.f), Random.FRandRange(.5f, 1.f), false, 8.f);
    }
    // The Commander's hoard behind his entrance.
    Stand(World, Inferno(TEXT("Props/SM_GoldPileBig_001")), FVector2D(0.f, 3550.f), 0.f, 180.f, .8f, true);
    Stand(World, Inferno(TEXT("Props/SM_ChestBig_001")), FVector2D(-420.f, 3450.f), 0.f, 160.f, .7f, true);
    Stand(World, Inferno(TEXT("Props/SM_GoldPileSmall_001")), FVector2D(380.f, 3380.f), 0.f, 20.f, .9f, false);
    for (int32 I = 0; I < 5; ++I)
        Stand(World, Inferno(*FString::Printf(TEXT("Props/SM_Gem_00%d"), I + 1)), FVector2D(Random.FRandRange(-500.f, 500.f), Random.FRandRange(3250.f, 3700.f)), 0.f, Random.FRandRange(0.f, 360.f), .8f, false);
    // Colossal knights stand in the lava sea, watching the arena.
    for (int32 I = 0; I < 4; ++I)
    {
        const FVector2D P = Polar(45.f + I * 90.f, 8200.f);
        Stand(World, Inferno(TEXT("Statues/SM_StatueKnight_002")), P, -1080.f, Facing(P), .5f, false, 200.f);
        PointGlow(World, FVector(P.X, P.Y, -600.f), FLinearColor(1.f, .25f, .05f), 60000.f, 4500.f);
    }
    // Embers drifting over the whole arena.
    for (int32 I = 0; I < 8; ++I)
    {
        const FVector2D P = Polar(I * 45.f + 20.f, 3000.f);
        Embers(World, FVector(P.X, P.Y, 80.f), 2.5f);
    }
}
