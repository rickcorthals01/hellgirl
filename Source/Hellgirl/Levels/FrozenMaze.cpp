// World IV: the Frozen Maze, the Deprived's icy cave: one big maze of tall ice walls (Rules/FrozenMazeRules.h, designed
// in Tools/Maze). URL options: Maze=1?Spawn=N (0..3: which of the four spawn rooms she arrives in; a new run picks one
// at random). Map only for now: the Deprived come later and will rise from the lairs, one at a time. Walking under an
// icicle trap shakes its icicle loose; an attack on frozen remains smashes them for the Souls inside; stepping into the
// vortex leads back to camp. There is no minimap here: finding the way is the point. Scenery: FrozenMazeScenery.cpp.
#include "Levels/ArenaGameMode.h"
#include "Levels/ForestArt.h"
#include "Levels/FrozenMazeArt.h"
#include "Rules/FrozenMazeRules.h"
#include "Fighter/ArenaFighter.h"
#include "Progress/CoinPickup.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
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
#include "Particles/ParticleSystem.h"
#include "ProceduralMeshComponent.h"
#include "UnrealClient.h"
#if WITH_EDITOR
#include "ShaderCompiler.h"
#endif

namespace MazePlay
{
using namespace FrozenMaze;

UParticleSystem* Effect(const TCHAR* Path) { return LoadObject<UParticleSystem>(nullptr, Path); }
const TCHAR* const IceBurst = TEXT("/Game/Realistic_Starter_VFX_Pack_Vol2/Particles/Hit/P_Ice.P_Ice");
const TCHAR* const IceShatter = TEXT("/Game/Realistic_Starter_VFX_Pack_Vol2/Particles/Destruction/P_Destruction_Glass.P_Destruction_Glass");

// The way out of a dead end (zero if the cell has more than one way).
FVector2D WayOut(FCell C)
{
    FVector2D Out = FVector2D::ZeroVector;
    int32 Ways = 0;
    for (int32 D = 0; D < 4; ++D)
        if (Open(C.X, C.Y, StepX[D], StepY[D])) { Out = FVector2D(StepX[D], StepY[D]); ++Ways; }
    return Ways == 1 ? Out : FVector2D::ZeroVector;
}

// Yaw that turns a kit piece's front (-Y) to face Dir.
float FaceYaw(FVector2D Dir) { return FMath::RadiansToDegrees(FMath::Atan2(Dir.X, -Dir.Y)); }
}

void AArenaGameMode::StartFrozenMaze()
{
    TravelToFrozenMaze(FMath::RandRange(0, 3));
}

void AArenaGameMode::TravelToFrozenMaze(int32 Spawn)
{
    UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this)), true, FString::Printf(TEXT("Maze=1?Spawn=%d"), FMath::Clamp(Spawn, 0, 3)));
}

