// World IV: the graveyard, a run of randomised rooms (Rules/GraveyardRules.h decides what stands in each).
// URL options: Graveyard=1?Seed=N?Room=R. Map only for now: no enemies yet (the ghosts come later); walking into the
// crypt in the east wall leads to the next room, and out of the last room back to camp. Scenery: GraveyardScenery.cpp.
#include "Levels/ArenaGameMode.h"
#include "Levels/ForestArt.h"
#include "Levels/GraveyardArt.h"
#include "Rules/GraveyardRules.h"
#include "Fighter/ArenaFighter.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/PointLight.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

void AArenaGameMode::StartGraveyard()
{
    using namespace GraveRoom;
    TravelToGraveRoom(FMath::RandRange(1, 999999), 1);
}

void AArenaGameMode::TravelToGraveRoom(int32 Seed, int32 Room)
{
    using namespace GraveRoom;
    UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this)), true, FString::Printf(TEXT("Graveyard=1?Seed=%d?Room=%d"), Seed, Room));
}

void AArenaGameMode::BuildGraveyard()
{
    using namespace GraveRoom;
    GraveSeed = UGameplayStatics::GetIntOption(OptionsString, TEXT("Seed"), 1);
    GraveRoomNumber = FMath::Clamp(UGameplayStatics::GetIntOption(OptionsString, TEXT("Room"), 1), 1, RoomsPerRun);
    const FPlan Plan = Make(GraveSeed, GraveRoomNumber);
    MapTitle = FString::Printf(TEXT("WORLD IV / THE GRAVEYARD / ROOM %d OF %d"), GraveRoomNumber, RoomsPerRun);
    MapPlatforms.Add(FVector4(0.f, 0.f, Half * 2.f, Half * 2.f));
    UWorld* World = GetWorld();

    // An invisible, flat floor to walk on; the visible ground is in GraveyardScenery.cpp.
    Prop(FVector(0, 0, -80), FVector(Half * 2.4f / 100.f, Half * 2.4f / 100.f, 1.6f), FLinearColor::Black)->SetActorHiddenInGame(true);
    BuildGraveyardScenery();

    // The planned pieces: one instanced batch per mesh, blocking where the piece is solid. The open graves and the
    // oak have no mesh collision (a hull would fill the pit or the crown), so they get simple hidden blockers.
    TMap<EPiece, UHierarchicalInstancedStaticMeshComponent*> Batches;
    int32 CandleLights = 0;
    for (const FItem& Item : Plan.Items)
    {
        UHierarchicalInstancedStaticMeshComponent*& Batch = Batches.FindOrAdd(Item.Piece);
        if (!Batch)
        {
            const bool Sways = Item.Piece == EPiece::RoseBush || Item.Piece == EPiece::RoseBushSmall || Item.Piece == EPiece::RoseArch
                || Item.Piece == EPiece::DeadTree || Item.Piece == EPiece::DeadOak || Item.Piece == EPiece::Candles;
            Batch = GraveArt::Batch(World, GraveArt::Mesh(Item.Piece), Item.IsSolid() && Item.Piece != EPiece::OpenGrave && Item.Piece != EPiece::DeadOak,
                Sways ? 2.5f : 0.f, Item.Piece == EPiece::Candles || Item.Piece == EPiece::Bones || Item.Piece == EPiece::RosesLaid ? 6000.f : 0.f);
            Batch->GetOwner()->Tags.Add(TEXT("GravePieces"));
        }
        Batch->AddInstance(FTransform(FRotator(Item.Pitch, Item.Yaw, Item.Roll), FVector(Item.P.X, Item.P.Y, 0.f), FVector(Item.Scale)));
        if (Item.Piece == EPiece::OpenGrave || Item.Piece == EPiece::DeadOak)
        {
            const FVector2D H = Item.Piece == EPiece::DeadOak ? FVector2D(90.f, 90.f) : Item.Half();
            auto* Blocker = Prop(FVector(Item.P.X, Item.P.Y, 150.f), FVector(H.X / 50.f, H.Y / 50.f, 3.f), FLinearColor::Black);
            Blocker->SetActorRotation(FRotator(0.f, Item.Yaw, 0.f));
            Blocker->SetActorHiddenInGame(true);
        }
        if (Item.Piece == EPiece::Candles && CandleLights++ < 8)
            ForestArt::PointGlow(World, FVector(Item.P.X, Item.P.Y, 40.f), FLinearColor(.5f, .8f, 1.f), 500.f, 260.f);
    }
    for (auto& [Piece, Batch] : Batches) Batch->BuildTreeIfOutdated(true, true);

    // The way on: the crypt's doorway glows brighter as you approach it.
    ExitPosition = FVector(Half - 100.f, 0.f, 0.f);
    GraveExitLight = ForestArt::PointGlow(World, FVector(Half + 40.f, 0.f, 180.f), FLinearColor(.5f, .8f, 1.f), 9000.f, 1100.f);
    ExitPortal = Prop(ExitPosition + FVector(0, 0, 200), FVector(.4f, 2.8f, 4.f), FLinearColor(.55f, .025f, .9f), true);
    ExitPortal->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    ExitPortal->SetActorHiddenInGame(true);
    TotalSites = 0;
}

