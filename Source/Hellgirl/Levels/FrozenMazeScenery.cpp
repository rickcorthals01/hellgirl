// The Frozen Maze's scenery: the cracked frozen floor with the dark lake under the vortex, the ice walls (and the hidden
// blockers along them), the pillars where walls meet, the cave roof hung with icicles, the waterfalls and curtains with
// their pools, the rooms' ice spires, the glow crystals, the Deprived's lairs, drifting mist and the cold, dim light.
// The parts that move (the vortex, the icicle traps, the frozen remains) are placed and run in FrozenMaze.cpp.
// Art: Tools/Environment/build_maze_kit.ps1. Layout: Tools/Maze (Rules/FrozenMazeLayout.h).
#include "Levels/ArenaGameMode.h"
#include "Levels/FrozenMazeArt.h"
#include "Levels/ForestArt.h"
#include "Levels/GraveyardArt.h"
#include "Levels/MapPieces.h"
#include "Rules/FrozenMazeRules.h"
#include "ProceduralMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/DirectionalLight.h"
#include "Engine/PointLight.h"
#include "Engine/SpotLight.h"
#include "Engine/SkyLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace MazeArt
{
UStaticMesh* Kit(const TCHAR* Name)
{
    return LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/Environment/Maze/Kit/%s.%s"), Name, Name));
}

UMaterialInterface* Material(const TCHAR* Name)
{
    return LoadObject<UMaterialInterface>(nullptr, *FString::Printf(TEXT("/Game/Environment/Maze/%s.%s"), Name, Name));
}

APointLight* Light(UWorld* World, FVector Where, FLinearColor Color, float Intensity, float Radius, bool Shadows)
{
    APointLight* Light = ForestArt::PointGlow(World, Where, Color, Intensity, Radius);
    if (Light) Light->PointLightComponent->SetCastShadows(Shadows);
    return Light;
}

AActor* Sheet(UWorld* World, FVector2D Min, FVector2D Max, float Z, bool FaceUp, UMaterialInterface* Material, float TileSize)
{
    auto* Actor = World->SpawnActor<AActor>();
    auto* Mesh = NewObject<UProceduralMeshComponent>(Actor);
    Actor->SetRootComponent(Mesh);
    Actor->AddInstanceComponent(Mesh);
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Mesh->RegisterComponent();
    TArray<FVector> Vertices, Normals;
    TArray<FVector2D> UVs;
    for (const FVector2D& C : {FVector2D(Min.X, Min.Y), FVector2D(Max.X, Min.Y), FVector2D(Max.X, Max.Y), FVector2D(Min.X, Max.Y)})
    {
        Vertices.Add(FVector(C.X, C.Y, Z));
        Normals.Add(FaceUp ? FVector::UpVector : FVector::DownVector);
        UVs.Add(C / TileSize);
    }
    // 0-2-1 / 0-3-2 faces up (like the graveyard's mist sheets); the other winding faces down.
    Mesh->CreateMeshSection(0, Vertices, FaceUp ? TArray<int32>{0, 2, 1, 0, 3, 2} : TArray<int32>{0, 1, 2, 0, 2, 3}, Normals, UVs, {}, {}, false);
    if (Material) Mesh->SetMaterial(0, Material);
    return Actor;
}

UMaterialInstanceDynamic* Waterfall(UWorld* World, FVector2D From, FVector2D To, FVector2D Out, float Top, float Bulge, float Glow, float Opacity, float Speed)
{
    auto* Actor = World->SpawnActor<AActor>();
    auto* Mesh = NewObject<UProceduralMeshComponent>(Actor);
    Actor->SetRootComponent(Mesh);
    Actor->AddInstanceComponent(Mesh);
    Actor->Tags.Add(TEXT("MazeWater"));
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Mesh->SetCastShadow(false);
    Mesh->RegisterComponent();
    const float Width = FVector2D::Distance(From, To);
    const int32 Across = FMath::Clamp(FMath::CeilToInt(Width / 150.f), 2, 64), Down = 28;
    TArray<FVector> Vertices, Normals;
    TArray<FVector2D> UVs;
    TArray<int32> Triangles;
    for (int32 D = 0; D <= Down; ++D)
    {
        const float S = static_cast<float>(D) / Down;
        // Straight down from the roof, bowing out over the last few metres where the water spreads at the foot.
        const float OutBy = Bulge * FMath::Pow(S, 4.f);
        for (int32 A = 0; A <= Across; ++A)
        {
            const float U = static_cast<float>(A) / Across;
            const FVector2D P = FMath::Lerp(From, To, U) + Out * (OutBy + 14.f * FMath::Sin(U * 17.f + S * 3.f));
            Vertices.Add(FVector(P.X, P.Y, Top * (1.f - S)));
            Normals.Add(FVector(Out.X, Out.Y, 0.f));
            UVs.Add(FVector2D(U, S));
        }
    }
    const int32 Row = Across + 1;
    for (int32 D = 0; D < Down; ++D)
        for (int32 A = 0; A < Across; ++A)
        {
            const int32 I = D * Row + A;
            Triangles.Append({I, I + Row, I + 1, I + 1, I + Row, I + Row + 1});
        }
    Mesh->CreateMeshSection(0, Vertices, Triangles, Normals, UVs, {}, {}, false);
    auto* Water = UMaterialInstanceDynamic::Create(Material(TEXT("M_MazeFall")), Actor);
    if (Water)
    {
        Water->SetScalarParameterValue(TEXT("TileU"), FMath::Max(1.f, Width / 400.f));
        Water->SetScalarParameterValue(TEXT("TileV"), Top / 400.f);
        Water->SetScalarParameterValue(TEXT("Glow"), Glow);
        Water->SetScalarParameterValue(TEXT("Opacity"), Opacity);
        Water->SetScalarParameterValue(TEXT("Speed"), Speed);
        Mesh->SetMaterial(0, Water);
    }
    return Water;
}
}

