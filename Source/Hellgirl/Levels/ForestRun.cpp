// Forest run: a chain of randomised forest rooms (Rules/ForestRoomRules.h decides what is in each room).
// URL options: ForestRun=1?Seed=N?Room=R. Rooms 1-3 are goblin fights, room 4 is the Goblin Queen.
// Health and energy carry over between rooms; dying or finishing the run returns to camp.
#include "Levels/ArenaGameMode.h"
#include "Levels/MapPieces.h"
#include "Levels/ForestArt.h"
#include "Rules/ForestRoomRules.h"
#include "Fighter/ArenaFighter.h"
#include "Enemies/EnemySpawnPoint.h"
#include "Components/StaticMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/PointLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "UnrealClient.h"

namespace
{
FVector ForestAt(FVector2D P, float Z = 0.f) { return FVector(P.X, P.Y, Z); }
}

void AArenaGameMode::StartForestRun()
{
    TravelToForestRoom(FMath::RandRange(1, 999999), 1);
}

void AArenaGameMode::TravelToForestRoom(int32 Seed, int32 Room)
{
    FString Options = FString::Printf(TEXT("ForestRun=1?Seed=%d?Room=%d"), Seed, Room);
    // Moving on to the next room keeps Hellgirl's health and energy; a new run starts fresh.
    if (bForestRun && Room > ForestRoomNumber)
        if (const auto* Player = Cast<AArenaFighter>(UGameplayStatics::GetPlayerPawn(this, 0)); Player && Player->IsAlive())
            Options += FString::Printf(TEXT("?RunHealth=%.1f?CombatEnergy=%.3f"), Player->Health, Player->Energy);
    UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this)), true, Options);
}

