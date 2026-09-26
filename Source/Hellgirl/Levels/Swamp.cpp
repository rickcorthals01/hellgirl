// World II: the swamp, a run of randomised corridors (Rules/SwampRules.h decides what is in each). URL options:
// Swamp=1?Seed=N?Room=R. Map only for now: no enemies yet; the last room holds the Frog King's giant lily pad.
// Walking on or near a risen zombie arm hurts. Hellgirl wades through the shallow soul water with ripples and
// splashes. The light at the east end leads on (after the last room, back to camp). Scenery: SwampScenery.cpp.
#include "Levels/ArenaGameMode.h"
#include "Levels/ForestArt.h"
#include "Levels/GraveyardArt.h"
#include "Rules/SwampRules.h"
#include "Fighter/ArenaFighter.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
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
#include "UnrealClient.h"

namespace SwampArt
{
UStaticMesh* Kit(const TCHAR* Name);
UMaterialInterface* Material(const TCHAR* Name);

UStaticMesh* Mesh(SwampRoom::EPiece Piece)
{
    static const TCHAR* Names[] = {TEXT("SM_SwampTreeA"), TEXT("SM_SwampTreeB"), TEXT("SM_SwampTreeC"), TEXT("SM_ZombieArm"), TEXT("SM_LilyPad"),
        TEXT("SM_LilyPadSmall"), TEXT("SM_GiantLilyPad"), TEXT("SM_Boardwalk"), TEXT("SM_BoardwalkBroken"), TEXT("SM_Reeds"), TEXT("SM_DrownedStump"), TEXT("SM_MudIsland")};
    static_assert(UE_ARRAY_COUNT(Names) == static_cast<int32>(SwampRoom::EPiece::Count), "one kit mesh per piece");
    return Kit(Names[FMath::Clamp(static_cast<int32>(Piece), 0, UE_ARRAY_COUNT(Names) - 1)]);
}

// Puts Material on every slot (the arm and the ripple are all glowing faces, whatever slots the import made).
void AllSlots(UStaticMeshComponent* Component, UMaterialInterface* Material)
{
    for (int32 I = 0; I < FMath::Max(1, Component->GetNumMaterials()); ++I) Component->SetMaterial(I, Material);
}

// The arms' cycle (seconds at rate 1): rise, reach and claw, sink, stay under.
constexpr float ArmCycle = 7.5f, ArmRising = .9f, ArmUp = 5.4f, ArmSinking = 6.2f;
float ArmRise(float T)
{
    T = FMath::Fmod(T, ArmCycle);
    if (T < ArmRising) return FMath::SmoothStep(0.f, 1.f, T / ArmRising);
    if (T < ArmUp) return 1.f;
    if (T < ArmSinking) return 1.f - FMath::SmoothStep(0.f, 1.f, (T - ArmUp) / (ArmSinking - ArmUp));
    return 0.f;
}
}

void AArenaGameMode::StartSwamp()
{
    TravelToSwampRoom(FMath::RandRange(1, 999999), 1);
}

void AArenaGameMode::TravelToSwampRoom(int32 Seed, int32 Room)
{
    UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this)), true, FString::Printf(TEXT("Swamp=1?Seed=%d?Room=%d"), Seed, Room));
}