namespace MazeScenery
{
using namespace FrozenMaze;

uint32 Hash(int32 A, int32 B, int32 C)
{
    return static_cast<uint32>(A * 73856093) ^ static_cast<uint32>(B * 19349663) ^ static_cast<uint32>(C * 83492791);
}

// The grid line leaving grid corner (I, J) in a direction (0 east, 1 west, 2 south, 3 north) is a wall.
bool WallLine(int32 I, int32 J, int32 Direction)
{
    switch (Direction)
    {
    case 0: return I < Columns && (J == 0 || J == Rows || WallSouth(I, J - 1));
    case 1: return I > 0 && (J == 0 || J == Rows || WallSouth(I - 1, J - 1));
    case 2: return J < Rows && (I == 0 || I == Columns || WallEast(I - 1, J));
    default: return J > 0 && (I == 0 || I == Columns || WallEast(I - 1, J - 1));
    }
}

// Deep underground: a faint cold light filtering down through the ice roof (the only thing that casts long shadows),
// thick blue-black fog that swallows the corridors a few turns ahead, and cold grading.
void Cave(UWorld* World)
{
    auto* Roof = World->SpawnActor<ADirectionalLight>(FVector(0, 0, 3000), FRotator(-66.f, 35.f, 0.f));
    auto* RoofLight = CastChecked<UDirectionalLightComponent>(Roof->GetLightComponent());
    RoofLight->SetMobility(EComponentMobility::Movable);
    RoofLight->SetIntensity(.55f);
    RoofLight->SetLightColor(FLinearColor(.45f, .62f, 1.f));
    RoofLight->SetVolumetricScatteringIntensity(.6f);
    RoofLight->SetDynamicShadowDistanceMovableLight(9000.f);
    auto* Sky = World->SpawnActor<ASkyLight>();
    Sky->GetLightComponent()->SetMobility(EComponentMobility::Movable);
    Sky->GetLightComponent()->SetIntensity(.25f);
    Sky->GetLightComponent()->SetLightColor(FLinearColor(.4f, .55f, .9f));
    Sky->GetLightComponent()->SetRealTimeCaptureEnabled(false);
    auto* Fog = World->SpawnActor<AExponentialHeightFog>();
    auto* FogComponent = Fog->GetComponent();
    FogComponent->SetFogDensity(.028f);
    FogComponent->SetFogHeightFalloff(.05f);
    FogComponent->SetFogInscatteringColor(FLinearColor(.018f, .04f, .075f));
    FogComponent->SetStartDistance(300.f);
    FogComponent->SetFogMaxOpacity(1.f);
    FogComponent->SetVolumetricFog(true);
    FogComponent->SetVolumetricFogScatteringDistribution(.6f);
    FogComponent->SetVolumetricFogAlbedo(FColor(165, 195, 235));
    FogComponent->SetVolumetricFogExtinctionScale(.8f);
    auto* Post = World->SpawnActor<APostProcessVolume>();
    Post->bUnbound = true;
    auto& S = Post->Settings;
    S.bOverride_AutoExposureMinBrightness = S.bOverride_AutoExposureMaxBrightness = true;
    S.AutoExposureMinBrightness = S.AutoExposureMaxBrightness = 1.f;
    S.bOverride_BloomIntensity = true; S.BloomIntensity = 1.1f;
    S.bOverride_VignetteIntensity = true; S.VignetteIntensity = .6f;
    S.bOverride_AmbientOcclusionIntensity = true; S.AmbientOcclusionIntensity = .9f;
    S.bOverride_AmbientOcclusionRadius = true; S.AmbientOcclusionRadius = 150.f;
    S.bOverride_ColorGammaShadows = true; S.ColorGammaShadows = FVector4(.92f, .98f, 1.1f, 1.f);
    S.bOverride_ColorGainMidtones = true; S.ColorGainMidtones = FVector4(.95f, 1.f, 1.06f, 1.f);
    S.bOverride_ColorContrast = true; S.ColorContrast = FVector4(1.12f, 1.12f, 1.12f, 1.f);
    S.bOverride_ColorSaturation = true; S.ColorSaturation = FVector4(.9f, .9f, .9f, 1.f);
    S.bOverride_FilmGrainIntensity = true; S.FilmGrainIntensity = .06f;
}

// A pale shaft of light from a crack in the roof, down onto a waterfall (it shows in the fog).
void Shaft(UWorld* World, FVector2D Where, float Intensity)
{
    auto* Spot = World->SpawnActor<ASpotLight>(FVector(Where.X, Where.Y, RoofHeight - 100.f), FRotator(-90.f, 0.f, 0.f));
    if (!Spot) return;
    USpotLightComponent* Light = Spot->SpotLightComponent;
    Light->SetMobility(EComponentMobility::Movable);
    Light->SetInnerConeAngle(8.f);
    Light->SetOuterConeAngle(20.f);
    Light->SetIntensity(Intensity);
    Light->SetAttenuationRadius(3400.f);
    Light->SetLightColor(FLinearColor(.55f, .78f, 1.f));
    Light->SetCastShadows(false);
    Light->SetVolumetricScatteringIntensity(3.f);
}
}