bool AArenaGameMode::TickGraveyard(float Dt)
{
    using namespace GraveRoom;
    auto* Hero = Cast<AArenaFighter>(UGameplayStatics::GetPlayerPawn(this, 0));
    if (!Hero || !Hero->IsAlive()) return false;
    const bool Last = GraveRoomNumber >= RoomsPerRun;
    Objective = Last ? TEXT("THE GRAVEYARD / Map preview · the crypt leads back to camp")
        : TEXT("THE GRAVEYARD / Map preview · the crypt in the east leads deeper");
    Prompt.Empty();
    PromptAction = 0;
    const float Distance = FVector::Dist2D(Hero->GetActorLocation(), ExitPosition);
    if (GraveExitLight) GraveExitLight->PointLightComponent->SetIntensity(9000.f + 30000.f * FMath::Clamp(1.f - Distance / 2500.f, 0.f, 1.f));
    if (Hero->GetActorLocation().Z < -300.f)
    {
        Hero->SetActorLocation(FVector(Start.X, Start.Y, 115.f), false, nullptr, ETeleportType::TeleportPhysics);
        Hero->ResetAfterRecovery();
    }
    if (Distance > 220.f || FMath::Abs(Hero->GetActorLocation().Y) > 170.f) return false;
    // Stepping into the crypt: on to the next room, or home after the last one.
    if (FParse::Param(FCommandLine::Get(), TEXT("HellgirlGraveyardCheck"))) return true;
    if (Last) TravelToHub(); else TravelToGraveRoom(GraveSeed, GraveRoomNumber + 1);
    return true;
}