void AArenaGameMode::BuildForestRun()
{
    ForestSeed = UGameplayStatics::GetIntOption(OptionsString, TEXT("Seed"), 1);
    ForestRoomNumber = FMath::Clamp(UGameplayStatics::GetIntOption(OptionsString, TEXT("Room"), 1), 1, ForestRoom::RoomsPerRun);
    const ForestRoom::FPlan Plan = ForestRoom::Make(ForestSeed, ForestRoomNumber);
    MapTitle = FString::Printf(TEXT("FOREST RUN / ROOM %d OF %d%s"), ForestRoomNumber, ForestRoom::RoomsPerRun, Plan.bBoss ? TEXT(" / THE QUEEN") : TEXT(""));
    MapPlatforms.Add(FVector4(0.f, 0.f, ForestRoom::Radius * 2.f, ForestRoom::Radius * 2.f));
    UWorld* World = GetWorld();

    // An invisible, flat floor to walk on; the visible ground, trees and plants are in ForestScenery.cpp.
    Prop(FVector(0, 0, -80), FVector(80, 80, 1.6f), FLinearColor::Black)->SetActorHiddenInGame(true);
    BuildForestRunScenery();

    // Cover: realistic boulders, fallen logs and stumps from the kit.
    for (int32 I = 0; I < Plan.Cover.Num(); ++I)
    {
        const ForestRoom::FCover& C = Plan.Cover[I];
        FRandomStream Pick(ForestSeed * 31 + ForestRoomNumber * 7 + I);
        AActor* Piece = nullptr;
        if (C.Type == ForestRoom::ECover::Boulder)
        {
            UStaticMesh* Mesh = ForestArt::Boulder(Pick.RandRange(0, 6));
            if (!Mesh) continue;
            const FBoxSphereBounds B = Mesh->GetBounds();
            const float Scale = ForestRoom::CoverReach(C) * 2.f / (2.f * FMath::Max(B.BoxExtent.X, B.BoxExtent.Y));
            const FVector At = ForestAt(C.Position, -(B.Origin.Z - B.BoxExtent.Z) * Scale - 12.f);
            Piece = ForestArt::Solid(World, Mesh, FTransform(FRotator(0, C.Yaw, 0), At, FVector(Scale)), true);
        }
        else if (C.Type == ForestRoom::ECover::Log)
            Piece = ForestArt::Solid(World, ForestArt::Kit(TEXT("SM_Log")), FTransform(FRotator(0, C.Yaw, 0), ForestAt(C.Position, 38.f * C.Size), FVector(1.1f * C.Size)), true);
        else
            Piece = ForestArt::Solid(World, ForestArt::Kit(TEXT("SM_Stump")), FTransform(FRotator(0, C.Yaw, 0), ForestAt(C.Position, -4.f), FVector(.75f * C.Size)), true);
        if (Piece) Piece->Tags.Add(TEXT("ForestCover"));
    }

    // Thorn patches: a tangle of brambles with glowing berries and a dull red glow; standing in one hurts.
    auto* Brambles = ForestArt::Batch(World, ForestArt::Kit(TEXT("SM_Bramble")), true, 2.f);
    FRandomStream Bramble(ForestSeed + ForestRoomNumber * 101);
    for (const ForestRoom::FThorns& T : Plan.Thorns)
    {
        const int32 Count = FMath::RoundToInt(FMath::Square(T.Radius / 75.f));
        for (int32 I = 0; I < Count; ++I)
        {
            const float A = Bramble.FRandRange(0.f, 2.f * PI), D = FMath::Sqrt(Bramble.FRand()) * T.Radius * .85f;
            Brambles->AddInstance(FTransform(FRotator(0, Bramble.FRandRange(0.f, 360.f), 0),
                ForestAt(T.Position + FVector2D(FMath::Cos(A), FMath::Sin(A)) * D), FVector(Bramble.FRandRange(1.1f, 1.7f))));
        }
        ForestArt::PointGlow(World, ForestAt(T.Position, 60.f), FLinearColor(1.f, .16f, .08f), 1400.f, T.Radius * 1.6f)->Tags.Add(TEXT("ForestThorns"));
        ForestThorns.Add(FVector4(T.Position.X, T.Position.Y, T.Radius, 0.f));
    }
    Brambles->BuildTreeIfOutdated(true, true);

    // Waves come out of goblin burrows; the Goblin Queen waits in the last room.
    if (Plan.bBoss)
    {
        if (auto* Queen = Site(ForestAt(ForestRoom::QueenSpot, 10.f), TEXT("GOBLIN QUEEN"), 1, 0, false, true))
        { Queen->GroundType = EHellgirlEnemyType::GoblinQueen; Queen->Difficulty = 1; Queen->FlyingCount = 0; }
    }
    else
        for (int32 I = 0; I < Plan.Waves.Num(); ++I)
        {
            const ForestRoom::FWave& W = Plan.Waves[I];
            const TCHAR* Style = W.Style == ForestRoom::EWave::Ambush ? TEXT("AMBUSH") : W.Style == ForestRoom::EWave::Elite ? TEXT("ELITES") : TEXT("GOBLINS");
            auto* S = Site(ForestAt(W.Position, 10.f), FString::Printf(TEXT("WAVE %d / %s"), I + 1, Style), W.Count, 0, false);
            if (!S) continue;
            S->GroundType = EHellgirlEnemyType::Goblins;
            S->FlyingCount = 0;
            S->Difficulty = W.Style == ForestRoom::EWave::Elite ? 4 : ForestRoomNumber;
            S->EnemyScale = W.Style == ForestRoom::EWave::Elite ? 1.3f : 1.f;
            S->bInstantGroup = W.Style == ForestRoom::EWave::Ambush;
            S->bGroupGuardsHome = false;
            S->UseBurrow();
        }

    // Rune gateways frame the trail at both ends; a green wisp appears in the exit once the room is clear.
    ForestArt::Solid(World, ForestArt::Kit(TEXT("SM_Gateway")), FTransform(FRotator::ZeroRotator, ForestAt(ForestRoom::Exit + FVector2D(220.f, 0.f), -5.f), FVector(1.1f)), true);
    // At the entrance, standing stones either side of the trail (a lintel there would block the camera).
    for (int32 Side : {-1, 1})
        ForestArt::Solid(World, ForestArt::Kit(TEXT("SM_StandingStone")), FTransform(FRotator(Side * 4.f, Side * 20.f, 0), ForestAt(ForestRoom::Start + FVector2D(-120.f, Side * 380.f), -8.f), FVector(1.15f)), true);
    ForestExitMarker = Prop(ForestAt(ForestRoom::Exit + FVector2D(220.f, 0.f), 190.f), FVector(.32f), FLinearColor(.35f, 1.f, .4f), true);
    ForestExitMarker->SetActorEnableCollision(false);
    if (auto* Glow = MapPieces::Surface(ForestExitMarker, FLinearColor(.4f, 1.f, .45f), false, true))
    {
        Glow->SetScalarParameterValue(TEXT("EmissiveStrength"), 12.f);
        ForestExitMarker->GetStaticMeshComponent()->SetMaterial(0, Glow);
    }
    ForestExitMarker->SetActorHiddenInGame(true);
    ForestExitLight = ForestArt::PointGlow(World, ForestAt(ForestRoom::Exit + FVector2D(150.f, 0.f), 190.f), FLinearColor(.35f, 1.f, .45f), 9000.f, 900.f);
    ForestExitLight->PointLightComponent->SetVisibility(false);
    ForestArt::Fireflies(World, ForestAt(ForestRoom::Exit, 150.f), 1.f);
    ExitPosition = ForestAt(ForestRoom::Exit);
    ExitPortal = Prop(ExitPosition + FVector(0, 0, 200), FVector(.4f, 2.8f, 4.f), FLinearColor(.55f, .025f, .9f), true);
    ExitPortal->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    ExitPortal->SetActorHiddenInGame(true);
    TotalSites = SpawnSites.Num();
}