void AArenaGameMode::BuildFrozenMazeScenery()
{
    using namespace FrozenMaze;
    UWorld* World = GetWorld();
    const float HalfX = Columns * Cell * .5f, HalfY = Rows * Cell * .5f;
    const FVector2D Vortex = VortexCentre();
    FRandomStream Dice(7331);

    // Floor: cracked ice everywhere and the dark frozen lake round the vortex (M_MazeFloor works it out in world space).
    if (auto* Floor = UMaterialInstanceDynamic::Create(MazeArt::Material(TEXT("M_MazeFloor")), this))
    {
        Floor->SetVectorParameterValue(TEXT("VortexPosition"), FLinearColor(Vortex.X, Vortex.Y, 0.f));
        Floor->SetScalarParameterValue(TEXT("LakeRadius"), LakeRadius);
        MazeArt::Sheet(World, FVector2D(-HalfX - 600.f, -HalfY - 600.f), FVector2D(HalfX + 600.f, HalfY + 600.f), 0.f, true, Floor, 400.f)->Tags.Add(TEXT("MazeFloor"));
    }

    // Walls: one 8 m ice wall per wall edge (three variants, turned and heightened at random), the outer wall rising to
    // the roof. Each gets a hidden box that does the blocking (smooth for the camera, unlike the jagged ice).
    UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    const TCHAR* WallNames[3] = {TEXT("SM_IceWallA"), TEXT("SM_IceWallB"), TEXT("SM_IceWallC")};
    UHierarchicalInstancedStaticMeshComponent* Walls[3];
    for (int32 I = 0; I < 3; ++I)
    {
        Walls[I] = GraveArt::Batch(World, MazeArt::Kit(WallNames[I]), false, -1.f);
        Walls[I]->GetOwner()->Tags.Add(TEXT("MazeWalls"));
    }
    auto* Blockers = GraveArt::Batch(World, Cube, true, -1.f);
    Blockers->SetHiddenInGame(true);
    Blockers->SetCastShadow(false);
    Blockers->GetOwner()->Tags.Add(TEXT("MazeBlockers"));
    auto AddWall = [&](FVector2D Mid, bool AlongX, bool Outer, uint32 H)
    {
        const float Yaw = (AlongX ? 0.f : 90.f) + (H & 8 ? 180.f : 0.f);
        const float Tall = (Outer ? OuterScale : 1.f) * (.92f + .16f * static_cast<float>((H >> 5) % 101) / 100.f);
        Walls[H % 3]->AddInstance(FTransform(FRotator(0.f, Yaw, 0.f), FVector(Mid.X, Mid.Y, 0.f), FVector(1.f, 1.f, Tall)));
        const FVector Size = AlongX ? FVector(Cell + WallHalf * 2.f, WallHalf * 2.f, 1500.f) : FVector(WallHalf * 2.f, Cell + WallHalf * 2.f, 1500.f);
        Blockers->AddInstance(FTransform(FRotator::ZeroRotator, FVector(Mid.X, Mid.Y, 750.f), Size / 100.f));
    };
    for (int32 Y = 0; Y < Rows; ++Y)
        for (int32 X = 0; X < Columns; ++X)
        {
            if (X < Columns - 1 && WallEast(X, Y)) AddWall(Corner(X + 1.f, Y + .5f), false, false, MazeScenery::Hash(X, Y, 1));
            if (Y < Rows - 1 && WallSouth(X, Y)) AddWall(Corner(X + .5f, Y + 1.f), true, false, MazeScenery::Hash(X, Y, 2));
        }
    for (int32 X = 0; X < Columns; ++X)
    {
        AddWall(Corner(X + .5f, 0.f), true, true, MazeScenery::Hash(X, -1, 3));
        AddWall(Corner(X + .5f, Rows), true, true, MazeScenery::Hash(X, Rows, 3));
    }
    for (int32 Y = 0; Y < Rows; ++Y)
    {
        AddWall(Corner(0.f, Y + .5f), false, true, MazeScenery::Hash(-1, Y, 4));
        AddWall(Corner(Columns, Y + .5f), false, true, MazeScenery::Hash(Columns, Y, 4));
    }
    // Pillars where walls meet, turn or end (a straight run needs none), each with its own blocker.
    auto* Pillars = GraveArt::Batch(World, MazeArt::Kit(TEXT("SM_IcePillar")), false, -1.f);
    for (int32 J = 0; J <= Rows; ++J)
        for (int32 I = 0; I <= Columns; ++I)
        {
            const bool E = MazeScenery::WallLine(I, J, 0), W = MazeScenery::WallLine(I, J, 1), S = MazeScenery::WallLine(I, J, 2), N = MazeScenery::WallLine(I, J, 3);
            const int32 Count = E + W + S + N;
            if (Count == 0 || (Count == 2 && ((E && W) || (N && S)))) continue;
            const bool Outer = I == 0 || J == 0 || I == Columns || J == Rows;
            const uint32 H = MazeScenery::Hash(I, J, 5);
            const FVector2D P = Corner(I, J);
            Pillars->AddInstance(FTransform(FRotator(0.f, static_cast<float>(H % 360), 0.f), FVector(P.X, P.Y, 0.f),
                FVector(1.f, 1.f, (Outer ? OuterScale * .95f : 1.f) * (.95f + .1f * static_cast<float>((H >> 4) % 11) / 10.f))));
            Blockers->AddInstance(FTransform(FRotator::ZeroRotator, FVector(P.X, P.Y, 750.f), FVector(1.9f, 1.9f, 15.f)));
        }
    // Loose ice at the foot of some walls.
    auto* Rubble = GraveArt::Batch(World, MazeArt::Kit(TEXT("SM_IceRubble")), false, -1.f, 9000.f);
    for (int32 Y = 0; Y < Rows; ++Y)
        for (int32 X = 0; X < Columns; ++X)
            for (int32 D = 0; D < 4; ++D)
            {
                if (Open(X, Y, StepX[D], StepY[D]) || Dice.FRand() > .2f || InRoom(VortexRoom, X, Y)) continue;
                const FVector2D Toward(StepX[D], StepY[D]), Along(-Toward.Y, Toward.X);
                const FVector2D P = Centre(X, Y) + Toward * (Cell * .5f - WallHalf - 70.f) + Along * Dice.FRandRange(-280.f, 280.f);
                Rubble->AddInstance(FTransform(FRotator(0.f, Dice.FRandRange(0.f, 360.f), 0.f), FVector(P.X, P.Y, 0.f), FVector(Dice.FRandRange(.8f, 1.5f))));
            }
    for (auto* Batch : {Walls[0], Walls[1], Walls[2], Blockers, Pillars, Rubble}) Batch->BuildTreeIfOutdated(true, true);

    // The cave roof, dark rock hung with icicles far above the walls.
    if (AActor* Roof = MazeArt::Sheet(World, FVector2D(-HalfX - 2000.f, -HalfY - 2000.f), FVector2D(HalfX + 2000.f, HalfY + 2000.f), RoofHeight, false, nullptr, 800.f))
    {
        auto* Mesh = Roof->FindComponentByClass<UProceduralMeshComponent>();
        Mesh->SetCastShadow(false);
        if (auto* Rock = MapPieces::Surface(Roof, FLinearColor(.012f, .016f, .024f))) Mesh->SetMaterial(0, Rock);
    }
    auto* RoofIcicles = GraveArt::Batch(World, MazeArt::Kit(TEXT("SM_RoofIcicles")), false, -1.f);
    RoofIcicles->SetCastShadow(false);
    for (int32 I = 0; I < 480; ++I)
        RoofIcicles->AddInstance(FTransform(FRotator(0.f, Dice.FRandRange(0.f, 360.f), 0.f),
            FVector(Dice.FRandRange(-HalfX, HalfX), Dice.FRandRange(-HalfY, HalfY), RoofHeight + 30.f), FVector(Dice.FRandRange(1.f, 2.f))));
    RoofIcicles->BuildTreeIfOutdated(true, true);

    // Waterfalls down the outer wall from cracks in the roof, each into a pool, with a shaft of light on it.
    UMaterialInterface* Pool = MazeArt::Material(TEXT("MI_MazePool"));
    int32 Seed = 1;
    for (const FFall& Fall : Falls)
    {
        const float Y = Fall.bNorth ? 0.f : static_cast<float>(Rows);
        const FVector2D Out(0.f, Fall.bNorth ? 1.f : -1.f);
        const FVector2D From = Corner(Fall.X0, Y) + Out * (WallHalf + 40.f), To = Corner(Fall.X1, Y) + Out * (WallHalf + 40.f);
        const FVector2D Mid = (From + To) * .5f;
        MazeArt::Waterfall(World, From, To, Out, RoofHeight, 170.f, 1.2f, .7f, .9f);
        MazeArt::Waterfall(World, From + Out * 50.f, To + Out * 50.f, Out, RoofHeight, 240.f, .8f, .4f, 1.25f);
        GraveArt::Strip(World, From + Out * 200.f, To + Out * 200.f, 230.f, 500.f, Pool, 3.f, Seed++);
        MazeArt::Light(World, FVector(Mid.X + Out.X * 350.f, Mid.Y + Out.Y * 350.f, 450.f), FLinearColor(.45f, .75f, 1.f), 16000.f, 2600.f, true);
        MazeScenery::Shaft(World, Mid + Out * 250.f, 45000.f);
    }
    // Curtains: water falling from the roof across a gap in a maze wall; she walks straight through.
    for (const FrozenMazeLayout::FCurtain& C : FrozenMazeLayout::Curtains)
    {
        const bool East = C.Side == TEXT('E');
        const FVector2D From = East ? Corner(C.Cell.X + 1.f, C.Cell.Y) : Corner(C.Cell.X, C.Cell.Y + 1.f);
        const FVector2D To = East ? Corner(C.Cell.X + 1.f, C.Cell.Y + C.Length) : Corner(C.Cell.X + C.Length, C.Cell.Y + 1.f);
        const FVector2D Out = East ? FVector2D(1.f, 0.f) : FVector2D(0.f, 1.f);
        MazeArt::Waterfall(World, From - Out * 25.f, To - Out * 25.f, -Out, RoofHeight, 90.f, 1.f, .5f, 1.f);
        MazeArt::Waterfall(World, From + Out * 25.f, To + Out * 25.f, Out, RoofHeight, 90.f, 1.f, .5f, 1.1f);
        GraveArt::Strip(World, From, To, 200.f, 500.f, Pool, 2.5f, Seed++);
        for (const float Side : {-1.f, 1.f})
        {
            const FVector2D At = (From + To) * .5f + Out * Side * 260.f;
            MazeArt::Light(World, FVector(At.X, At.Y, 320.f), FLinearColor(.4f, .78f, 1.f), 5000.f, 1400.f, true);
        }
    }

    // Rooms: ice spires in the corners of the spawn rooms and round the frozen lake; a crystal cluster in each spawn
    // room on the wall facing its door, and a cold light over it.
    UHierarchicalInstancedStaticMeshComponent* Spires[2] = {GraveArt::Batch(World, MazeArt::Kit(TEXT("SM_IceSpireA")), true, -1.f),
        GraveArt::Batch(World, MazeArt::Kit(TEXT("SM_IceSpireB")), true, -1.f)};
    auto Spire = [&](FVector2D P, float Scale) { Spires[Dice.RandRange(0, 1)]->AddInstance(FTransform(FRotator(0.f, Dice.FRandRange(0.f, 360.f), 0.f), FVector(P.X, P.Y, 0.f), FVector(Scale))); };
    auto* Crystals = GraveArt::Batch(World, MazeArt::Kit(TEXT("SM_GlowCrystals")), false, -1.f);
    Crystals->SetCastShadow(false);
    Crystals->GetOwner()->Tags.Add(TEXT("MazeCrystals"));
    auto Crystal = [&](FVector2D Base, FVector2D Into, float Scale)
    {
        // The cluster's front (-Y) turns to face Into.
        const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Into.X, -Into.Y));
        Crystals->AddInstance(FTransform(FRotator(0.f, Yaw, 0.f), FVector(Base.X, Base.Y, 0.f), FVector(Scale)));
        MazeArt::Light(World, FVector(Base.X + Into.X * 170.f, Base.Y + Into.Y * 170.f, 190.f), FLinearColor(.3f, .78f, 1.f), 9000.f, 1700.f, true);
    };
    for (const FRoom& R : SpawnRooms)
    {
        const FVector2D Door = Centre(R.Door);
        for (const FVector2D& C : {Corner(R.X0, R.Y0) + FVector2D(260.f, 260.f), Corner(R.X1, R.Y0) + FVector2D(-260.f, 260.f),
                 Corner(R.X0, R.Y1) + FVector2D(260.f, -260.f), Corner(R.X1, R.Y1) + FVector2D(-260.f, -260.f)})
            if (FVector2D::Distance(C, Door) > 900.f) Spire(C, Dice.FRandRange(.9f, 1.4f));
        const FVector2D Into = FVector2D(R.OutX, R.OutY);
        Crystal(RoomCentre(R) - Into * ((R.OutX ? (R.X1 - R.X0) : (R.Y1 - R.Y0)) * Cell * .5f - WallHalf - 60.f), Into, 1.8f);
        MazeArt::Light(World, FVector(RoomCentre(R).X, RoomCentre(R).Y, 700.f), FLinearColor(.5f, .7f, 1.f), 7000.f, 2400.f, true);
    }
    const FVector2D DoorWay = Centre(VortexRoom.Door) - Vortex;
    for (int32 I = 0; I < 12; ++I)
    {
        const float A = FMath::DegreesToRadians(15.f + 30.f * I);
        const FVector2D Dir(FMath::Cos(A), FMath::Sin(A));
        if (FVector2D::DotProduct(Dir, DoorWay.GetSafeNormal()) > .8f) continue;  // keep the way from the door open
        Spire(Vortex + Dir * FVector2D(3100.f, 2500.f), Dice.FRandRange(1.2f, 1.9f));
    }
    for (const FVector2D& C : {Corner(VortexRoom.X0, VortexRoom.Y0) + FVector2D(350.f, 350.f), Corner(VortexRoom.X0, VortexRoom.Y1) + FVector2D(350.f, -350.f),
             Corner(VortexRoom.X1, VortexRoom.Y1) + FVector2D(-350.f, -350.f)})
        Spire(C, Dice.FRandRange(1.5f, 2.1f));

    // Glow crystals at junctions, against a wall of the junction (the maze's only light between the rooms).
    for (const FCell& C : FrozenMazeLayout::Crystals)
    {
        int32 Side = -1;
        for (int32 D = 0; D < 4 && Side < 0; ++D) if (!Open(C.X, C.Y, StepX[D], StepY[D])) Side = D;
        const FVector2D Wall = Side >= 0 ? FVector2D(StepX[Side], StepY[Side]) : FVector2D(-.7071f, -.7071f);
        Crystal(Centre(C) + Wall * (Cell * .5f - WallHalf - 60.f), -Wall, 1.5f);
    }

    // The Deprived's lairs: shadow pools in dead ends, old bones round them, a faint violet glow.
    auto* Pools = GraveArt::Batch(World, MazeArt::Kit(TEXT("SM_ShadowPool")), false, -1.f);
    Pools->GetOwner()->Tags.Add(TEXT("MazeLairs"));
    UStaticMesh* Bones[3] = {ForestArt::Inferno(TEXT("Decorations/SM_Bone_003")), ForestArt::Inferno(TEXT("Decorations/SM_Bone_004")),
        ForestArt::Inferno(TEXT("Decorations/SM_Bone_006"))};
    for (const FCell& C : FrozenMazeLayout::Lairs)
    {
        const FVector2D P = Centre(C);
        Pools->AddInstance(FTransform(FRotator(0.f, Dice.FRandRange(0.f, 360.f), 0.f), FVector(P.X, P.Y, 1.f), FVector(1.1f)));
        for (int32 K = 0; K < 3; ++K)
        {
            const float A = Dice.FRandRange(0.f, 2.f * PI);
            const FVector2D B = P + FVector2D(FMath::Cos(A), FMath::Sin(A)) * Dice.FRandRange(150.f, 260.f);
            if (Bones[K]) ForestArt::Solid(World, Bones[K], FTransform(FRotator(0.f, Dice.FRandRange(0.f, 360.f), 0.f), FVector(B.X, B.Y, 0.f)), false);
        }
        MazeArt::Light(World, FVector(P.X, P.Y, 70.f), FLinearColor(.45f, .08f, 1.f), 1800.f, 650.f, false);
    }
    Pools->BuildTreeIfOutdated(true, true);
    Spires[0]->BuildTreeIfOutdated(true, true);
    Spires[1]->BuildTreeIfOutdated(true, true);
    Crystals->BuildTreeIfOutdated(true, true);

    // Mist: three drifting layers over the ice, thicker low down.
    const float MistExtent = HalfX + 1000.f;
    GraveArt::MistSheet(World, 20.f, MistExtent, 1700.f, .5f, 1.f, FLinearColor(.12f, .18f, .27f));
    GraveArt::MistSheet(World, 75.f, MistExtent, 1300.f, .32f, 1.4f, FLinearColor(.12f, .18f, .27f));
    GraveArt::MistSheet(World, 170.f, MistExtent, 2100.f, .16f, .8f, FLinearColor(.1f, .15f, .23f));
    MazeScenery::Cave(World);
}