void AArenaGameMode::RunGraveyardCheck(float Dt)
{
    using namespace GraveRoom;
#if WITH_DEV_AUTOMATION_TESTS
    // -HellgirlGraveyardPreview (needs a GPU): photographs the room from above, from the gate, into each quarter and
    // at the crypt, to Saved/Screenshots/Graveyard/<seed>_<room>_<shot>.png.
    if (FParse::Param(FCommandLine::Get(), TEXT("HellgirlGraveyardPreview")))
    {
        static int32 Shot = 0;
        static float Clock = 0.f;
        static TWeakObjectPtr<ACameraActor> Camera;
        Clock += Dt;
        auto* PC = UGameplayStatics::GetPlayerController(this, 0);
        if (!PC) return;
        if (!Camera.IsValid())
        {
            Camera = GetWorld()->SpawnActor<ACameraActor>();
            Camera->GetCameraComponent()->SetFieldOfView(75.f);
        }
        if (Clock < 4.f) return;
        struct FView { FVector Eye, Look; };
        const FView Views[] = {
            {FVector(-400.f, 0.f, 13500.f), FVector(0.f, 0.f, 0.f)},                 // the whole yard from above
            {FVector(-5250.f, 0.f, 420.f), FVector(-2000.f, 0.f, 80.f)},            // from the gate, the player's first view
            {FVector(-700.f, 700.f, 700.f), FVector(-2800.f, 2800.f, 0.f)},         // into each quarter from the plaza
            {FVector(700.f, 700.f, 700.f), FVector(2800.f, 2800.f, 0.f)},
            {FVector(-700.f, -700.f, 700.f), FVector(-2800.f, -2800.f, 0.f)},
            {FVector(700.f, -700.f, 700.f), FVector(2800.f, -2800.f, 0.f)},
            {FVector(3600.f, -500.f, 380.f), FVector(Half, 0.f, 250.f)},            // the crypt
            {FVector(-2000.f, 600.f, 350.f), FVector(1500.f, -300.f, 150.f)},       // a low view across the yard to the moon
            {FVector(-5350.f, 0.f, 330.f), FVector(-5350.f, 0.f, 330.f) + FRotator(-20.f, 0.f, 0.f).Vector() * 1000.f}, // the play camera at the gate
        };
        const FView& View = Views[FMath::Min(Shot, static_cast<int32>(UE_ARRAY_COUNT(Views)) - 1)];
        Camera->SetActorLocationAndRotation(View.Eye, (View.Look - View.Eye).Rotation());
        PC->SetViewTarget(Camera.Get());
        if (Clock >= 5.f + Shot * 1.5f)
        {
            FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / FString::Printf(TEXT("Screenshots/Graveyard/%d_%d_%d.png"), GraveSeed, GraveRoomNumber, Shot), false, false);
            if (++Shot >= static_cast<int32>(UE_ARRAY_COUNT(Views))) FPlatformMisc::RequestExitWithStatus(false, 0);
        }
        return;
    }
    if (!FParse::Param(FCommandLine::Get(), TEXT("HellgirlGraveyardCheck"))) return;
    static float Clock = 0.f;
    static bool Done = false;
    Clock += Dt;
    if (Done || Clock < 1.f) return;
    Done = true;
    auto* Hero = Cast<AArenaFighter>(UGameplayStatics::GetPlayerPawn(this, 0));
    bool Passed = bGraveyard && Hero && SpawnSites.IsEmpty();
    FString Detail = Passed ? TEXT("") : TEXT("not a graveyard room, or enemies present");
    // 1. The planner: fair (spacing, reachability, full quarters) for many seeds, repeatable, and varied.
    TSet<int32> Layouts;
    TSet<uint8> Landmarks, Themes;
    int32 Pieces = 0, Most = 0, Unfair = 0;
    FString FirstUnfair;
    for (int32 Seed = 1; Seed <= 400; ++Seed)
        for (int32 Room = 1; Room <= RoomsPerRun; ++Room)
        {
            const FPlan A = Make(Seed, Room), B = Make(Seed, Room);
            FString Why;
            // Every unfair room is counted (the first few are logged), so one run shows all the kinds of failure.
            if (!IsFair(A, &Why))
            {
                if (Unfair++ < 12) UE_LOG(LogTemp, Warning, TEXT("Unfair room: seed %d room %d: %s"), Seed, Room, *Why);
                if (FirstUnfair.IsEmpty()) FirstUnfair = FString::Printf(TEXT("seed %d room %d: %s"), Seed, Room, *Why);
                Passed = false;
            }
            bool Same = A.Items.Num() == B.Items.Num();
            for (int32 I = 0; Same && I < A.Items.Num(); ++I) Same = A.Items[I].P.Equals(B.Items[I].P) && A.Items[I].Piece == B.Items[I].Piece;
            if (!Same) { Passed = false; Detail = FString::Printf(TEXT("seed %d room %d is not repeatable"), Seed, Room); }
            int32 Layout = 0;
            for (ETheme T : A.Themes) { Layout = Layout * 8 + static_cast<int32>(T); Themes.Add(static_cast<uint8>(T)); }
            Layouts.Add(Layout);
            Landmarks.Add(static_cast<uint8>(A.Landmark));
            Pieces += A.Items.Num();
            Most = FMath::Max(Most, A.Items.Num());
        }
    if (Unfair) Detail = FString::Printf(TEXT("%d of 1600 rooms unfair, e.g. %s"), Unfair, *FirstUnfair);
    if (Passed && (Layouts.Num() < 100 || Landmarks.Num() < 3 || Themes.Num() < static_cast<int32>(ETheme::Count)))
    { Passed = false; Detail = FString::Printf(TEXT("rooms are not varied enough (%d quarter layouts, %d landmarks)"), Layouts.Num(), Landmarks.Num()); }

    // 2. The built room matches its plan, and there is a floor at the gate.
    const FPlan Plan = Make(GraveSeed, GraveRoomNumber);
    int32 Instances = 0, Boundary = 0;
    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
    {
        if (It->ActorHasTag(TEXT("GravePieces")))
            if (auto* Batch = It->FindComponentByClass<UHierarchicalInstancedStaticMeshComponent>()) Instances += Batch->GetInstanceCount();
        Boundary += It->ActorHasTag(TEXT("GraveBoundary"));
    }
    if (Passed && (Instances != Plan.Items.Num() || Boundary != 4))
    { Passed = false; Detail = FString::Printf(TEXT("built room differs from plan (%d of %d pieces, %d boundary walls)"), Instances, Plan.Items.Num(), Boundary); }
    FHitResult Floor;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(GraveFloor), false);
    if (Passed && !(GetWorld()->LineTraceSingleByChannel(Floor, FVector(Start.X, Start.Y, 600.f), FVector(Start.X, Start.Y, -400.f), ECC_Visibility, Query)
        && FMath::Abs(Floor.ImpactPoint.Z) < 30.f))
    { Passed = false; Detail = TEXT("no floor at the gate"); }
    // A solid piece really blocks: a trace across the first headstone hits it.
    for (const FItem& Item : Plan.Items)
    {
        if (!Passed || Item.Piece != EPiece::HeadstoneRound) continue;
        const FVector Across = FVector(Rotate(FVector2D(0.f, 1.f), Item.Yaw), 0.f) * 200.f;
        const FVector Mid(Item.P.X, Item.P.Y, 50.f);
        FHitResult Hit;
        if (!GetWorld()->LineTraceSingleByChannel(Hit, Mid - Across, Mid + Across, ECC_WorldStatic, Query))
        { Passed = false; Detail = FString::Printf(TEXT("the headstone at %s does not block"), *Item.P.ToString()); }
        break;
    }

    // 3. Hellgirl starts at the gate; the crypt only takes her on once she steps into it.
    if (Passed && FVector::Dist2D(Hero->GetActorLocation(), FVector(Start.X, Start.Y, 0.f)) > 150.f) { Passed = false; Detail = TEXT("Hellgirl does not start at the gate"); }
    if (Passed && TickGraveyard(0.f)) { Passed = false; Detail = TEXT("the crypt opened at the gate"); }
    Hero->SetActorLocation(ExitPosition + FVector(0, 0, 115.f), false, nullptr, ETeleportType::TeleportPhysics);
    if (Passed && !TickGraveyard(0.f)) { Passed = false; Detail = TEXT("stepping into the crypt does nothing"); }

    if (Passed) { UE_LOG(LogTemp, Display, TEXT("GRAVEYARD CHECK PASSED: 1600 planned rooms fair, reachable and repeatable (%d quarter layouts, %d pieces on average, at most %d); room %d of seed %d built as planned (%d pieces)"),
        Layouts.Num(), Pieces / 1600, Most, GraveRoomNumber, GraveSeed, Instances); }
    else { UE_LOG(LogTemp, Error, TEXT("GRAVEYARD CHECK FAILED: %s"), *Detail); }
    FPlatformMisc::RequestExitWithStatus(false, Passed ? 0 : 1);
#endif
}