void AArenaGameMode::ShowForestExit(bool Open)
{
    if (ForestExitMarker) ForestExitMarker->SetActorHiddenInGame(!Open);
    if (ForestExitLight) ForestExitLight->PointLightComponent->SetVisibility(Open);
}

void AArenaGameMode::TickForestRun(float Dt)
{
    auto* Hero = Cast<AArenaFighter>(UGameplayStatics::GetPlayerPawn(this, 0));
    if (!Hero || !Hero->IsAlive()) return;
    auto* PC = Cast<APlayerController>(Hero->GetController());
    ActivatedSites = ClearedSites = EnemiesRemaining = 0;
    for (auto S : SpawnSites)
    {
        ActivatedSites += S->bActivated;
        ClearedSites += S->bCleared;
        EnemiesRemaining += S->LivingEnemies();
    }
    // Waves arrive one after another, with a short breather in between.
    int32 Next = 0;
    while (Next < SpawnSites.Num() && SpawnSites[Next]->bCleared) ++Next;
    if (Next != ArenaWaveIndex) { ArenaWaveIndex = Next; WaveCountdown = Next == 0 ? 2.5f : 3.f; }
    const bool Boss = ForestRoomNumber == ForestRoom::RoomsPerRun;
    Prompt.Empty();
    PromptAction = 0;
    if (Next < SpawnSites.Num())
    {
        auto* Current = SpawnSites[Next].Get();
        if (!Current->bActivated)
        {
            WaveCountdown -= Dt;
            if (WaveCountdown <= 0.f) { Current->bEnabled = true; Current->bActivated = true; }
        }
        Objective = Boss ? (Current->bActivated ? TEXT("Defeat the Goblin Queen") : TEXT("The Goblin Queen approaches"))
            : FString::Printf(TEXT("ROOM %d / WAVE %d OF %d / %s"), ForestRoomNumber, Next + 1, SpawnSites.Num(),
                Current->bActivated ? *Current->SiteName.RightChop(Current->SiteName.Find(TEXT("/")) + 2) : TEXT("Incoming"));
        ShowForestExit(false);
    }
    else if (Boss)
    {
        Objective = TEXT("THE QUEEN IS DEFEATED / Run complete");
        PromptAction = 4;
        Prompt = TEXT("Return to camp? E / D-pad Up: YES");
        if (PC && (PC->WasInputKeyJustPressed(EKeys::E) || PC->WasInputKeyJustPressed(EKeys::Gamepad_DPad_Up))) { RestartMap(); return; }
    }
    else
    {
        Objective = TEXT("ROOM CLEAR / Follow the glowing trail east");
        ShowForestExit(true);
        if (FVector::Dist2D(Hero->GetActorLocation(), ExitPosition) < 300.f && !FParse::Param(FCommandLine::Get(), TEXT("HellgirlForestRunCheck")))
        {
            TravelToForestRoom(ForestSeed, ForestRoomNumber + 1);
            return;
        }
    }
    // Thorns: a small hit every half second while standing in a patch.
    HazardClock = FMath::Max(0.f, HazardClock - Dt);
    for (const FVector4& T : ForestThorns)
        if (HazardClock <= 0.f && Hero->GetCharacterMovement()->IsMovingOnGround()
            && FVector::Dist2D(Hero->GetActorLocation(), FVector(T.X, T.Y, 0.f)) < T.Z)
        {
            Hero->ApplyPhysicsDamage(4.f, FVector::ZeroVector);
            Hero->MoveLabel = TEXT("THORNS!");
            HazardClock = .5f;
        }
    if (Hero->GetActorLocation().Z < -300.f)
    {
        Hero->SetActorLocation(ForestAt(ForestRoom::Start, 110.f), false, nullptr, ETeleportType::TeleportPhysics);
        Hero->ResetAfterRecovery();
    }
}