void AArenaGameMode::BuildSwamp()
{
    using namespace SwampRoom;
    SwampSeed = UGameplayStatics::GetIntOption(OptionsString, TEXT("Seed"), 1);
    SwampRoomNumber = FMath::Clamp(UGameplayStatics::GetIntOption(OptionsString, TEXT("Room"), 1), 1, RoomsPerRun);
    const FPlan Plan = Make(SwampSeed, SwampRoomNumber);
    SwampWaterStart = Plan.WaterStart;
    SwampWaterEnd = Plan.WaterEnd;
    MapTitle = FString::Printf(TEXT("WORLD II / THE SWAMP / ROOM %d OF %d%s"), SwampRoomNumber, RoomsPerRun, Plan.bFrogKing ? TEXT(" / THE FROG KING") : TEXT(""));
    MapPlatforms.Add(FVector4(0.f, 0.f, Half * 2.f, HalfWidth * 2.f));
    UWorld* World = GetWorld();

    // An invisible, flat floor under both the mud and the water; the visible ground is in SwampScenery.cpp.
    Prop(FVector(0, 0, -80), FVector(Half * 2.4f / 100.f, HalfWidth * 3.f / 100.f, 1.6f), FLinearColor::Black)->SetActorHiddenInGame(true);
    BuildSwampScenery();

    // The planned pieces, one instanced batch per mesh. The boardwalk, islands, stumps and the giant lily pad block
    // (and can be stood on); trees get simple trunk blockers. Lily pads float on the surface; arms are animated actors.
    TMap<EPiece, UHierarchicalInstancedStaticMeshComponent*> Batches;
    UMaterialInterface* Soul = SwampArt::Material(TEXT("MI_SwampSoul"));
    for (const FItem& Item : Plan.Items)
    {
        if (Item.Piece == EPiece::ZombieArm)
        {
            FSwampArm Arm;
            Arm.P = Item.P;
            Arm.Yaw = Item.Yaw;
            Arm.Scale = Item.Scale;
            Arm.Phase = FMath::Frac(FMath::Abs(Item.P.X * .013f + Item.P.Y * .007f)) * SwampArt::ArmCycle;
            Arm.Rate = .85f + .3f * FMath::Frac(FMath::Abs(Item.P.Y * .011f));
            if (auto* Actor = ForestArt::Solid(World, SwampArt::Mesh(EPiece::ZombieArm), FTransform(FRotator(0, Item.Yaw, 0), FVector(Item.P.X, Item.P.Y, -230.f * Item.Scale), FVector(Item.Scale)), false))
            {
                SwampArt::AllSlots(Actor->GetStaticMeshComponent(), Soul);
                Actor->GetStaticMeshComponent()->SetCastShadow(false);
                Actor->Tags.Add(TEXT("SwampArm"));
                Arm.Actor = Actor;
            }
            Arm.Light = ForestArt::PointGlow(World, FVector(Item.P.X, Item.P.Y, 140.f), FLinearColor(.4f, .8f, 1.f), 0.f, 420.f);
            SwampArms.Add(Arm);
            continue;
        }
        UHierarchicalInstancedStaticMeshComponent*& Batch = Batches.FindOrAdd(Item.Piece);
        if (!Batch)
        {
            const bool Collide = Item.IsSolid() && !IsTree(Item.Piece);
            const bool Sways = IsTree(Item.Piece) || Item.Piece == EPiece::Reeds || Item.Piece == EPiece::LilyPad || Item.Piece == EPiece::LilyPadSmall;
            Batch = GraveArt::Batch(World, SwampArt::Mesh(Item.Piece), Collide, Sways ? 2.5f : 0.f);
            Batch->GetOwner()->Tags.Add(TEXT("SwampPieces"));
        }
        const bool Floats = Item.Piece == EPiece::LilyPad || Item.Piece == EPiece::LilyPadSmall;
        Batch->AddInstance(FTransform(FRotator(0, Item.Yaw, 0), FVector(Item.P.X, Item.P.Y, Floats ? WaterZ - 1.f : 0.f), FVector(Item.Scale)));
        if (IsTree(Item.Piece))
        {
            auto* Trunk = Prop(FVector(Item.P.X, Item.P.Y, 200.f), FVector(1.1f * Item.Scale, 1.1f * Item.Scale, 4.f), FLinearColor::Black);
            Trunk->SetActorHiddenInGame(true);
        }
    }
    for (auto& [Piece, Batch] : Batches) Batch->BuildTreeIfOutdated(true, true);

    ExitPosition = FVector(Exit.X, Exit.Y, 0.f);
    ExitPortal = Prop(ExitPosition + FVector(0, 0, 200), FVector(.4f, 2.8f, 4.f), FLinearColor(.55f, .025f, .9f), true);
    ExitPortal->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    ExitPortal->SetActorHiddenInGame(true);
    TotalSites = 0;
}

