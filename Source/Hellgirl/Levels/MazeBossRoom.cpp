// The Frozen Maze's boss room (Rules/MazeBossRules.h): a wide open oval ice cavern beyond the vortex, 100 x 70 m under a
// roof 45 m up. The vortex's charred imprint smoulders in the middle; four huge stalagmites stand round it under their
// stalactites; frozen bodies hold Souls (smashed like the maze's frozen remains); purple lightning crawls along the walls,
// arcs across the roof and jumps between each stalactite and the stalagmite below it. URL option MazeBoss=1. Room only
// for now: the boss (who will already be waiting across the imprint) and the stages that lead here come later.
#include "Levels/ArenaGameMode.h"
#include "Levels/ForestArt.h"
#include "Levels/FrozenMazeArt.h"
#include "Levels/GraveyardArt.h"
#include "Levels/MapPieces.h"
#include "Rules/MazeBossRules.h"
#include "Fighter/ArenaFighter.h"
#include "Progress/CoinPickup.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/PointLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "ProceduralMeshComponent.h"
#include "UnrealClient.h"
#if WITH_EDITOR
#include "ShaderCompiler.h"
#endif

namespace MazeBossArt
{
using namespace MazeBoss;

// Yaw that turns a kit piece's front (-Y) to face Dir.
float FaceYaw(FVector2D Dir) { return FMath::RadiansToDegrees(FMath::Atan2(Dir.X, -Dir.Y)); }
// The kit spire is 7.5 m tall: where a stalagmite of this scale ends, and how big its stalactite must be to leave a
// 6 m gap for the lightning between their points.
float SpireTip(float Scale) { return 750.f * Scale - 30.f; }
float StalactiteScale(float Scale) { return (RoofHeight - SpireTip(Scale) - 600.f) / 750.f; }
}

void AArenaGameMode::TravelToMazeBoss()
{
    UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this)), true, TEXT("MazeBoss=1"));
}