void AArenaGameMode::BuildFrozenMaze()
{
    using namespace FrozenMaze;
    using namespace MazePlay;
    MazeSpawn = FMath::Clamp(UGameplayStatics::GetIntOption(OptionsString, TEXT("Spawn"), 0), 0, 3);
    MapTitle = TEXT("WORLD IV / THE FROZEN MAZE");
    MapPlatforms.Add(FVector4(0.f, 0.f, Columns * Cell, Rows * Cell));
    UWorld* World = GetWorld();
    FRandomStream Dice(9151);

    // An invisible, flat floor to walk on; the visible ice is in FrozenMazeScenery.cpp.
    Prop(FVector(0, 0, -80), FVector(Columns * Cell * 1.1f / 100.f, Rows * Cell * 1.1f / 100.f, 1.6f), FLinearColor::Black)->SetActorHiddenInGame(true);
    BuildFrozenMazeScenery();

    // The vortex: a turning purple spiral standing over the frozen lake (always square to the camera), its whirl on
    // the ice beneath, and its light.
    const FVector2D V = VortexCentre();
    UStaticMesh* Plane = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
    MazeVortexCard = ForestArt::Solid(World, Plane, FTransform(FRotator::ZeroRotator, FVector(V.X, V.Y, 620.f), FVector(11.f)), false);
    if (MazeVortexCard)
    {
        MazeVortexCard->GetStaticMeshComponent()->SetCastShadow(false);
        MazeVortexCard->GetStaticMeshComponent()->SetMaterial(0, MazeArt::Material(TEXT("M_MazeVortex")));
        MazeVortexCard->Tags.Add(TEXT("MazeVortex"));
    }
    if (auto* Whirl = ForestArt::Solid(World, Plane, FTransform(FRotator::ZeroRotator, FVector(V.X, V.Y, 4.f), FVector(34.f)), false))
    {
        Whirl->GetStaticMeshComponent()->SetCastShadow(false);
        if (auto* Swirl = UMaterialInstanceDynamic::Create(MazeArt::Material(TEXT("M_MazeVortex")), Whirl))
        {
            Swirl->SetScalarParameterValue(TEXT("Glow"), .45f);
            Swirl->SetScalarParameterValue(TEXT("Speed"), .5f);
            Swirl->SetScalarParameterValue(TEXT("Twist"), 3.f);
            Whirl->GetStaticMeshComponent()->SetMaterial(0, Swirl);
        }
    }
    MazeVortexLight = MazeArt::Light(World, FVector(V.X, V.Y, 700.f), FLinearColor(.6f, .2f, 1.f), 60000.f, 5200.f, true);
    MazeArt::Light(World, FVector(V.X, V.Y, 120.f), FLinearColor(.7f, .3f, 1.f), 15000.f, 2600.f, false);
    ExitPosition = FVector(V.X, V.Y, 0.f);
    ExitPortal = Prop(ExitPosition + FVector(0, 0, 200), FVector(.4f, 2.8f, 4.f), FLinearColor(.55f, .025f, .9f), true);
    ExitPortal->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    ExitPortal->SetActorHiddenInGame(true);

    // Icicle traps: an ice bridge across the corridor at the top of its walls, the big icicle hanging from its middle,
    // and the shadow that grows on the ice when it is about to fall (hidden until then).
    UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    UMaterialInterface* ShadowBase = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Environment/Swamp/M_SwampRipple.M_SwampRipple"));
    for (const FCell& C : FrozenMazeLayout::Traps)
    {
        const FVector2D P = Centre(C);
        const bool EastWest = Open(C.X, C.Y, 1, 0) && Open(C.X, C.Y, -1, 0);
        ForestArt::Solid(World, MazeArt::Kit(TEXT("SM_IceBridge")), FTransform(FRotator(0.f, EastWest ? 90.f : 0.f, 0.f), FVector(P.X, P.Y, BridgeHeight)), false);
        FMazeTrap Trap;
        Trap.Where = FVector(P.X, P.Y, 0.f);
        Trap.Icicle = ForestArt::Solid(World, MazeArt::Kit(TEXT("SM_BigIcicle")), FTransform(FRotator(0.f, Dice.FRandRange(0.f, 360.f), 0.f), FVector(P.X, P.Y, BridgeHeight - 50.f)), false);
        if (auto* Shadow = ForestArt::Solid(World, Cylinder, FTransform(FRotator::ZeroRotator, FVector(P.X, P.Y, 2.f), FVector(4.8f, 4.8f, .01f)), false))
        {
            Shadow->GetStaticMeshComponent()->SetCastShadow(false);
            if (auto* Dark = UMaterialInstanceDynamic::Create(ShadowBase, Shadow))
            {
                Dark->SetVectorParameterValue(TEXT("Color"), FLinearColor::Black);
                Dark->SetScalarParameterValue(TEXT("Opacity"), 0.f);
                Shadow->GetStaticMeshComponent()->SetMaterial(0, Dark);
                Trap.ShadowMaterial = Dark;
            }
            Shadow->SetActorHiddenInGame(true);
            Trap.Shadow = Shadow;
        }
        MazeTraps.Add(Trap);
    }

    // Frozen remains: stood back against the closed end of a dead end, facing out, with a faint cold glint.
    for (const FCell& C : FrozenMazeLayout::Remains)
    {
        const FVector2D Out = WayOut(C);
        const FVector2D P = Centre(C) - Out * 170.f;
        FMazeRemains Remains;
        Remains.Where = FVector(P.X, P.Y, 0.f);
        Remains.Actor = ForestArt::Solid(World, MazeArt::Kit(TEXT("SM_FrozenRemains")), FTransform(FRotator(0.f, FaceYaw(Out), 0.f), Remains.Where, FVector(1.1f)), true);
        Remains.Glint = MazeArt::Light(World, FVector(P.X + Out.X * 220.f, P.Y + Out.Y * 220.f, 260.f), FLinearColor(.6f, .82f, 1.f), 1600.f, 700.f, false);
        MazeRemains.Add(Remains);
    }
    TotalSites = 0;
    UE_LOG(LogTemp, Display, TEXT("FROZEN MAZE BUILD: spawn room %s, %d traps, %d remains"), SpawnRoom(MazeSpawn).Name, MazeTraps.Num(), MazeRemains.Num());
}