void AArenaGameMode::SwampSplash(FVector Where, float Size, int32 Droplets)
{
    UWorld* World = GetWorld();
    auto Take = [&](bool Droplet) -> FSwampRipple*
    {
        for (FSwampRipple& R : SwampRipples)
            if (R.bDroplet == Droplet && R.Age >= R.Life && R.Actor.IsValid()) return &R;
        if (SwampRipples.Num() >= 96) return nullptr;
        UStaticMesh* Mesh = Droplet ? LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")) : SwampArt::Kit(TEXT("SM_Ripple"));
        auto* Actor = ForestArt::Solid(World, Mesh, FTransform(Where), false);
        if (!Actor) return nullptr;
        Actor->GetStaticMeshComponent()->SetCastShadow(false);
        FSwampRipple R;
        R.Actor = Actor;
        R.bDroplet = Droplet;
        if (auto* Material = UMaterialInstanceDynamic::Create(SwampArt::Material(TEXT("M_SwampRipple")), Actor))
        {
            SwampArt::AllSlots(Actor->GetStaticMeshComponent(), Material);
            R.Material = Material;
        }
        return &SwampRipples.Add_GetRef(R);
    };
    if (FSwampRipple* Ring = Take(false))
    {
        Ring->Age = 0.f;
        Ring->Life = .7f + .25f * Size;
        Ring->Size = Size;
        Ring->Actor->SetActorLocation(FVector(Where.X, Where.Y, SwampRoom::WaterZ + 1.f));
        Ring->Actor->SetActorHiddenInGame(false);
    }
    for (int32 I = 0; I < Droplets; ++I)
        if (FSwampRipple* Drop = Take(true))
        {
            const float A = FMath::FRandRange(0.f, 2.f * PI), Out = FMath::FRandRange(60.f, 170.f) * FMath::Sqrt(Size);
            Drop->Age = 0.f;
            Drop->Life = 1.5f;
            Drop->Size = FMath::FRandRange(.035f, .07f);
            Drop->Velocity = FVector(FMath::Cos(A) * Out, FMath::Sin(A) * Out, FMath::FRandRange(220.f, 420.f) * FMath::Sqrt(Size));
            Drop->Actor->SetActorLocation(FVector(Where.X, Where.Y, SwampRoom::WaterZ + 5.f));
            Drop->Actor->SetActorScale3D(FVector(Drop->Size));
            Drop->Actor->SetActorHiddenInGame(false);
        }
}