void AArenaGameMode::BuildMazeBoss()
{
    using namespace MazeBoss;
    using namespace MazeBossArt;
    MapTitle = TEXT("WORLD IV / THE FROZEN MAZE / BOSS ROOM");
    MapPlatforms.Add(FVector4(0.f, 0.f, HalfX * 2.f, HalfY * 2.f));
    UWorld* World = GetWorld();
    FRandomStream Dice(4471);
    UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    UStaticMesh* Plane = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));

    // An invisible, flat floor to walk on, and the cracked ice over it. Near the imprint the cracks glow like embers.
    Prop(FVector(0, 0, -80), FVector(HalfX * 2.6f / 100.f, HalfY * 2.6f / 100.f, 1.6f), FLinearColor::Black)->SetActorHiddenInGame(true);
    if (auto* Floor = UMaterialInstanceDynamic::Create(MazeArt::Material(TEXT("M_MazeFloor")), this))
    {
        Floor->SetVectorParameterValue(TEXT("VortexPosition"), FLinearColor(0.f, 0.f, 0.f));
        Floor->SetScalarParameterValue(TEXT("LakeRadius"), 0.f);
        Floor->SetVectorParameterValue(TEXT("VortexColor"), FLinearColor(1.f, .3f, .05f));
        Floor->SetScalarParameterValue(TEXT("VortexGlow"), 1.2f);
        Floor->SetScalarParameterValue(TEXT("VortexReach"), 2300.f);
        MazeArt::Sheet(World, FVector2D(-HalfX * 1.4f, -HalfY * 1.5f), FVector2D(HalfX * 1.4f, HalfY * 1.5f), 0.f, true, Floor, 400.f);
    }
    // The vortex's charred imprint, and the embers' glow.
    if (auto* Imprint = ForestArt::Solid(World, Plane, FTransform(FRotator::ZeroRotator, FVector(0.f, 0.f, 3.f), FVector(ImprintX / 50.f, ImprintY / 50.f, 1.f)), false))
    {
        Imprint->GetStaticMeshComponent()->SetCastShadow(false);
        Imprint->GetStaticMeshComponent()->SetMaterial(0, MazeArt::Material(TEXT("M_MazeImprint")));
        Imprint->Tags.Add(TEXT("MazeImprint"));
    }
    MazeEmberLight = MazeArt::Light(World, FVector(0.f, 0.f, 120.f), FLinearColor(1.f, .4f, .12f), 12000.f, 1900.f, false);

    // The wall: two staggered rings of tall ice walls round the oval (rising almost to the roof), knots of pillars, and
    // a hidden blocker along each piece.
    const TCHAR* WallNames[3] = {TEXT("SM_IceWallA"), TEXT("SM_IceWallB"), TEXT("SM_IceWallC")};
    UHierarchicalInstancedStaticMeshComponent* Walls[3];
    for (int32 I = 0; I < 3; ++I) Walls[I] = GraveArt::Batch(World, MazeArt::Kit(WallNames[I]), false, -1.f);
    auto* Pillars = GraveArt::Batch(World, MazeArt::Kit(TEXT("SM_IcePillar")), false, -1.f);
    auto* Blockers = GraveArt::Batch(World, Cube, true, -1.f);
    Blockers->SetHiddenInGame(true);
    Blockers->SetCastShadow(false);
    Blockers->GetOwner()->Tags.Add(TEXT("MazeBossWall"));
    constexpr int32 Pieces = 40;
    for (int32 Ring = 0; Ring < 2; ++Ring)
        for (int32 K = 0; K < Pieces; ++K)
        {
            const float T = 2.f * PI * (K + Ring * .5f) / Pieces;
            const FVector2D Tangent = FVector2D(-FMath::Sin(T) * HalfX, FMath::Cos(T) * HalfY).GetSafeNormal();
            const FVector2D Out(Tangent.Y, -Tangent.X);
            const FVector2D P = Edge(T) + Out * (60.f + Ring * 150.f);
            const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Tangent.Y, Tangent.X)) + (Dice.FRand() < .5f ? 180.f : 0.f);
            Walls[(K + Ring) % 3]->AddInstance(FTransform(FRotator(0.f, Yaw, 0.f), FVector(P.X, P.Y, 0.f), FVector(1.f, 1.f, Dice.FRandRange(3.2f, 3.8f) + Ring * .2f)));
            if (Ring == 0)
            {
                Blockers->AddInstance(FTransform(FRotator(0.f, Yaw, 0.f), FVector(P.X, P.Y, 750.f), FVector(8.6f, 1.5f, 15.f)));
                if (K % 2 == 0)
                {
                    const FVector2D Joint = Edge(T + PI / Pieces) + Out * 40.f;
                    Pillars->AddInstance(FTransform(FRotator(0.f, Dice.FRandRange(0.f, 360.f), 0.f), FVector(Joint.X, Joint.Y, 0.f), FVector(1.f, 1.f, Dice.FRandRange(3.3f, 3.9f))));
                }
            }
        }
    auto* Rubble = GraveArt::Batch(World, MazeArt::Kit(TEXT("SM_IceRubble")), false, -1.f);
    for (int32 I = 0; I < 70; ++I)
    {
        const FVector2D P = Edge(Dice.FRandRange(0.f, 2.f * PI), Dice.FRandRange(.9f, .97f));
        Rubble->AddInstance(FTransform(FRotator(0.f, Dice.FRandRange(0.f, 360.f), 0.f), FVector(P.X, P.Y, 0.f), FVector(Dice.FRandRange(1.f, 2.2f))));
    }

    // The roof, dark rock hung with icicles.
    if (AActor* Roof = MazeArt::Sheet(World, FVector2D(-HalfX * 1.6f, -HalfY * 1.8f), FVector2D(HalfX * 1.6f, HalfY * 1.8f), RoofHeight, false, nullptr, 800.f))
    {
        auto* Mesh = Roof->FindComponentByClass<UProceduralMeshComponent>();
        Mesh->SetCastShadow(false);
        if (auto* Rock = MapPieces::Surface(Roof, FLinearColor(.012f, .014f, .026f))) Mesh->SetMaterial(0, Rock);
        Roof->Tags.Add(TEXT("MazeBossRoof"));
    }
    auto* RoofIcicles = GraveArt::Batch(World, MazeArt::Kit(TEXT("SM_RoofIcicles")), false, -1.f);
    RoofIcicles->SetCastShadow(false);
    for (int32 I = 0; I < 170; ++I)
    {
        const FVector2D P(Dice.FRandRange(-HalfX, HalfX), Dice.FRandRange(-HalfY, HalfY));
        if (OnFloor(P, -500.f)) RoofIcicles->AddInstance(FTransform(FRotator(0.f, Dice.FRandRange(0.f, 360.f), 0.f), FVector(P.X, P.Y, RoofHeight + 30.f), FVector(Dice.FRandRange(1.4f, 2.6f))));
    }

    // The stalagmites and, hanging over each, its stalactite; ice rubble and two glowing crystals at each foot.
    auto* StalagmiteMeshes = GraveArt::Batch(World, MazeArt::Kit(TEXT("SM_IceSpireA")), true, -1.f);
    StalagmiteMeshes->GetOwner()->Tags.Add(TEXT("MazeBossStalagmites"));
    auto* StalactiteMeshes = GraveArt::Batch(World, MazeArt::Kit(TEXT("SM_IceSpireA")), false, -1.f);
    auto* Crystals = GraveArt::Batch(World, MazeArt::Kit(TEXT("SM_GlowCrystals")), false, -1.f);
    Crystals->SetCastShadow(false);
    auto Crystal = [&](FVector2D Base, FVector2D Into, float Scale, bool Shadows)
    {
        Crystals->AddInstance(FTransform(FRotator(0.f, FaceYaw(Into), 0.f), FVector(Base.X, Base.Y, 0.f), FVector(Scale)));
        MazeArt::Light(World, FVector(Base.X + Into.X * 200.f, Base.Y + Into.Y * 200.f, 220.f), FLinearColor(.3f, .78f, 1.f), 10000.f, 2200.f, Shadows);
    };
    for (const FSpire& S : Stalagmites)
    {
        const float Yaw = Dice.FRandRange(0.f, 360.f);
        StalagmiteMeshes->AddInstance(FTransform(FRotator(0.f, Yaw, 0.f), FVector(S.P.X, S.P.Y, 0.f), FVector(S.Scale)));
        StalactiteMeshes->AddInstance(FTransform(FRotator(180.f, Yaw + 40.f, 0.f), FVector(S.P.X, S.P.Y, RoofHeight + 60.f), FVector(StalactiteScale(S.Scale))));
        for (int32 K = 0; K < 5; ++K)
        {
            const float A = Dice.FRandRange(0.f, 2.f * PI);
            const FVector2D P = S.P + FVector2D(FMath::Cos(A), FMath::Sin(A)) * Dice.FRandRange(300.f, 520.f) * S.Scale / 3.f;
            Rubble->AddInstance(FTransform(FRotator(0.f, Dice.FRandRange(0.f, 360.f), 0.f), FVector(P.X, P.Y, 0.f), FVector(Dice.FRandRange(1.2f, 2.f))));
        }
        const FVector2D ToCentre = (-S.P).GetSafeNormal();
        Crystal(S.P + ToCentre * 330.f * S.Scale / 3.f, ToCentre, 1.6f, false);
    }
    // Glow crystals all round the foot of the wall, facing into the room.
    for (int32 K = 0; K < 14; ++K)
    {
        const float T = 2.f * PI * (K + .3f) / 14.f;
        Crystal(Edge(T, .955f), (-Edge(T)).GetSafeNormal(), 1.8f, true);
    }
    for (auto* Batch : {Walls[0], Walls[1], Walls[2], Pillars, Blockers, Rubble, RoofIcicles, StalagmiteMeshes, StalactiteMeshes, Crystals}) Batch->BuildTreeIfOutdated(true, true);

    // Frozen bodies, turned toward the imprint (smashed for Souls like the maze's frozen remains).
    for (const FVector2D& P : Bodies)
    {
        FMazeRemains Body;
        Body.Where = FVector(P.X, P.Y, 0.f);
        Body.Actor = ForestArt::Solid(World, MazeArt::Kit(TEXT("SM_FrozenRemains")), FTransform(FRotator(0.f, FaceYaw((-P).GetSafeNormal()), 0.f), Body.Where, FVector(1.15f)), true);
        const FVector2D Front = P + (-P).GetSafeNormal() * 230.f;
        Body.Glint = MazeArt::Light(World, FVector(Front.X, Front.Y, 260.f), FLinearColor(.6f, .82f, 1.f), 1600.f, 700.f, false);
        MazeRemains.Add(Body);
    }

    // Lightning. Along the walls: bolts crawling up and across the ice. Across the roof: arcs between the stalactites
    // (and out over the imprint). And between every stalactite and the stalagmite below it.
    UMaterialInterface* BoltMaterial = MazeArt::Material(TEXT("M_MazeBolt"));
    auto AddBolt = [&](FVector From, FVector To, float Width)
    {
        FMazeBolt Bolt;
        Bolt.From = From;
        Bolt.To = To;
        Bolt.Width = Width;
        auto* Actor = World->SpawnActor<AActor>();
        auto* Mesh = NewObject<UProceduralMeshComponent>(Actor);
        Actor->SetRootComponent(Mesh);
        Actor->AddInstanceComponent(Mesh);
        Actor->Tags.Add(TEXT("MazeBolt"));
        Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Mesh->SetCastShadow(false);
        Mesh->RegisterComponent();
        Mesh->SetVisibility(false);
        Bolt.Mesh = Mesh;
        if (auto* Glow = UMaterialInstanceDynamic::Create(BoltMaterial, Actor)) { Bolt.Glow = Glow; Mesh->SetMaterial(0, Glow); }
        Bolt.Light = MazeArt::Light(World, (From + To) * .5f, FLinearColor(.62f, .25f, 1.f), 0.f, 3600.f, false);
        Bolt.Wait = Dice.FRandRange(.2f, BoltWaitMax);
        MazeBolts.Add(Bolt);
    };
    for (int32 K = 0; K < 12; ++K)
    {
        const float T = 2.f * PI * (K + Dice.FRandRange(0.f, .6f)) / 12.f, Span = Dice.FRandRange(.1f, .22f) * (Dice.FRand() < .5f ? -1.f : 1.f);
        const FVector2D A = Edge(T, .975f), B = Edge(T + Span, .975f);
        AddBolt(FVector(A.X, A.Y, Dice.FRandRange(300.f, 1600.f)), FVector(B.X, B.Y, Dice.FRandRange(1200.f, 3400.f)), 30.f);
    }
    const int32 Count = UE_ARRAY_COUNT(Stalagmites);
    for (int32 I = 0; I < Count; ++I)
    {
        const FVector2D P = Stalagmites[I].P, Next = Stalagmites[(I + 1) % Count].P;
        AddBolt(FVector(P.X, P.Y, RoofHeight - 350.f), FVector(Next.X, Next.Y, RoofHeight - 350.f), 45.f);
        AddBolt(FVector(P.X, P.Y, SpireTip(Stalagmites[I].Scale) + 40.f), FVector(P.X, P.Y, RoofHeight + 60.f - 750.f * StalactiteScale(Stalagmites[I].Scale) - 40.f), 35.f);
    }
    AddBolt(FVector(Stalagmites[0].P.X, Stalagmites[0].P.Y, RoofHeight - 400.f), FVector(0.f, -300.f, RoofHeight - 250.f), 45.f);
    AddBolt(FVector(Stalagmites[2].P.X, Stalagmites[2].P.Y, RoofHeight - 400.f), FVector(0.f, 300.f, RoofHeight - 250.f), 45.f);
    // A faint violet glow from high up, so the cavern reads as one great space between the flashes.
    MazeArt::Light(World, FVector(0.f, 0.f, RoofHeight - 800.f), FLinearColor(.45f, .25f, 1.f), 25000.f, 7500.f, false);

    // Mist over the ice and the cave's light, with thinner fog than the maze so the far side of the room shows.
    GraveArt::MistSheet(World, 20.f, HalfX * 1.3f, 1700.f, .45f, 1.f, FLinearColor(.12f, .16f, .27f));
    GraveArt::MistSheet(World, 80.f, HalfX * 1.3f, 1300.f, .28f, 1.4f, FLinearColor(.13f, .15f, .27f));
    GraveArt::MistSheet(World, 180.f, HalfX * 1.3f, 2100.f, .14f, .8f, FLinearColor(.12f, .12f, .25f));
    MazeArt::Cave(World, .012f);

    ExitPosition = FVector(Start.X, Start.Y, 0.f);
    ExitPortal = Prop(FVector(0, 0, -5000), FVector(.4f, 2.8f, 4.f), FLinearColor(.55f, .025f, .9f), true);
    ExitPortal->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    ExitPortal->SetActorHiddenInGame(true);
    TotalSites = 0;
    UE_LOG(LogTemp, Display, TEXT("MAZE BOSS ROOM BUILD: %d bolts, %d frozen bodies"), MazeBolts.Num(), MazeRemains.Num());
}