void AArenaGameMode::RunForestRunCheck()
{
#if WITH_DEV_AUTOMATION_TESTS
    // -HellgirlForestRunPreview (needs a GPU): films the room from above and from the entrance, then quits.
    if (FParse::Param(FCommandLine::Get(), TEXT("HellgirlForestRunPreview")))
    {
        static int32 Shot = 0;
        static float Clock = 0.f;
        static TWeakObjectPtr<ACameraActor> Camera;
        Clock += GetWorld()->GetDeltaSeconds();
        auto* PC = UGameplayStatics::GetPlayerController(this, 0);
        if (!Camera.IsValid() && PC)
        {
            Camera = GetWorld()->SpawnActor<ACameraActor>();
            Camera->GetCameraComponent()->SetFieldOfView(70.f);
        }
        // -PreviewMeshes=/Game/A.A;/Game/B.B lines up those meshes in the clearing, to judge assets in this lighting.
        static bool Lined = false;
        FString MeshList;
        FParse::Value(FCommandLine::Get(), TEXT("PreviewMeshes="), MeshList, false);
        if (!Lined && !MeshList.IsEmpty())
        {
            Lined = true;
            TArray<FString> Paths;
            MeshList.ParseIntoArray(Paths, TEXT(";"));
            float Y = 0.f;
            TArray<TPair<UStaticMesh*, float>> Meshes;
            for (const FString& Path : Paths)
                if (auto* Mesh = LoadObject<UStaticMesh>(nullptr, *Path))
                {
                    const FVector Size = Mesh->GetBounds().BoxExtent * 2.f;
                    const float Scale = FMath::Min(1.f, 420.f / FMath::Max(Size.X, Size.Y));
                    Meshes.Add({Mesh, Scale});
                    Y += FMath::Max(Size.X, Size.Y) * Scale + 80.f;
                }
            float At = -Y * .5f;
            for (auto& [Mesh, Scale] : Meshes)
            {
                const FBoxSphereBounds B = Mesh->GetBounds();
                const float Width = FMath::Max(B.BoxExtent.X, B.BoxExtent.Y) * 2.f * Scale;
                auto* A = GetWorld()->SpawnActor<AStaticMeshActor>(FVector(-600.f, At + Width * .5f, -(B.Origin.Z - B.BoxExtent.Z) * Scale), FRotator::ZeroRotator);
                A->SetMobility(EComponentMobility::Movable);
                A->GetStaticMeshComponent()->SetStaticMesh(Mesh);
                A->SetActorScale3D(FVector(Scale));
                UE_LOG(LogTemp, Display, TEXT("PREVIEW MESH %s: size %s, scale %.2f, %d LODs, nanite %d"), *Mesh->GetName(),
                    *(B.BoxExtent * 2.f).ToString(), Scale, Mesh->GetNumLODs(), Mesh->IsNaniteEnabled() ? 1 : 0);
                At += Width + 80.f;
            }
            // -PreviewMaterials=/Game/M.M;... lays out one 4 m tile per material in front of the meshes.
            FString MaterialList;
            FParse::Value(FCommandLine::Get(), TEXT("PreviewMaterials="), MaterialList, false);
            TArray<FString> MaterialPaths;
            MaterialList.ParseIntoArray(MaterialPaths, TEXT(";"));
            for (int32 I = 0; I < MaterialPaths.Num(); ++I)
                if (auto* Material = LoadObject<UMaterialInterface>(nullptr, *MaterialPaths[I]))
                {
                    auto* Tile = GetWorld()->SpawnActor<AStaticMeshActor>(FVector(-1150.f, (I - (MaterialPaths.Num() - 1) * .5f) * 420.f, 1.f), FRotator::ZeroRotator);
                    Tile->SetMobility(EComponentMobility::Movable);
                    Tile->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane")));
                    Tile->GetStaticMeshComponent()->SetMaterial(0, Material);
                    Tile->SetActorScale3D(FVector(4.f));
                }
        }
        if (!Camera.IsValid() || Clock < 3.f) return;
        const bool Lineup = !MeshList.IsEmpty();
        // Lineup shots: the meshes, a closer angle, then straight down onto the material tiles.
        const FVector Eye = Lineup ? (Shot == 0 ? FVector(-1700.f, 0.f, 420.f) : Shot == 1 ? FVector(-1100.f, -700.f, 250.f) : FVector(-1450.f, 0.f, 1500.f))
            : Shot == 0 ? FVector(-600.f, 0.f, 5200.f) : FVector(-3300.f, 0.f, 900.f);
        const FVector Look = Lineup ? (Shot == 2 ? FVector(-1150.f, 0.f, 0.f) : FVector(-600.f, Shot == 0 ? 0.f : -300.f, 120.f))
            : Shot == 0 ? FVector(0.f, 0.f, 0.f) : FVector(0.f, 0.f, 50.f);
        Camera->SetActorLocationAndRotation(Eye, (Look - Eye).Rotation());
        PC->SetViewTarget(Camera.Get());
        if (Clock >= 4.f + Shot * 1.5f)
        {
            FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / FString::Printf(TEXT("Screenshots/ForestRun/%d_%d_%d.png"), ForestSeed, ForestRoomNumber, Shot), false, false);
            if (++Shot > 2) FPlatformMisc::RequestExitWithStatus(false, 0);
        }
        return;
    }
    if (!FParse::Param(FCommandLine::Get(), TEXT("HellgirlForestRunCheck"))) return;
    static bool Done = false;
    if (Done) return;
    Done = true;
    bool Passed = bForestRun;
    FString Detail;
    // 1. The planner: fair for many seeds, repeatable, and actually varied.
    TSet<int32> CoverCounts, WaveShapes;
    for (int32 Seed = 1; Seed <= 400 && Passed; ++Seed)
        for (int32 Room = 1; Room <= ForestRoom::RoomsPerRun && Passed; ++Room)
        {
            const ForestRoom::FPlan A = ForestRoom::Make(Seed, Room), B = ForestRoom::Make(Seed, Room);
            FString Why;
            if (!ForestRoom::IsFair(A, &Why)) { Passed = false; Detail = FString::Printf(TEXT("seed %d room %d: %s"), Seed, Room, *Why); }
            bool Same = A.Cover.Num() == B.Cover.Num() && A.Waves.Num() == B.Waves.Num() && A.Thorns.Num() == B.Thorns.Num();
            for (int32 I = 0; Same && I < A.Cover.Num(); ++I) Same = A.Cover[I].Position.Equals(B.Cover[I].Position) && A.Cover[I].Type == B.Cover[I].Type;
            if (!Same) { Passed = false; Detail = FString::Printf(TEXT("seed %d room %d is not repeatable"), Seed, Room); }
            CoverCounts.Add(A.Cover.Num());
            int32 Shape = A.Waves.Num();
            for (const auto& W : A.Waves) Shape = Shape * 10 + static_cast<int32>(W.Style);
            WaveShapes.Add(Shape);
        }
    if (Passed && (CoverCounts.Num() < 4 || WaveShapes.Num() < 6)) { Passed = false; Detail = TEXT("rooms are not varied enough"); }

    // 2. The built room matches its plan.
    const ForestRoom::FPlan Plan = ForestRoom::Make(ForestSeed, ForestRoomNumber);
    int32 Cover = 0, Thorns = 0, Walls = 0;
    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
    {
        Cover += It->ActorHasTag(TEXT("ForestCover"));
        Thorns += It->ActorHasTag(TEXT("ForestThorns"));
        Walls += It->ActorHasTag(TEXT("ForestBoundary"));
    }
    if (Passed && (Cover != Plan.Cover.Num() || Thorns != Plan.Thorns.Num() || Walls != 56 || SpawnSites.Num() != (Plan.bBoss ? 1 : Plan.Waves.Num())))
    { Passed = false; Detail = FString::Printf(TEXT("built room differs from plan (cover %d/%d, thorns %d/%d, sites %d)"), Cover, Plan.Cover.Num(), Thorns, Plan.Thorns.Num(), SpawnSites.Num()); }
    FHitResult Floor;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(ForestFloor), false);
    if (Passed && !(GetWorld()->LineTraceSingleByChannel(Floor, ForestAt(ForestRoom::Start, 600.f), ForestAt(ForestRoom::Start, -400.f), ECC_Visibility, Query)
        && FMath::Abs(Floor.ImpactPoint.Z) < 30.f))
    { Passed = false; Detail = TEXT("no floor at the entrance"); }

    // 3. Waves run in order and the exit only lights up when the room is clear.
    for (int32 I = 0; Passed && I < SpawnSites.Num(); ++I)
    {
        TickForestRun(10.f);
        Passed &= SpawnSites[I]->bActivated && (I + 1 >= SpawnSites.Num() || !SpawnSites[I + 1]->bActivated);
        Passed &= ForestExitMarker && ForestExitMarker->IsHidden();
        if (!Passed) Detail = FString::Printf(TEXT("wave %d did not start in order"), I + 1);
        SpawnSites[I]->bCleared = true;
    }
    TickForestRun(0.f);
    if (Passed && !Plan.bBoss && (!ForestExitMarker || ForestExitMarker->IsHidden())) { Passed = false; Detail = TEXT("exit did not open"); }
    if (Passed && Plan.bBoss && PromptAction != 4) { Passed = false; Detail = TEXT("boss room offers no way home"); }

    if (Passed) { UE_LOG(LogTemp, Display, TEXT("FOREST RUN CHECK PASSED: 1600 planned rooms fair and repeatable, room %d of seed %d built as planned"), ForestRoomNumber, ForestSeed); }
    else { UE_LOG(LogTemp, Error, TEXT("FOREST RUN CHECK FAILED: %s"), *Detail); }
    FPlatformMisc::RequestExitWithStatus(false, Passed ? 0 : 1);
#endif
}