bool AArenaGameMode::TickSwamp(float Dt)
{
    using namespace SwampRoom;
    SwampClock += Dt;
    auto* Hero = Cast<AArenaFighter>(UGameplayStatics::GetPlayerPawn(this, 0));
    const bool Last = SwampRoomNumber >= RoomsPerRun;

    // The arms: rise out of the water, reach and claw (the material flexes the fingers), sink, stay under a while.
    for (FSwampArm& Arm : SwampArms)
    {
        const float Before = Arm.Rise;
        Arm.Rise = SwampArt::ArmRise(SwampClock * Arm.Rate + Arm.Phase);
        if (Arm.Actor.IsValid())
        {
            const float T = SwampClock + Arm.Phase;
            Arm.Actor->SetActorLocationAndRotation(FVector(Arm.P.X, Arm.P.Y, -230.f * (1.f - Arm.Rise) * Arm.Scale),
                FRotator(7.f * FMath::Sin(T * .9f), Arm.Yaw + 14.f * FMath::Sin(T * .6f), 6.f * FMath::Sin(T * 1.3f)));
            Arm.Actor->SetActorHiddenInGame(Arm.Rise <= 0.f);
        }
        if (Arm.Light.IsValid()) Arm.Light->PointLightComponent->SetIntensity(2200.f * Arm.Rise);
        if (Before <= 0.f && Arm.Rise > 0.f) SwampSplash(FVector(Arm.P.X, Arm.P.Y, WaterZ), 1.6f, 8);
    }

    // Ripples spread and fade; droplets fly and fall back into the water.
    for (FSwampRipple& R : SwampRipples)
    {
        if (R.Age >= R.Life || !R.Actor.IsValid()) continue;
        R.Age += Dt;
        const float F = FMath::Clamp(R.Age / R.Life, 0.f, 1.f);
        if (R.bDroplet)
        {
            R.Velocity.Z -= 980.f * Dt;
            const FVector At = R.Actor->GetActorLocation() + R.Velocity * Dt;
            R.Actor->SetActorLocation(At);
            if (At.Z < WaterZ) R.Age = R.Life;
            if (R.Material.IsValid()) R.Material->SetScalarParameterValue(TEXT("Opacity"), .9f);
        }
        else
        {
            const float S = R.Size * (.3f + 1.3f * FMath::Sqrt(F));
            R.Actor->SetActorScale3D(FVector(S, S, 1.f));
            if (R.Material.IsValid()) R.Material->SetScalarParameterValue(TEXT("Opacity"), .75f * FMath::Pow(1.f - F, 1.5f));
        }
        if (R.Age >= R.Life) R.Actor->SetActorHiddenInGame(true);
    }

    if (!Hero || !Hero->IsAlive()) return false;
    Objective = Last ? TEXT("THE SWAMP / Map preview · the Frog King's lily pad · the light leads back to camp")
        : TEXT("THE SWAMP / Map preview · follow the light at the end");
    Prompt.Empty();
    PromptAction = 0;

    // Wading: ripples at her feet while she moves through the water, a big splash when she lands or dodges in it.
    const FVector At = Hero->GetActorLocation();
    auto* Move = Hero->GetCharacterMovement();
    const bool OnGround = Move->IsMovingOnGround();
    const bool Wading = At.X > SwampWaterStart && At.X < SwampWaterEnd && OnGround && Move->CurrentFloor.HitResult.ImpactPoint.Z < 10.f;
    const float Speed = Hero->GetVelocity().Size2D();
    const FVector Feet(At.X, At.Y, WaterZ);
    if (Wading)
    {
        SwampStepClock -= Dt;
        if (Speed > 120.f && SwampStepClock <= 0.f) { SwampSplash(Feet, .9f, Speed > 500.f ? 3 : 1); SwampStepClock = .28f; }
        if (bSwampWasFalling) SwampSplash(Feet, 2.3f, 14);
        else if (Speed > SwampLastSpeed + 500.f) SwampSplash(Feet, 1.8f, 10);
    }
    bSwampWasFalling = !OnGround;
    SwampLastSpeed = Speed;

    // A risen arm claws at anyone on or near it.
    SwampHurtClock = FMath::Max(0.f, SwampHurtClock - Dt);
    for (const FSwampArm& Arm : SwampArms)
        if (SwampHurtClock <= 0.f && Arm.Rise > .5f && At.Z < 260.f && FVector2D::Distance(FVector2D(At), Arm.P) < ArmReach)
        {
            Hero->ApplyPhysicsDamage(6.f, FVector::ZeroVector);
            Hero->MoveLabel = TEXT("DROWNED HANDS!");
            SwampHurtClock = .5f;
            SwampSplash(FVector(Arm.P.X, Arm.P.Y, WaterZ), 1.4f, 6);
        }

    if (At.Z < -300.f)
    {
        Hero->SetActorLocation(FVector(Start.X, Start.Y, 115.f), false, nullptr, ETeleportType::TeleportPhysics);
        Hero->ResetAfterRecovery();
    }
    if (FVector::Dist2D(At, ExitPosition) > 260.f) return false;
    // Stepping into the light: on to the next room, or home after the last one.
    if (FParse::Param(FCommandLine::Get(), TEXT("HellgirlSwampCheck"))) return true;
    if (Last) TravelToHub(); else TravelToSwampRoom(SwampSeed, SwampRoomNumber + 1);
    return true;
}