void AArenaGameMode::StrikeMazeBolt(FMazeBolt& Bolt)
{
    UProceduralMeshComponent* Mesh = Bolt.Mesh.Get();
    if (!Mesh) return;
    const FVector Dir = Bolt.To - Bolt.From;
    const float Length = static_cast<float>(Dir.Size());
    const FVector Along = Dir / FMath::Max(Length, 1.f);
    FVector Side = FVector::CrossProduct(Along, FVector::UpVector);
    if (Side.IsNearlyZero()) Side = FVector::RightVector;
    Side.Normalize();
    const FVector Up = FVector::CrossProduct(Side, Along).GetSafeNormal();
    // A fresh jagged path each strike, with one short fork.
    auto Jagged = [&](FVector From, FVector To, int32 Segments, float Jag)
    {
        TArray<FVector> Path;
        for (int32 I = 0; I <= Segments; ++I)
        {
            const float T = static_cast<float>(I) / Segments, J = (I == 0 || I == Segments) ? 0.f : Jag;
            Path.Add(FMath::Lerp(From, To, T) + Side * FMath::FRandRange(-J, J) + Up * FMath::FRandRange(-J, J));
        }
        return Path;
    };
    const int32 Segments = FMath::Clamp(FMath::RoundToInt(Length / 250.f), 6, 24);
    TArray<TArray<FVector>> Paths = {Jagged(Bolt.From, Bolt.To, Segments, Length * .05f)};
    const FVector ForkStart = Paths[0][Segments / 2];
    Paths.Add(Jagged(ForkStart, ForkStart + (Along + Side * FMath::FRandRange(-.8f, .8f) + Up * FMath::FRandRange(-.5f, .5f)).GetSafeNormal() * Length * .25f, 4, Length * .03f));
    // Each path as two crossed ribbons, so it shows from every side; thinner toward its ends (and thinner for the fork).
    TArray<FVector> Vertices;
    TArray<FVector2D> UVs;
    TArray<int32> Triangles;
    for (int32 PathIndex = 0; PathIndex < Paths.Num(); ++PathIndex)
    {
        const TArray<FVector>& Path = Paths[PathIndex];
        for (const FVector& Across : {Side, Up})
        {
            const int32 Base = Vertices.Num();
            for (int32 I = 0; I < Path.Num(); ++I)
            {
                const float T = static_cast<float>(I) / (Path.Num() - 1);
                const float W = Bolt.Width * (PathIndex ? .6f : 1.f) * (.5f + .5f * FMath::Sin(PI * T));
                Vertices.Add(Path[I] - Across * W);
                Vertices.Add(Path[I] + Across * W);
                UVs.Add(FVector2D(T, 0.f));
                UVs.Add(FVector2D(T, 1.f));
                if (I > 0)
                {
                    const int32 A = Base + (I - 1) * 2;
                    Triangles.Append({A, A + 2, A + 1, A + 1, A + 2, A + 3});
                }
            }
        }
    }
    Mesh->CreateMeshSection(0, Vertices, Triangles, TArray<FVector>(), UVs, {}, {}, false);
    if (APointLight* Light = Bolt.Light.Get()) Light->SetActorLocation((Bolt.From + Bolt.To) * .5f);
    Bolt.Clock = 0.f;
    ++Bolt.Strikes;
}

