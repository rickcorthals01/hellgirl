// Forest run: a chain of randomised forest rooms (Rules/ForestRoomRules.h decides what is in each room).
// URL options: ForestRun=1?Seed=N?Room=R. Rooms 1-3 are goblin fights, room 4 is the Goblin Queen.
// Health and energy carry over between rooms; dying or finishing the run returns to camp.
#include "Levels/ArenaGameMode.h"
#include "Levels/MapPieces.h"
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
const TCHAR* ForestCylinder = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");
const TCHAR* ForestCone = TEXT("/Engine/BasicShapes/Cone.Cone");
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
    auto Shape = [&](FVector P, FVector Scale, FLinearColor Color, const TCHAR* MeshPath, bool Collide)
    {
        auto* A = Prop(P, Scale, Color);
        A->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, MeshPath));
        A->SetActorEnableCollision(Collide);
        return A;
    };

    // Fixed room shape: forest floor, a trodden clearing, and the trail from west to east.
    auto* Ground = Prop(FVector(0, 0, -85), FVector(80, 80, 1.6f), FLinearColor(.08f, .115f, .06f));
    if (auto* Earth = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Environment/Materials/M_EnvironmentEarth.M_EnvironmentEarth")))
        if (auto* Mat = UMaterialInstanceDynamic::Create(Earth, Ground))
        {
            Mat->SetVectorParameterValue(TEXT("Color"), FLinearColor(.34f, .44f, .28f));
            Ground->GetStaticMeshComponent()->SetMaterial(0, Mat);
        }
    Shape(FVector(0, 0, -7), FVector(ForestRoom::Radius * .02f, ForestRoom::Radius * .02f, .15f), FLinearColor(.25f, .19f, .11f), ForestCylinder, true);
    for (int32 I = -9; I <= 9; ++I)
        if (FMath::Abs(I) > 2)
            Shape(FVector(I * 260.f, 0, -1), FVector(3.4f, 3.1f, .09f), FLinearColor(.21f, .15f, .09f), ForestCylinder, false);
    BuildForestRunScenery();

    // Cover.
    for (int32 I = 0; I < Plan.Cover.Num(); ++I)
    {
        const ForestRoom::FCover& C = Plan.Cover[I];
        AActor* Piece = nullptr;
        if (C.Type == ForestRoom::ECover::Boulder)
            Piece = MapPieces::Rock(GetWorld(), ForestAt(C.Position, 190.f * C.Size), FVector(380.f, 330.f, 210.f) * C.Size,
                FLinearColor(.19f, .2f, .16f), ForestSeed * 31 + ForestRoomNumber * 7 + I, true, false);
        else if (C.Type == ForestRoom::ECover::Log)
        {
            Piece = Shape(ForestAt(C.Position, 48.f * C.Size), FVector(1.f, 1.f, 5.2f) * C.Size, FLinearColor(.16f, .09f, .045f), ForestCylinder, true);
            Piece->SetActorRotation(FRotator(90.f, C.Yaw, 0.f));
        }
        else
        {
            Piece = Shape(ForestAt(C.Position, 55.f * C.Size), FVector(1.5f, 1.5f, 1.1f) * C.Size, FLinearColor(.2f, .12f, .06f), ForestCylinder, true);
            Shape(ForestAt(C.Position, 112.f * C.Size), FVector(1.3f, 1.3f, .04f) * C.Size, FLinearColor(.42f, .3f, .17f), ForestCylinder, false);
        }
        if (Piece) Piece->Tags.Add(TEXT("ForestCover"));
    }

    // Thorn patches: a dark bramble bed with spikes; standing in one hurts.
    auto* Spikes = MapPieces::DecorationBatch(GetWorld(), ForestCone, FLinearColor(.2f, .04f, .05f));
    FRandomStream Bramble(ForestSeed + ForestRoomNumber * 101);
    for (const ForestRoom::FThorns& T : Plan.Thorns)
    {
        auto* Bed = Shape(ForestAt(T.Position, 2.f), FVector(T.Radius * .02f, T.Radius * .02f, .05f), FLinearColor(.12f, .03f, .035f), ForestCylinder, false);
        Bed->Tags.Add(TEXT("ForestThorns"));
        for (int32 I = 0; I < 26; ++I)
        {
            const float A = Bramble.FRandRange(0.f, 2.f * PI), D = FMath::Sqrt(Bramble.FRand()) * T.Radius * .92f;
            Spikes->AddInstance(FTransform(FRotator(Bramble.FRandRange(-25.f, 25.f), 0, Bramble.FRandRange(-25.f, 25.f)),
                ForestAt(T.Position + FVector2D(FMath::Cos(A), FMath::Sin(A)) * D, 22.f), FVector(.16f, .16f, Bramble.FRandRange(.35f, .75f))));
        }
        ForestThorns.Add(FVector4(T.Position.X, T.Position.Y, T.Radius, 0.f));
    }

    // Waves (goblins), or the Goblin Queen in the last room.
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
        }

    // The trail marker lights up once the room is clear.
    ForestExitMarker = Shape(ForestAt(ForestRoom::Exit, 160.f), FVector(.5f, .5f, 3.2f), FLinearColor(.35f, 1.f, .3f), ForestCylinder, false);
    if (auto* GlowBase = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Environment/Materials/M_EnvironmentGlow.M_EnvironmentGlow")))
        if (auto* Glow = UMaterialInstanceDynamic::Create(GlowBase, ForestExitMarker))
        {
            Glow->SetVectorParameterValue(TEXT("Color"), FLinearColor(.35f, 1.f, .3f));
            ForestExitMarker->GetStaticMeshComponent()->SetMaterial(0, Glow);
        }
    ForestExitMarker->SetActorHiddenInGame(true);
    ExitPosition = ForestAt(ForestRoom::Exit);
    ExitPortal = Prop(ExitPosition + FVector(0, 0, 200), FVector(.4f, 2.8f, 4.f), FLinearColor(.55f, .025f, .9f), true);
    ExitPortal->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    ExitPortal->SetActorHiddenInGame(true);
    TotalSites = SpawnSites.Num();

    // Moonlit forest lighting, as at camp but without the campfire.
    auto* Moon = GetWorld()->SpawnActor<ADirectionalLight>(FVector(0, 0, 2000), FRotator(-45, -30, 0));
    Moon->GetLightComponent()->SetMobility(EComponentMobility::Movable);
    Moon->GetLightComponent()->SetIntensity(2.1f);
    Moon->GetLightComponent()->SetLightColor(FLinearColor(.42f, .55f, .8f));
    auto* Sky = GetWorld()->SpawnActor<ASkyLight>();
    Sky->GetLightComponent()->SetMobility(EComponentMobility::Movable);
    Sky->GetLightComponent()->SetIntensity(.7f);
    Sky->GetLightComponent()->SetRealTimeCaptureEnabled(false);
    auto* Fog = GetWorld()->SpawnActor<AExponentialHeightFog>();
    Fog->GetComponent()->SetFogDensity(.016f);
    Fog->GetComponent()->SetFogInscatteringColor(FLinearColor(.028f, .06f, .063f));
    Fog->GetComponent()->SetStartDistance(1400.f);
    auto* Post = GetWorld()->SpawnActor<APostProcessVolume>();
    Post->bUnbound = true;
    Post->Settings.bOverride_AutoExposureMinBrightness = Post->Settings.bOverride_AutoExposureMaxBrightness = true;
    Post->Settings.AutoExposureMinBrightness = Post->Settings.AutoExposureMaxBrightness = 1.f;
    Post->Settings.bOverride_BloomIntensity = true;
    Post->Settings.BloomIntensity = .45f;
}