void AArenaGameMode::RunSwampCheck(float Dt)
{
#if WITH_DEV_AUTOMATION_TESTS
    using namespace SwampRoom;
    // -HellgirlSwampPreview (needs a GPU): photographs the room to Saved/Screenshots/Swamp/<seed>_<room>_<shot>.png.
    if (FParse::Param(FCommandLine::Get(), TEXT("HellgirlSwampPreview")))
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
        const FVector2D Arm = SwampArms.Num() ? SwampArms[0].P : FVector2D(0.f, 0.f);
        const float Mid = (SwampWaterStart + SwampWaterEnd) * .5f;
        struct FView { FVector Eye; FRotator Look; };
        auto Toward = [](FVector Eye, FVector Target) { return FView{Eye, (Target - Eye).Rotation()}; };
        const FView Views[] = {
            {FVector(0.f, 0.f, 9000.f), FRotator(-89.9f, 90.f, 0.f)},                                   // the whole corridor from above
            Toward(FVector(Start.X - 450.f, 0.f, 380.f), FVector(Start.X + 3000.f, 0.f, 60.f)),       // the way in
            Toward(FVector(SwampWaterStart - 300.f, -900.f, 300.f), FVector(SwampWaterStart + 2500.f, 200.f, 20.f)), // the water's edge
            Toward(FVector(Mid - 1800.f, 700.f, 350.f), FVector(Mid + 1500.f, -300.f, 20.f)),        // across the water
            Toward(FVector(Arm.X - 450.f, Arm.Y - 250.f, 220.f), FVector(Arm.X, Arm.Y, 90.f)),         // a zombie arm
            Toward(FVector(SwampWaterEnd + 400.f, 900.f, 400.f), FVector(SwampWaterEnd - 2500.f, -200.f, 20.f)), // back over the water
            Toward(FVector(Exit.X - 1800.f, 400.f, 300.f), FVector(Exit.X + 150.f, -80.f, 180.f)),    // the light at the end
            {FVector(Start.X - 400.f, 0.f, 330.f), FRotator(-20.f, 0.f, 0.f)},                          // the play camera at the start
        };
        const FView& View = Views[FMath::Min(Shot, static_cast<int32>(UE_ARRAY_COUNT(Views)) - 1)];
        Camera->SetActorLocationAndRotation(View.Eye, View.Look);
        PC->SetViewTarget(Camera.Get());
        // Keep the arms up for the photographs.
        for (FSwampArm& A : SwampArms) A.Phase = SwampArt::ArmRising + 1.f - SwampClock * A.Rate;
        if (Clock >= 5.f + Shot * 1.5f)
        {
            FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / FString::Printf(TEXT("Screenshots/Swamp/%d_%d_%d.png"), SwampSeed, SwampRoomNumber, Shot), false, false);
            if (++Shot >= static_cast<int32>(UE_ARRAY_COUNT(Views))) FPlatformMisc::RequestExitWithStatus(false, 0);
        }
        return;
    }
    if (!FParse::Param(FCommandLine::Get(), TEXT("HellgirlSwampCheck"))) return;
    static float Clock = 0.f;
    static bool Done = false;
    Clock += Dt;
    if (Done || Clock < 1.f) return;
    Done = true;
    auto* Hero = Cast<AArenaFighter>(UGameplayStatics::GetPlayerPawn(this, 0));
    bool Passed = bSwamp && Hero && SpawnSites.IsEmpty();
    FString Detail = Passed ? TEXT("") : TEXT("not a swamp room, or enemies present");
    // 1. The planner: fair (mud holds only trees, arms clear of the boardwalk, a safe way to the light) for many seeds,
    // repeatable, and varied.
    int32 Unfair = 0, Pieces = 0;
    FString FirstUnfair;
    TSet<int32> Shapes;
    for (int32 Seed = 1; Seed <= 400; ++Seed)
        for (int32 Room = 1; Room <= RoomsPerRun; ++Room)
        {
            const FPlan A = Make(Seed, Room), B = Make(Seed, Room);
            FString Why;
            if (!IsFair(A, &Why))
            {
                if (Unfair++ < 12) UE_LOG(LogTemp, Warning, TEXT("Unfair swamp room: seed %d room %d: %s"), Seed, Room, *Why);
                if (FirstUnfair.IsEmpty()) FirstUnfair = FString::Printf(TEXT("seed %d room %d: %s"), Seed, Room, *Why);
                Passed = false;
            }
            bool Same = A.Items.Num() == B.Items.Num();
            for (int32 I = 0; Same && I < A.Items.Num(); ++I) Same = A.Items[I].P.Equals(B.Items[I].P) && A.Items[I].Piece == B.Items[I].Piece;
            if (!Same) { Passed = false; FirstUnfair = FString::Printf(TEXT("seed %d room %d is not repeatable"), Seed, Room); ++Unfair; }
            Shapes.Add(FMath::RoundToInt(A.Walkway.Last().Y / 100.f) * 1000 + A.Items.Num());
            Pieces += A.Items.Num();
        }
    if (Unfair) Detail = FString::Printf(TEXT("%d of 1600 rooms unfair, e.g. %s"), Unfair, *FirstUnfair);
    if (Passed && Shapes.Num() < 200) { Passed = false; Detail = TEXT("rooms are not varied enough"); }

    // 2. The built room matches its plan, with the water and a floor at the way in.
    const FPlan Plan = Make(SwampSeed, SwampRoomNumber);
    int32 Instances = 0, Boundary = 0, Water = 0, PlannedArms = 0;
    for (const FItem& Item : Plan.Items) PlannedArms += Item.Piece == EPiece::ZombieArm;
    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
    {
        if (It->ActorHasTag(TEXT("SwampPieces")))
            if (auto* Batch = It->FindComponentByClass<UHierarchicalInstancedStaticMeshComponent>()) Instances += Batch->GetInstanceCount();
        Boundary += It->ActorHasTag(TEXT("SwampBoundary"));
        Water += It->ActorHasTag(TEXT("SwampWater"));
    }
    if (Passed && (Instances + SwampArms.Num() != Plan.Items.Num() || SwampArms.Num() != PlannedArms || Boundary != 4 || Water != 1))
    { Passed = false; Detail = FString::Printf(TEXT("built room differs from plan (%d + %d arms of %d pieces, %d walls, %d water)"), Instances, SwampArms.Num(), Plan.Items.Num(), Boundary, Water); }
    FHitResult Floor;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(SwampFloor), false);
    if (Passed && !(GetWorld()->LineTraceSingleByChannel(Floor, FVector(Start.X, Start.Y, 600.f), FVector(Start.X, Start.Y, -400.f), ECC_Visibility, Query)
        && FMath::Abs(Floor.ImpactPoint.Z) < 30.f))
    { Passed = false; Detail = TEXT("no floor at the way in"); }
    // The boardwalk can be stood on: a trace down onto its first section hits its deck.
    for (const FItem& Item : Plan.Items)
    {
        if (!Passed || Item.Piece != EPiece::Boardwalk) continue;
        FHitResult Deck;
        if (!GetWorld()->LineTraceSingleByChannel(Deck, FVector(Item.P.X, Item.P.Y, 300.f), FVector(Item.P.X, Item.P.Y, -50.f), ECC_Visibility, Query) || Deck.ImpactPoint.Z < 15.f)
        { Passed = false; Detail = TEXT("the boardwalk cannot be stood on"); }
        break;
    }

    // 3. A risen arm hurts Hellgirl next to it, a sunken one does not, and wading makes ripples.
    if (Passed && SwampArms.Num())
    {
        FSwampArm& Arm = SwampArms[0];
        Hero->MaxHealth = Hero->Health = 1000.f;
        Hero->SetActorLocation(FVector(Arm.P.X + 80.f, Arm.P.Y, 100.f), false, nullptr, ETeleportType::TeleportPhysics);
        for (FSwampArm& A : SwampArms) A.Phase = SwampArt::ArmCycle - .5f - SwampClock * A.Rate;      // all under water
        SwampHurtClock = 0.f;
        TickSwamp(.02f);
        const float AfterSunk = Hero->Health;
        Arm.Phase = SwampArt::ArmRising + 1.f - SwampClock * Arm.Rate;                                // this one up
        SwampHurtClock = 0.f;
        TickSwamp(.02f);
        if (AfterSunk != 1000.f || Hero->Health >= AfterSunk)
        { Passed = false; Detail = FString::Printf(TEXT("arm damage wrong (health %.0f when sunk, %.0f when risen)"), AfterSunk, Hero->Health); }
        int32 Rings = 0;
        SwampSplash(FVector(Arm.P.X, Arm.P.Y, WaterZ), 1.f, 4);
        for (const FSwampRipple& R : SwampRipples) Rings += R.Age < R.Life;
        if (Passed && Rings < 5) { Passed = false; Detail = TEXT("no splash"); }
    }

    // 4. Hellgirl starts at the way in; the light only takes her on once she reaches it.
    Hero->SetActorLocation(FVector(Start.X, Start.Y, 115.f), false, nullptr, ETeleportType::TeleportPhysics);
    if (Passed && TickSwamp(0.f)) { Passed = false; Detail = TEXT("the light took her at the way in"); }
    Hero->SetActorLocation(ExitPosition + FVector(0, 0, 115.f), false, nullptr, ETeleportType::TeleportPhysics);
    if (Passed && !TickSwamp(0.f)) { Passed = false; Detail = TEXT("reaching the light does nothing"); }

    if (Passed) { UE_LOG(LogTemp, Display, TEXT("SWAMP CHECK PASSED: 1600 planned rooms fair and repeatable (%d pieces on average); room %d of seed %d built as planned (%d pieces, %d arms), arms hurt only when risen, splashes"),
        Pieces / 1600, SwampRoomNumber, SwampSeed, Plan.Items.Num(), SwampArms.Num()); }
    else { UE_LOG(LogTemp, Error, TEXT("SWAMP CHECK FAILED: %s"), *Detail); }
    FPlatformMisc::RequestExitWithStatus(false, Passed ? 0 : 1);
#endif
}