void AArenaGameMode::TickMazeBoss(float Dt)
{
    using namespace MazeBoss;
    MazeClock += Dt;
    for (FMazeBolt& Bolt : MazeBolts)
    {
        Bolt.Clock += Dt;
        if (Bolt.Clock >= Bolt.Wait)
        {
            StrikeMazeBolt(Bolt);
            Bolt.Wait = BoltFlash + FMath::FRandRange(BoltWaitMin, BoltWaitMax);
        }
        // A flash that flickers as it fades.
        const float K = Bolt.Clock < BoltFlash ? 1.f - Bolt.Clock / BoltFlash : 0.f;
        const float Flash = K * (.55f + .45f * FMath::Abs(FMath::Sin(Bolt.Clock * 95.f)));
        if (UProceduralMeshComponent* Mesh = Bolt.Mesh.Get()) Mesh->SetVisibility(Flash > .01f);
        if (UMaterialInstanceDynamic* Glow = Bolt.Glow.Get()) Glow->SetScalarParameterValue(TEXT("Glow"), 5.f * Flash);
        if (APointLight* Light = Bolt.Light.Get()) Light->PointLightComponent->SetIntensity(40000.f * Flash);
    }
    if (MazeEmberLight) MazeEmberLight->PointLightComponent->SetIntensity(12000.f * (.8f + .2f * FMath::Sin(MazeClock * 3.1f) * FMath::Sin(MazeClock * 1.7f)));
    auto* Hero = Cast<AArenaFighter>(UGameplayStatics::GetPlayerPawn(this, 0));
    if (!Hero || !Hero->IsAlive()) return;
    Objective = TEXT("THE BOSS ROOM / Room preview · the boss comes later");
    Prompt.Empty();
    PromptAction = 0;
    TickMazeRemains(Hero);
    if (Hero->GetActorLocation().Z < -300.f)
    {
        Hero->SetActorLocation(FVector(Start.X, Start.Y, 115.f), false, nullptr, ETeleportType::TeleportPhysics);
        Hero->ResetAfterRecovery();
    }
}