void AArenaGameMode::TickMazeTraps(float Dt, AArenaFighter* Hero)
{
    using namespace FrozenMaze;
    using namespace MazePlay;
    constexpr float Drop = BridgeHeight - 50.f - 340.f;  // until the 3.4 m icicle's point meets the ice
    for (FMazeTrap& T : MazeTraps)
    {
        AStaticMeshActor* Icicle = T.Icicle.Get();
        AStaticMeshActor* Shadow = T.Shadow.Get();
        if (!Icicle) continue;
        const FVector Hang = T.Where + FVector(0.f, 0.f, BridgeHeight - 50.f);
        T.Clock += Dt;
        if (T.State == 0)
        {
            if (Hero && Hero->IsAlive() && FVector::Dist2D(Hero->GetActorLocation(), T.Where) < TrapTrigger && Hero->GetActorLocation().Z < 400.f)
            {
                T.State = 1;
                T.Clock = 0.f;
                if (Shadow) Shadow->SetActorHiddenInGame(false);
            }
        }
        else if (T.State == 1)
        {
            // It shakes loose while its shadow darkens and grows on the ice.
            const float K = FMath::Clamp(T.Clock / TrapWarning, 0.f, 1.f);
            Icicle->SetActorLocation(Hang + FVector(FMath::Sin(T.Clock * 70.f), FMath::Cos(T.Clock * 55.f), 0.f) * 7.f * K);
            if (UMaterialInstanceDynamic* Dark = T.ShadowMaterial.Get()) Dark->SetScalarParameterValue(TEXT("Opacity"), .15f + .55f * K);
            if (Shadow) Shadow->SetActorScale3D(FVector(4.8f * (.55f + .45f * K), 4.8f * (.55f + .45f * K), .01f));
            if (T.Clock >= TrapWarning) { T.State = 2; T.Clock = 0.f; }
        }
        else if (T.State == 2)
        {
            const float S = FMath::Clamp(T.Clock / TrapFall, 0.f, 1.f);
            Icicle->SetActorLocation(Hang - FVector(0.f, 0.f, Drop * S * S));
            if (S < 1.f) continue;
            // It shatters on the ice: whoever is under it is hurt and thrown back (a dodge or a block still works).
            for (const TCHAR* Path : {IceShatter, IceBurst})
                if (UParticleSystem* Burst = Effect(Path)) UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), Burst, T.Where + FVector(0.f, 0.f, 40.f), FRotator::ZeroRotator, FVector(1.2f));
            if (Hero && Hero->IsAlive() && FVector::Dist2D(Hero->GetActorLocation(), T.Where) < TrapHitRadius && Hero->GetActorLocation().Z < 350.f)
            {
                FVector Away = (Hero->GetActorLocation() - T.Where).GetSafeNormal2D();
                if (Away.IsNearlyZero()) Away = -Hero->GetActorForwardVector();
                const float Before = Hero->Health;
                Hero->ReceiveHit(TrapDamage, Away, 450.f);
                MazeTrapHits += Hero->Health < Before;
            }
            Icicle->SetActorHiddenInGame(true);
            if (Shadow) Shadow->SetActorHiddenInGame(true);
            T.State = 3;
            T.Clock = 0.f;
        }
        else if (T.State == 3)
        {
            if (T.Clock < TrapRegrow) continue;
            // A new icicle grows back on the bridge.
            Icicle->SetActorLocation(Hang);
            Icicle->SetActorScale3D(FVector(.01f));
            Icicle->SetActorHiddenInGame(false);
            T.State = 4;
            T.Clock = 0.f;
        }
        else
        {
            Icicle->SetActorScale3D(FVector(FMath::Clamp(T.Clock / 2.f, .01f, 1.f)));
            if (T.Clock >= 2.f) { T.State = 0; T.Clock = 0.f; }
        }
    }
}