void AArenaGameMode::BuildForestRunScenery()
{
    using namespace MapPieces;
    // The tree wall is the same every room (it is the room's shape), so it uses a fixed seed.
    FRandomStream Random(7717);
    auto* Trunks = DecorationBatch(GetWorld(), ForestCylinder, FLinearColor(.17f, .1f, .055f));
    auto* Needles = DecorationBatch(GetWorld(), ForestCone, FLinearColor(.17f, .32f, .19f));
    auto* DarkNeedles = DecorationBatch(GetWorld(), ForestCone, FLinearColor(.1f, .21f, .13f));
    auto* Ferns = DecorationBatch(GetWorld(), ForestCone, FLinearColor(.11f, .27f, .11f));
    auto* Fireflies = DecorationBatch(GetWorld(), TEXT("/Engine/BasicShapes/Sphere.Sphere"), FLinearColor(.35f, 1.f, .24f), true);
    Needles->SetCastShadow(false);
    DarkNeedles->SetCastShadow(false);
    for (auto* Batch : {Trunks, Needles, DarkNeedles, Ferns, Fireflies}) Batch->bAutoRebuildTreeOnInstanceChanges = false;
    for (int32 I = 0; I < 150; ++I)
    {
        const float A = I * 2.399963f + Random.FRandRange(-.1f, .1f);
        const float R = ForestRoom::Radius + Random.FRandRange(120.f, 1000.f);
        const FVector P(FMath::Cos(A) * R, FMath::Sin(A) * R, 0);
        // Openings where the trail enters (west) and leaves (east).
        if (FMath::Abs(P.Y) < 330.f) continue;
        const float H = Random.FRandRange(700.f, 1250.f), Crown = Random.FRandRange(150.f, 230.f);
        Trunks->AddInstance(FTransform(FRotator(0, Random.FRandRange(0.f, 360.f), 0), P + FVector(0, 0, H * .5f), FVector(.5f, .5f, H / 100.f)));
        for (int32 Layer = 0; Layer < 3; ++Layer)
        {
            const FVector C = P + FVector(Random.FRandRange(-35.f, 35.f), Random.FRandRange(-35.f, 35.f), H * (.56f + Layer * .067f));
            ((I + Layer) % 3 == 0 ? DarkNeedles : Needles)->AddInstance(FTransform(FRotator(0, Random.FRandRange(0.f, 360.f), 0), C,
                FVector(Crown * (1.f - .28f * Layer) / 50.f, Crown * (1.f - .28f * Layer) / 50.f, H * (.5f - .08f * Layer) / 100.f)));
        }
    }
    for (int32 I = 0; I < 260; ++I)
    {
        const float A = Random.FRandRange(0.f, 2.f * PI), R = Random.FRandRange(ForestRoom::Radius - 250.f, ForestRoom::Radius + 700.f);
        const FVector P(FMath::Cos(A) * R, FMath::Sin(A) * R, 20.f);
        const float S = Random.FRandRange(.25f, .75f);
        Ferns->AddInstance(FTransform(FRotator(0, Random.FRandRange(0.f, 360.f), 0), P, FVector(S, S, Random.FRandRange(.7f, 1.7f))));
    }
    for (int32 I = 0; I < 70; ++I)
    {
        const float A = Random.FRandRange(0.f, 2.f * PI), R = Random.FRandRange(900.f, ForestRoom::Radius + 600.f);
        Fireflies->AddInstance(FTransform(FRotator::ZeroRotator, FVector(FMath::Cos(A) * R, FMath::Sin(A) * R, Random.FRandRange(90.f, 450.f)),
            FVector(Random.FRandRange(.018f, .045f))));
    }
    for (auto* Batch : {Trunks, Needles, DarkNeedles, Ferns, Fireflies}) Batch->BuildTreeIfOutdated(true, true);
    // Invisible wall along the tree line; the trail openings stay closed too (the exit is inside the clearing).
    constexpr int32 Sections = 56;
    for (int32 I = 0; I < Sections; ++I)
    {
        const float A = 2.f * PI * I / Sections, B = 2.f * PI * (I + 1) / Sections;
        const FVector From(ForestRoom::Radius * FMath::Cos(A), ForestRoom::Radius * FMath::Sin(A), 0), To(ForestRoom::Radius * FMath::Cos(B), ForestRoom::Radius * FMath::Sin(B), 0);
        const FVector Delta = To - From;
        auto* Wall = Prop((From + To) * .5f + FVector(0, 0, 2900.f), FVector((Delta.Size() + 24.f) / 100.f, .8f, 60.f), FLinearColor::Black);
        Wall->SetActorRotation(FRotator(0, FMath::RadiansToDegrees(FMath::Atan2(Delta.Y, Delta.X)), 0));
        Wall->Tags.Add(TEXT("ForestBoundary"));
        Wall->SetActorHiddenInGame(true);
    }
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
        if (ForestExitMarker) ForestExitMarker->SetActorHiddenInGame(true);
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
        if (ForestExitMarker) ForestExitMarker->SetActorHiddenInGame(false);
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
        if (!Camera.IsValid() || Clock < 3.f) return;
        const FVector Eye = Shot == 0 ? FVector(-600.f, 0.f, 5200.f) : FVector(-3300.f, 0.f, 900.f);
        const FVector Look = Shot == 0 ? FVector(0.f, 0.f, 0.f) : FVector(0.f, 0.f, 50.f);
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