void AArenaGameMode::RunMazeBossCheck(float Dt)
{
#if WITH_DEV_AUTOMATION_TESTS
    using namespace MazeBoss;
    // -HellgirlMazeBossPreview (needs a GPU): photographs the room to Saved/Screenshots/MazeBoss/<shot>.png with every
    // bolt striking: from the start, across the room from high up, the imprint, a stalagmite and its arc, the wall
    // lightning, a frozen body, the room from above (no roof or fog) and the play camera. It logs the average frame time.
    if (FParse::Param(FCommandLine::Get(), TEXT("HellgirlMazeBossPreview")))
    {
        static int32 Shot = 0;
        static float Clock = 0.f, FrameTime = 0.f;
        static int32 Frames = 0;
        static TWeakObjectPtr<ACameraActor> Camera;
#if WITH_EDITOR
        if (GShaderCompilingManager && GShaderCompilingManager->IsCompiling()) return;
#endif
        Clock += Dt;
        auto* PC = UGameplayStatics::GetPlayerController(this, 0);
        auto* Hero = PC ? Cast<AArenaFighter>(PC->GetPawn()) : nullptr;
        if (!PC || !Hero) return;
        if (!Camera.IsValid())
        {
            Camera = GetWorld()->SpawnActor<ACameraActor>();
            Camera->GetCameraComponent()->SetFieldOfView(80.f);
        }
        if (Clock > 3.f) { FrameTime += Dt; ++Frames; }
        if (Clock < 4.f) return;
        struct FView { FVector Eye, Look; };
        const FVector2D M3 = Stalagmites[2].P, R1 = Bodies[0];
        const FView Views[] = {
            {FVector(Start.X - 600.f, Start.Y, 350.f), FVector(0.f, 0.f, 300.f)},
            {FVector(-4200.f, 2600.f, 2800.f), FVector(800.f, -600.f, 600.f)},
            {FVector(0.f, 1600.f, 900.f), FVector(0.f, 0.f, 0.f)},
            {FVector(M3.X - 1700.f, M3.Y + 1500.f, 700.f), FVector(M3.X, M3.Y, 2000.f)},
            {FVector(0.f, 0.f, 600.f), FVector(0.f, -HalfY, 1500.f)},
            {FVector(R1.X + 450.f, R1.Y + 150.f, 260.f), FVector(R1.X, R1.Y, 110.f)},
            {FVector(0.f, 3000.f, 14000.f), FVector(0.f, 0.f, 0.f)}};
        const int32 Overview = UE_ARRAY_COUNT(Views) - 1, PlayView = UE_ARRAY_COUNT(Views);
        if (Shot <= Overview)
        {
            if (Shot == Overview)
            {
                for (TActorIterator<AExponentialHeightFog> It(GetWorld()); It; ++It) It->GetComponent()->SetVisibility(false);
                for (TActorIterator<AActor> It(GetWorld()); It; ++It) if (It->ActorHasTag(TEXT("MazeBossRoof"))) It->SetActorHiddenInGame(true);
            }
            Camera->SetActorLocationAndRotation(Views[Shot].Eye, (Views[Shot].Look - Views[Shot].Eye).Rotation());
            PC->SetViewTarget(Camera.Get());
        }
        else PC->SetViewTarget(Hero);
        const float ShotAt = 5.f + Shot * 1.5f;
        // Every bolt strikes just before the picture.
        static int32 StruckFor = -1;
        if (Clock >= ShotAt - .06f && StruckFor != Shot) { StruckFor = Shot; for (FMazeBolt& Bolt : MazeBolts) Bolt.Wait = Bolt.Clock; }
        if (Clock >= ShotAt)
        {
            const FString Name = Shot == PlayView ? TEXT("Play") : FString::Printf(TEXT("%02d"), Shot);
            FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / FString::Printf(TEXT("Screenshots/MazeBoss/%s.png"), *Name), false, false);
            if (++Shot > PlayView)
            {
                UE_LOG(LogTemp, Display, TEXT("MAZE BOSS PREVIEW: %d shots, average frame %.1f ms"), Shot, Frames ? FrameTime / Frames * 1000.f : 0.f);
                FPlatformMisc::RequestExitWithStatus(false, 0);
            }
        }
        return;
    }
    if (!FParse::Param(FCommandLine::Get(), TEXT("HellgirlMazeBossCheck"))) return;
    static float Clock = 0.f;
    static bool Done = false;
    Clock += Dt;
    if (Done || Clock < 1.5f) return;
    Done = true;
    auto* Hero = Cast<AArenaFighter>(UGameplayStatics::GetPlayerPawn(this, 0));
    bool Passed = bMazeBoss && Hero && SpawnSites.IsEmpty();
    FString Detail = Passed ? TEXT("") : TEXT("not the boss room, or enemies present");
    auto Fail = [&](const FString& Why) { if (Passed) Detail = Why; Passed = false; };
    FCollisionQueryParams Query(SCENE_QUERY_STAT(MazeBossCheck), false, Hero);
    auto Trace = [&](FVector From, FVector To, FHitResult& Hit) { return GetWorld()->LineTraceSingleByChannel(Hit, From, To, ECC_Pawn, Query); };

    // 1. She starts on the ice where she comes out of the vortex, looking toward the imprint and the boss.
    if (Hero && (FVector::Dist2D(Hero->GetActorLocation(), FVector(Start.X, Start.Y, 0.f)) > 150.f || !Hero->GetCharacterMovement()->IsMovingOnGround()))
        Fail(TEXT("Hellgirl is not standing at the start"));
    if (const AController* Controller = Hero ? Hero->GetController() : nullptr; Controller && FMath::Abs(FRotator::NormalizeAxis(Controller->GetControlRotation().Yaw)) > 5.f)
        Fail(TEXT("the camera does not look toward the imprint"));
    // 2. The floor is there everywhere inside the wall, and the wall closes the room all round.
    FHitResult Hit;
    for (int32 I = 0; I < 24 && Passed; ++I)
    {
        const float T = 2.f * PI * I / 24.f;
        for (const float R : {.3f, .6f, .9f})
        {
            const FVector2D P = Edge(T, R);
            if (!Trace(FVector(P.X, P.Y, 500.f), FVector(P.X, P.Y, -300.f), Hit) || FMath::Abs(Hit.ImpactPoint.Z) > 5.f)
                Fail(FString::Printf(TEXT("no level floor at %.0f, %.0f"), P.X, P.Y));
        }
        const FVector2D Far = Edge(T, 1.5f);
        if (!Trace(FVector(0.f, 0.f, 150.f), FVector(Far.X, Far.Y, 150.f), Hit) || !OnFloor(FVector2D(Hit.ImpactPoint), -300.f))
            Fail(FString::Printf(TEXT("a way out of the room toward %.0f degrees"), FMath::RadiansToDegrees(T)));
    }
    // 3. The stalagmites block; the imprint and the boss's place are open.
    for (const FSpire& S : Stalagmites)
        if (Passed && !Trace(FVector(S.P.X - 900.f, S.P.Y, 200.f), FVector(S.P.X + 900.f, S.P.Y, 200.f), Hit))
            Fail(FString::Printf(TEXT("the stalagmite at %.0f, %.0f does not block"), S.P.X, S.P.Y));
    // (The imprint keeps 12 m clear; the boss's place 5 m, with frozen bodies R3 and R4 standing near it.)
    for (const FVector3f& Clear : {FVector3f(0.f, 0.f, 1200.f), FVector3f(static_cast<float>(BossSpot.X), static_cast<float>(BossSpot.Y), 500.f)})
        for (int32 I = 0; I < 8 && Passed; ++I)
        {
            const FVector2D Centre(Clear.X, Clear.Y);
            const FVector2D To = Centre + FVector2D(FMath::Cos(PI * I / 4.f), FMath::Sin(PI * I / 4.f)) * Clear.Z;
            if (Trace(FVector(Centre.X, Centre.Y, 150.f), FVector(To.X, To.Y, 150.f), Hit)) Fail(FString::Printf(TEXT("something blocks near %.0f, %.0f"), Centre.X, Centre.Y));
        }
    // 4. A frozen body smashes when she attacks it, and Souls drop.
    if (Passed && MazeRemains.Num())
    {
        FMazeRemains& Body = MazeRemains[0];
        const FVector2D Out = (-FVector2D(Body.Where)).GetSafeNormal();
        const FRotator Toward(0.f, FMath::RadiansToDegrees(FMath::Atan2(-Out.Y, -Out.X)), 0.f);
        Hero->SetActorLocation(Body.Where + FVector(Out.X, Out.Y, 0.f) * 230.f + FVector(0.f, 0.f, 95.f), false, nullptr, ETeleportType::TeleportPhysics);
        Hero->SetActorRotation(Toward);
        if (AController* Controller = Hero->GetController()) Controller->SetControlRotation(Toward);
        Hero->Attack();
        TickMazeRemains(Hero);
        int32 Pickups = 0;
        for (TActorIterator<ACoinPickup> It(GetWorld()); It; ++It) ++Pickups;
        if (!Body.bSmashed || !Pickups) Fail(TEXT("attacking a frozen body does not smash it"));
    }
    // 5. Every bolt of lightning strikes within a few seconds, and shows only while it flashes.
    for (int32 I = 0; I < 60; ++I) TickMazeBoss(.1f);
    int32 Struck = 0;
    for (const FMazeBolt& Bolt : MazeBolts) Struck += Bolt.Strikes > 0;
    if (MazeBolts.Num() < 20 || Struck < MazeBolts.Num()) Fail(FString::Printf(TEXT("%d of %d bolts struck"), Struck, MazeBolts.Num()));
    if (Passed && MazeBolts.Num())
    {
        FMazeBolt& Bolt = MazeBolts[0];
        Bolt.Wait = Bolt.Clock;
        TickMazeBoss(.01f);
        const bool Shown = Bolt.Mesh.IsValid() && Bolt.Mesh->IsVisible();
        TickMazeBoss(BoltFlash + .05f);
        if (!Shown || (Bolt.Mesh.IsValid() && Bolt.Mesh->IsVisible() && Bolt.Clock < Bolt.Wait)) Fail(TEXT("a bolt does not flash and fade"));
    }

    if (Passed) { UE_LOG(LogTemp, Display, TEXT("MAZE BOSS CHECK PASSED: she starts facing the imprint, the floor and the wall close the room, the stalagmites block, the imprint and the boss's place are open, a frozen body smashes for Souls, all %d bolts strike and fade"), MazeBolts.Num()); }
    else { UE_LOG(LogTemp, Error, TEXT("MAZE BOSS CHECK FAILED: %s"), *Detail); }
    FPlatformMisc::RequestExitWithStatus(false, Passed ? 0 : 1);
#endif
}