void AArenaGameMode::TickMazeRemains(AArenaFighter* Hero)
{
    using namespace FrozenMaze;
    using namespace MazePlay;
    if (!Hero || !Hero->IsAlive() || Hero->GetAttackClock() <= 0.f) return;
    for (FMazeRemains& R : MazeRemains)
    {
        if (R.bSmashed) continue;
        const FVector To = R.Where - Hero->GetActorLocation();
        if (To.Size2D() > RemainsReach || FVector::DotProduct(Hero->GetActorForwardVector(), To.GetSafeNormal2D()) < .3f) continue;
        // The ice bursts; the Souls frozen in it spill out toward her.
        R.bSmashed = true;
        if (AStaticMeshActor* Block = R.Actor.Get()) { Block->SetActorHiddenInGame(true); Block->SetActorEnableCollision(false); }
        if (APointLight* Glint = R.Glint.Get()) Glint->PointLightComponent->SetVisibility(false);
        for (const TCHAR* Path : {IceShatter, IceBurst})
            if (UParticleSystem* Burst = Effect(Path)) UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), Burst, R.Where + FVector(0.f, 0.f, 110.f), FRotator::ZeroRotator, FVector(1.4f));
        const FVector Drop = R.Where - To.GetSafeNormal2D() * 110.f + FVector(0.f, 0.f, 30.f);
        if (auto* Pickup = GetWorld()->SpawnActor<ACoinPickup>(Drop, FRotator::ZeroRotator)) Pickup->SetAmount(RemainsSouls + FMath::RandRange(0, 6));
        ++MazeRemainsSmashed;
    }
}

bool AArenaGameMode::TickFrozenMaze(float Dt)
{
    using namespace FrozenMaze;
    using namespace MazePlay;
    MazeClock += Dt;
    // The vortex turns square to the camera, breathing slowly; its light flickers with it.
    if (MazeVortexCard)
    {
        if (const APlayerCameraManager* View = UGameplayStatics::GetPlayerCameraManager(this, 0))
        {
            const FVector ToCamera = (View->GetCameraLocation() - MazeVortexCard->GetActorLocation()).GetSafeNormal2D();
            if (!ToCamera.IsNearlyZero()) MazeVortexCard->SetActorRotation(FRotator(0.f, ToCamera.Rotation().Yaw + 90.f, 90.f));
        }
        MazeVortexCard->SetActorScale3D(FVector(11.f * (1.f + .03f * FMath::Sin(MazeClock * 1.3f))));
    }
    if (MazeVortexLight) MazeVortexLight->PointLightComponent->SetIntensity(60000.f * (1.f + .12f * FMath::Sin(MazeClock * 2.3f) + .05f * FMath::Sin(MazeClock * 7.1f)));
    auto* Hero = Cast<AArenaFighter>(UGameplayStatics::GetPlayerPawn(this, 0));
    TickMazeTraps(Dt, Hero);
    if (!Hero || !Hero->IsAlive()) return false;
    Objective = TEXT("THE FROZEN MAZE / Map preview · find the vortex");
    Prompt.Empty();
    PromptAction = 0;
    TickMazeRemains(Hero);
    if (Hero->GetActorLocation().Z < -300.f)
    {
        const FVector2D Start = SpawnPoint(MazeSpawn);
        Hero->SetActorLocation(FVector(Start.X, Start.Y, 115.f), false, nullptr, ETeleportType::TeleportPhysics);
        Hero->ResetAfterRecovery();
    }
    if (FVector::Dist2D(Hero->GetActorLocation(), ExitPosition) > VortexReach) return false;
    // Into the vortex: back to camp (the Ghoul waits beyond it later).
    if (FParse::Param(FCommandLine::Get(), TEXT("HellgirlMazeCheck"))) return true;
    TravelToHub();
    return true;
}

void AArenaGameMode::RunFrozenMazeCheck(float Dt)
{
    using namespace FrozenMaze;
    using namespace MazePlay;
#if WITH_DEV_AUTOMATION_TESTS
    // -HellgirlMazePreview (needs a GPU): photographs the maze to Saved/Screenshots/Maze/<shot>.png: the spawn room, a
    // corridor, a glowing junction, a curtain, a waterfall, the vortex room, a lair, frozen remains, a trap, the whole
    // maze from above (no fog or roof) and the play camera. It logs the average frame time.
    if (FParse::Param(FCommandLine::Get(), TEXT("HellgirlMazePreview")))
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
        TArray<FView> Views;
        auto At = [](FVector2D P, float Z) { return FVector(P.X, P.Y, Z); };
        const FVector2D Start = SpawnPoint(MazeSpawn);
        const FVector2D Facing = FVector2D(FMath::Cos(FMath::DegreesToRadians(SpawnYaw(MazeSpawn))), FMath::Sin(FMath::DegreesToRadians(SpawnYaw(MazeSpawn))));
        Views.Add({At(Start - Facing * 600.f, 380.f), At(Start + Facing * 900.f, 150.f)});
        // The longest straight corridor, seen from one end.
        FCell Best{0, 0};
        int32 BestRun = 0, BestDX = 1, BestDY = 0;
        for (int32 Y = 0; Y < Rows; ++Y)
            for (int32 X = 0; X < Columns; ++X)
                for (int32 D : {0, 2})
                {
                    if (InAnyRoom(X, Y)) continue;
                    int32 Run = 0;
                    while (Open(X + StepX[D] * Run, Y + StepY[D] * Run, StepX[D], StepY[D]) && !InAnyRoom(X + StepX[D] * (Run + 1), Y + StepY[D] * (Run + 1))) ++Run;
                    if (Run > BestRun) { BestRun = Run; Best = {X, Y}; BestDX = StepX[D]; BestDY = StepY[D]; }
                }
        const FVector2D Along(BestDX, BestDY);
        Views.Add({At(Centre(Best) - Along * 200.f, 260.f), At(Centre(Best) + Along * Cell * BestRun, 200.f)});
        // A glowing junction: looking at its crystal from across the junction.
        {
            const FCell C = FrozenMazeLayout::Crystals[4];
            FVector2D Wall(0.f, 0.f);
            for (int32 D = 0; D < 4; ++D) if (!Open(C.X, C.Y, StepX[D], StepY[D])) { Wall = FVector2D(StepX[D], StepY[D]); break; }
            Views.Add({At(Centre(C) - Wall * 700.f, 300.f), At(Centre(C) + Wall * 300.f, 120.f)});
        }
        // A curtain (C2), seen from one side.
        {
            const auto& C = FrozenMazeLayout::Curtains[1];
            const FVector2D Mid = Corner(C.Cell.X + 1.f, C.Cell.Y + C.Length * .5f);
            Views.Add({At(Mid - FVector2D(750.f, 0.f), 280.f), At(Mid, 450.f)});
        }
        // The biggest waterfall (W5), from out in the corridor.
        {
            const FFall& F = Falls[4];
            const FVector2D Mid = Corner((F.X0 + F.X1) * .5f, Rows);
            Views.Add({At(Mid + FVector2D(0.f, -700.f), 300.f), At(Mid, 900.f)});
        }
        // The vortex room, from its door.
        Views.Add({At(Centre(OutsideDoor(VortexRoom)), 450.f), At(VortexCentre(), 350.f)});
        // A lair and frozen remains, each from the way into its dead end; a trap from along its corridor.
        for (const FCell& C : {FrozenMazeLayout::Lairs[2], FrozenMazeLayout::Remains[1]})
        {
            const FVector2D Out = WayOut(C);
            Views.Add({At(Centre(C) + Out * 750.f, 330.f), At(Centre(C), 60.f)});
        }
        {
            const FCell C = FrozenMazeLayout::Traps[0];
            const FVector2D Dir = Open(C.X, C.Y, 1, 0) ? FVector2D(1.f, 0.f) : FVector2D(0.f, 1.f);
            Views.Add({At(Centre(C) + Dir * 750.f, 220.f), At(Centre(C), 650.f)});
        }
        Views.Add({FVector(0.f, 9000.f, 30000.f), FVector(0.f, 0.f, 0.f)});
        const int32 Overview = Views.Num() - 1, PlayView = Views.Num();
        if (Shot <= Overview)
        {
            if (Shot == Overview)
            {
                // Above the roof, with the fog cleared, to see the whole maze.
                for (TActorIterator<AExponentialHeightFog> It(GetWorld()); It; ++It) It->GetComponent()->SetVisibility(false);
                for (TActorIterator<AActor> It(GetWorld()); It; ++It)
                    if (UProceduralMeshComponent* Mesh = It->FindComponentByClass<UProceduralMeshComponent>(); Mesh && Mesh->Bounds.Origin.Z > RoofHeight - 50.f) It->SetActorHiddenInGame(true);
            }
            Camera->SetActorLocationAndRotation(Views[Shot].Eye, (Views[Shot].Look - Views[Shot].Eye).Rotation());
            PC->SetViewTarget(Camera.Get());
        }
        else PC->SetViewTarget(Hero);
        if (Clock >= 5.f + Shot * 1.5f)
        {
            const FString Name = Shot == PlayView ? TEXT("Play") : FString::Printf(TEXT("%02d"), Shot);
            FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / FString::Printf(TEXT("Screenshots/Maze/%s.png"), *Name), false, false);
            if (++Shot > PlayView)
            {
                UE_LOG(LogTemp, Display, TEXT("MAZE PREVIEW: %d shots, average frame %.1f ms"), Shot, Frames ? FrameTime / Frames * 1000.f : 0.f);
                FPlatformMisc::RequestExitWithStatus(false, 0);
            }
        }
        return;
    }
    if (!FParse::Param(FCommandLine::Get(), TEXT("HellgirlMazeCheck"))) return;
    static float Clock = 0.f;
    static bool Done = false;
    Clock += Dt;
    if (Done || Clock < 1.5f) return;
    Done = true;
    auto* Hero = Cast<AArenaFighter>(UGameplayStatics::GetPlayerPawn(this, 0));
    bool Passed = bFrozenMaze && Hero && SpawnSites.IsEmpty();
    FString Detail = Passed ? TEXT("") : TEXT("not the maze, or enemies present");
    auto Fail = [&](const FString& Why) { if (Passed) Detail = Why; Passed = false; };

    // 1. The layout: every square of the maze can be reached from the vortex's door, so every spawn room leads there;
    //    the walls are as exported; curtains open; lairs and remains in dead ends; traps on straight stretches.
    const TArray<int32> Steps = Walk({OutsideDoor(VortexRoom)});
    int32 Unreached = 0, Inner = 0;
    for (int32 Y = 0; Y < Rows; ++Y)
        for (int32 X = 0; X < Columns; ++X)
        {
            Unreached += Steps[Index(X, Y)] < 0;
            Inner += (X < Columns - 1 && WallEast(X, Y)) + (Y < Rows - 1 && WallSouth(X, Y));
        }
    if (Unreached) Fail(FString::Printf(TEXT("%d squares cannot be reached"), Unreached));
    if (Inner != FrozenMazeLayout::InnerWalls) Fail(FString::Printf(TEXT("%d inner walls, the layout says %d"), Inner, FrozenMazeLayout::InnerWalls));
    FString Walks;
    TArray<FCell> SpawnDoors;
    for (const FRoom& R : SpawnRooms)
    {
        const FCell Out = OutsideDoor(R);
        SpawnDoors.Add(Out);
        Walks += FString::Printf(TEXT(" %s %d m"), R.Name, Steps[Index(Out.X, Out.Y)] * static_cast<int32>(Cell / 100.f));
        if (!Open(R.Door.X, R.Door.Y, R.OutX, R.OutY)) Fail(FString::Printf(TEXT("%s's door is walled"), R.Name));
    }
    for (const auto& C : FrozenMazeLayout::Curtains)
        for (int32 K = 0; K < C.Length; ++K)
        {
            const bool East = C.Side == TEXT('E');
            if (!Open(C.Cell.X + (East ? 0 : K), C.Cell.Y + (East ? K : 0), East ? 1 : 0, East ? 0 : 1)) Fail(FString::Printf(TEXT("curtain %s is walled"), C.Name));
        }
    const TArray<int32> FromSpawns = Walk(SpawnDoors);
    for (const FCell& C : FrozenMazeLayout::Lairs)
        if (OpenSides(C.X, C.Y) != 1 || FromSpawns[Index(C.X, C.Y)] < 7) Fail(FString::Printf(TEXT("lair at %d,%d is not a dead end away from the spawn rooms"), C.X, C.Y));
    for (const FCell& C : FrozenMazeLayout::Remains)
        if (OpenSides(C.X, C.Y) != 1) Fail(FString::Printf(TEXT("remains at %d,%d are not in a dead end"), C.X, C.Y));
    for (const FCell& C : FrozenMazeLayout::Traps)
        if (OpenSides(C.X, C.Y) != 2 || Open(C.X, C.Y, 1, 0) != Open(C.X, C.Y, -1, 0)) Fail(FString::Printf(TEXT("trap at %d,%d is not on a straight stretch"), C.X, C.Y));

    // 2. The built maze: one ice wall per wall edge (the outer wall too), each blocked; a trace across a wall stops, one
    //    through a curtain or an open side does not.
    int32 WallPieces = 0, BlockerCount = 0;
    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
    {
        if (auto* Batch = It->FindComponentByClass<UHierarchicalInstancedStaticMeshComponent>())
        {
            if (It->ActorHasTag(TEXT("MazeWalls"))) WallPieces += Batch->GetInstanceCount();
            if (It->ActorHasTag(TEXT("MazeBlockers"))) BlockerCount += Batch->GetInstanceCount();
        }
    }
    const int32 Expected = FrozenMazeLayout::InnerWalls + 2 * (Columns + Rows);
    if (WallPieces != Expected || BlockerCount < Expected) Fail(FString::Printf(TEXT("%d wall pieces and %d blockers for %d walls"), WallPieces, BlockerCount, Expected));
    FCollisionQueryParams Query(SCENE_QUERY_STAT(MazeCheck), false, Hero);
    auto Blocked = [&](FCell A, FCell B)
    {
        FHitResult Hit;
        return GetWorld()->LineTraceSingleByChannel(Hit, FVector(Centre(A).X, Centre(A).Y, 100.f), FVector(Centre(B).X, Centre(B).Y, 100.f), ECC_Pawn, Query);
    };
    int32 WallTraces = 0, OpenTraces = 0;
    for (int32 Y = 1; Y < Rows - 1 && Passed; Y += 3)
        for (int32 X = 1; X < Columns - 2 && Passed; X += 3)
        {
            if (InAnyRoom(X, Y) || InAnyRoom(X + 1, Y)) continue;
            const bool Wall = WallEast(X, Y);
            if (Blocked({X, Y}, {X + 1, Y}) != Wall) Fail(FString::Printf(TEXT("between %d,%d and %d,%d the %s"), X, Y, X + 1, Y, Wall ? TEXT("wall lets her through") : TEXT("open side is blocked")));
            ++(Wall ? WallTraces : OpenTraces);
        }
    const auto& C1 = FrozenMazeLayout::Curtains[0];
    if (Blocked(C1.Cell, {C1.Cell.X + 1, C1.Cell.Y})) Fail(TEXT("curtain C1 blocks"));

    // 3. Hellgirl starts on the ice in her spawn room.
    const FCell StartCell = CellAt(FVector2D(Hero->GetActorLocation()));
    if (!InRoom(SpawnRoom(MazeSpawn), StartCell.X, StartCell.Y) || !Hero->GetCharacterMovement()->IsMovingOnGround()) Fail(TEXT("Hellgirl is not standing in her spawn room"));

    // 4. Frozen remains: an attack in front of them smashes them and Souls drop out.
    if (Passed && MazeRemains.Num())
    {
        FMazeRemains& R = MazeRemains[0];
        const FVector2D Out = WayOut(FrozenMazeLayout::Remains[0]);
        Hero->SetActorLocation(R.Where + FVector(Out.X, Out.Y, 0.f) * 230.f + FVector(0.f, 0.f, 95.f), false, nullptr, ETeleportType::TeleportPhysics);
        Hero->SetActorRotation(FRotator(0.f, FMath::RadiansToDegrees(FMath::Atan2(-Out.Y, -Out.X)), 0.f));
        Hero->Attack();
        if (Hero->GetAttackClock() <= 0.f) Fail(TEXT("the attack did not start"));
        TickMazeRemains(Hero);
        int32 Pickups = 0;
        for (TActorIterator<ACoinPickup> It(GetWorld()); It; ++It) ++Pickups;
        if (Passed && (!R.bSmashed || !Pickups || (R.Actor.IsValid() && R.Actor->GetActorEnableCollision()))) Fail(TEXT("attacking the frozen remains does not smash them"));
    }
    // 5. An icicle trap: standing under it, it shakes loose, falls and hurts her; then it grows back.
    if (Passed && MazeTraps.Num())
    {
        FMazeTrap& T = MazeTraps[0];
        Hero->ResetAfterRecovery();
        Hero->SetActorLocation(T.Where + FVector(0.f, 0.f, 95.f), false, nullptr, ETeleportType::TeleportPhysics);
        const float Before = Hero->Health;
        TickMazeTraps(.05f, Hero);
        if (T.State != 1) Fail(TEXT("walking under the trap does not shake the icicle"));
        for (int32 I = 0; I < 60 && T.State < 3; ++I) TickMazeTraps(.05f, Hero);
        if (Passed && (T.State != 3 || Hero->Health >= Before || !T.Icicle.IsValid() || !T.Icicle->IsHidden())) Fail(TEXT("the icicle does not fall and hurt her"));
        Hero->SetActorLocation(FVector(SpawnPoint(MazeSpawn).X, SpawnPoint(MazeSpawn).Y, 115.f), false, nullptr, ETeleportType::TeleportPhysics);
        TickMazeTraps(TrapRegrow + .1f, Hero);
        TickMazeTraps(2.1f, Hero);
        if (Passed && (T.State != 0 || T.Icicle->IsHidden())) Fail(TEXT("the icicle does not grow back"));
    }
    // 6. The vortex only takes her once she steps into it.
    Hero->SetActorLocation(FVector(SpawnPoint(MazeSpawn).X, SpawnPoint(MazeSpawn).Y, 115.f), false, nullptr, ETeleportType::TeleportPhysics);
    if (Passed && TickFrozenMaze(0.f)) Fail(TEXT("the vortex took her from the spawn room"));
    Hero->SetActorLocation(ExitPosition + FVector(0.f, 0.f, 115.f), false, nullptr, ETeleportType::TeleportPhysics);
    if (Passed && !TickFrozenMaze(0.f)) Fail(TEXT("stepping into the vortex does nothing"));

    if (Passed) { UE_LOG(LogTemp, Display, TEXT("MAZE CHECK PASSED: every square reachable, walks to the vortex:%s; %d walls built and blocked (%d wall and %d open traces), remains smash, the trap falls and grows back, the vortex takes her"),
        *Walks, WallPieces, WallTraces, OpenTraces); }
    else { UE_LOG(LogTemp, Error, TEXT("MAZE CHECK FAILED: %s"), *Detail); }
    FPlatformMisc::RequestExitWithStatus(false, Passed ? 0 : 1);
#endif
}
