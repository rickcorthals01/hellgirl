#include "Fighter/ArenaFighter.h"
#include "Levels/ArenaGameMode.h"
#include "Rules/CombatEnergyRules.h"
#include "Enemies/EnemySpawnPoint.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformMisc.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

// Explicit unattended regression mode. High-health subjects never die, and the
// entire check stays above the map so it cannot collect or create wallet coins.
void AArenaFighter::RunEnergyCheck()
{
#if WITH_DEV_AUTOMATION_TESTS
    if (bEnemy || UGameplayStatics::GetPlayerPawn(this, 0) != this) return;
    if (FParse::Param(FCommandLine::Get(), TEXT("HellgirlEnergyPreview")))
    {
        static int32 PreviewPhase = 0;
        static float PreviewStarted = 0.f;
        const float Now = GetWorld()->GetTimeSeconds();
        if (PreviewPhase == 0 && Now >= 2.f)
        {
            float PreviewEnergy = 80.f;
            FParse::Value(FCommandLine::Get(), TEXT("EnergyPreviewValue="), PreviewEnergy);
            Energy = FMath::Clamp(PreviewEnergy, 0.f, MaxEnergy);
            PreviewStarted = Now; PreviewPhase = 1;
        }
        if (PreviewPhase == 1 && Now - PreviewStarted >= .2f)
        {
            FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("Screenshots/EnergyMeter.png"), false, false);
            PreviewPhase = 2;
        }
        if (PreviewPhase == 2 && Now - PreviewStarted >= 1.f)
        {
            PreviewPhase = 3;
            UE_LOG(LogTemp, Display, TEXT("ENERGY PREVIEW CHECK PASSED: HUD capture requested at energy %.0f"), Energy);
            FPlatformMisc::RequestExitWithStatus(false, 0);
        }
        return;
    }
    if (!FParse::Param(FCommandLine::Get(), TEXT("HellgirlEnergyCheck")) || GetWorld()->GetTimeSeconds() < 1.f) return;
    static bool Done = false;
    if (Done) return;
    Done = true; // Some cases intentionally run the real actor Tick for buffering.
    auto Check = [](bool Good, const TCHAR* Reason)
    {
        if (!Good)
        {
            UE_LOG(LogTemp, Error, TEXT("ENERGY CHECK FAILED: %s"), Reason);
            FPlatformMisc::RequestExitWithStatus(false, 1);
        }
        return Good;
    };
    if (!Check(Energy == 0.f && MaxEnergy == HellgirlEnergy::Capacity, TEXT("Player starts with zero energy and a 100-point capacity"))) return;

    if (auto* GM = Cast<AArenaGameMode>(UGameplayStatics::GetGameMode(this)))
        for (AEnemySpawnPoint* Site : GM->GetSpawnSites()) Site->SetActorTickEnabled(false);
    const FVector Origin = GetActorLocation() + FVector(0.f, 0.f, 3000.f);
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Floor = GetWorld()->SpawnActor<AStaticMeshActor>(Origin - FVector(0.f,0.f,140.f), FRotator::ZeroRotator, Params);
    if (!Check(Floor != nullptr, TEXT("Isolated test floor spawned"))) return;
    Floor->SetMobility(EComponentMobility::Movable);
    Floor->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
    Floor->SetActorScale3D(FVector(100.f,100.f,1.f));
    Floor->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
    auto* First = GetWorld()->SpawnActor<AArenaFighter>(Origin + FVector(130.f,55.f,0.f), FRotator::ZeroRotator, Params);
    auto* Second = GetWorld()->SpawnActor<AArenaFighter>(Origin + FVector(130.f,-55.f,0.f), FRotator::ZeroRotator, Params);
    if (!Check(First != nullptr && Second != nullptr, TEXT("Energy hit subjects spawned"))) return;
    for (auto* Target : {First, Second})
    {
        Target->MakeEnemy(1, false);
        Target->SetActorTickEnabled(false);
        Target->GetCharacterMovement()->SetComponentTickEnabled(false);
    }
    auto* Movement = GetCharacterMovement();
    auto Targets = [&](bool InRange)
    {
        int32 Index = 0;
        for (auto* Target : {First, Second})
        {
            Target->ResetAfterRecovery();
            Target->Health = Target->MaxHealth = 10000.f;
            Target->HitClock = Target->DodgeClock = Target->KnockdownClock = 0.f;
            Target->bHitResolved = true;
            Target->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
            Target->SetActorLocation(Origin + FVector(InRange ? 130.f : 3000.f, Index++ ? -55.f : 55.f, 0.f), false, nullptr, ETeleportType::TeleportPhysics);
            Target->SetActorRotation((Origin - Target->GetActorLocation()).Rotation());
        }
    };
    auto Reset = [&](float NewEnergy, bool Airborne = false)
    {
        ResetAfterRecovery();
        Health = MaxHealth = 10000.f;
        Energy = NewEnergy;
        Stamina = 100.f;
        HitClock = KnockdownClock = AttackClock = DodgeClock = DodgeCooldown = 0.f;
        ComboClock = PostDodgeClock = BufferClock = ImpactTimeout = 0.f;
        Combo = AirCombo = 0;
        bAirFinisherUsed = bCounterDodge = bGroundImpactPending = false;
        bBlockHeld = bSprintHeld = bWalkHeld = false;
        bHitResolved = true;
        AirHangBudget = 2.4f;
        AirTarget.Reset(); CombatTarget.Reset();
        SetActorLocation(Origin, false, nullptr, ETeleportType::TeleportPhysics);
        SetActorRotation(FRotator::ZeroRotator);
        if (Controller) Controller->SetControlRotation(FRotator(-20.f,0.f,0.f));
        Movement->SetMovementMode(Airborne ? MOVE_Falling : MOVE_Walking);
        Movement->ClearAccumulatedForces();
        Movement->StopMovementImmediately();
        Targets(true);
    };

    Reset(0.f);
    Attack();
    if (!Check(AttackClock > 0.f && CurrentAttack.Type == FistCombat::Move::RightPunch && Energy == 0.f,
        TEXT("Normal punches remain available at zero energy"))) return;
    ResolveAttack();
    if (!Check(First->Health < 10000.f && Second->Health < 10000.f && Energy == 12.f,
        TEXT("One normal swing hitting two enemies grants 12 total energy"))) return;
    First->HitClock = Second->HitClock = 0.f;
    ResolveAttack();
    if (!Check(Energy == 12.f, TEXT("Resolving the same swing again cannot grant duplicate energy"))) return;

    Reset(0.f);
    HeavyAttack(); ReleaseHeavyAttack();
    if (!Check(CurrentAttack.Type == FistCombat::Move::HeavyPunch && AttackClock > 0.f && Energy == 0.f,
        TEXT("An uncharged heavy tap remains free at zero energy"))) return;
    ResolveAttack();
    if (!Check(Energy == 18.f, TEXT("A damaging heavy swing grants 18 energy once"))) return;

    Reset(0.f, true);
    Attack(); ResolveAttack();
    if (!Check(CurrentAttack.Type == FistCombat::Move::AirPunch && Energy == 12.f,
        TEXT("An ordinary aerial hit remains free and builds energy"))) return;

    Reset(23.f); Targets(false);
    Attack(); ResolveAttack();
    if (!Check(Energy == 23.f, TEXT("A missed basic attack grants no energy"))) return;
    Reset(23.f);
    First->HitClock = Second->HitClock = 1.f;
    Attack(); ResolveAttack();
    if (!Check(Energy == 23.f && First->Health == 10000.f && Second->Health == 10000.f,
        TEXT("An immune target grants no energy without actual damage"))) return;
    Reset(95.f); Attack(); ResolveAttack();
    if (!Check(Energy == MaxEnergy && Energy == 100.f, TEXT("Successful hits cap energy at 100"))) return;
    UE_LOG(LogTemp, Display, TEXT("ENERGY CHECK gains passed: free ground/heavy/air attacks, actual hits only, one gain per swing, capacity cap"));

    Reset(39.f);
    Movement->Velocity = FVector(200.f,0.f,0.f);
    const FVector BeforeRejectedCharge = Movement->Velocity;
    HeavyAttack(); ChargeClock = .4f; ReleaseHeavyAttack();
    if (!Check(Energy == 39.f && AttackClock == 0.f && GroundDashClock == 0.f
        && Movement->Velocity.Equals(BeforeRejectedCharge) && Movement->PendingLaunchVelocity.IsNearlyZero(),
        TEXT("An unaffordable charged release spends nothing and cannot dash or become a free heavy"))) return;

    Reset(100.f);
    HeavyAttack();
    Tick(.25f);
    if (!Check(bHeavyHeld && ChargeClock >= .2f && Energy == 100.f && AttackClock == 0.f,
        TEXT("Holding a charge does not spend energy"))) return;
    ReleaseHeavyAttack();
    if (!Check(CurrentAttack.Type == FistCombat::Move::ChargedStrike && AttackClock > 0.f && Energy == 60.f,
        TEXT("An accepted charged attack pays exactly 40 on release"))) return;
    ResolveAttack();
    if (!Check(First->Health < 10000.f && Energy == 60.f, TEXT("Charged area damage never refunds energy through hit gain"))) return;
    Dodge();
    if (!Check(AttackClock == 0.f && GroundDashClock == 0.f && Energy == 60.f,
        TEXT("Dodge cancellation does not refund a paid charge"))) return;

    Reset(40.f); Targets(false);
    HeavyAttack(); ChargeClock = .4f; ReleaseHeavyAttack(); ResolveAttack();
    if (!Check(Energy == 0.f && Energy >= 0.f, TEXT("A paid charge miss still costs 40 and cannot make energy negative"))) return;
    Reset(73.f);
    HeavyAttack(); ChargeClock = .4f; PrepareForPause();
    if (!Check(Energy == 73.f && !bHeavyHeld && PendingCharge == 0.f && AttackClock == 0.f,
        TEXT("Canceling a held charge before release is free"))) return;

    Reset(80.f);
    AttackClock = .1f; bHitResolved = true;
    PendingCharge = 1.f;
    QueueAttack(true);
    if (!Check(BufferClock > 0.f && Energy == 80.f, TEXT("Buffering a charged attack does not pay its cost yet"))) return;
    Tick(.11f);
    if (!Check(CurrentAttack.Type == FistCombat::Move::ChargedStrike && AttackClock > 0.f && Energy == 40.f,
        TEXT("A buffered charge pays exactly once when it begins"))) return;
    UE_LOG(LogTemp, Display, TEXT("ENERGY CHECK charge passed: rejected release, hold, accepted payment, miss/cancel, buffer timing"));

    Reset(34.f, true);
    Movement->Velocity = FVector(150.f,0.f,50.f);
    const FVector BeforeRejectedSlam = Movement->Velocity;
    HeavyAttack();
    if (!Check(Energy == 34.f && AttackClock == 0.f && !bGroundImpactPending && !bAirFinisherUsed
        && Movement->Velocity.Equals(BeforeRejectedSlam) && Movement->PendingLaunchVelocity.IsNearlyZero(),
        TEXT("An unaffordable explicit air slam does not spend, dive, or use the finisher"))) return;
    Reset(35.f, true); HeavyAttack();
    if (!Check(CurrentAttack.Type == FistCombat::Move::AirSlam && bGroundImpactPending && Energy == 0.f,
        TEXT("An affordable air slam pays exactly 35 when it begins"))) return;
    Movement->SetMovementMode(MOVE_Walking);
    Landed(FHitResult());
    if (!Check(First->Health < 10000.f && Energy == 0.f, TEXT("A slam landing deals damage without generating energy"))) return;
    Reset(35.f, true); HeavyAttack(); Dodge();
    if (!Check(Energy == 0.f && !bGroundImpactPending && AttackClock == 0.f,
        TEXT("Dodging out of a paid slam cancels its impact without a refund"))) return;

    Reset(0.f, true); AirCombo = 3; Attack();
    if (!Check(CurrentAttack.Type == FistCombat::Move::AirKick && CurrentAttack.NextCombo == 0
        && !bGroundImpactPending && !bAirFinisherUsed && Energy == 0.f,
        TEXT("The fourth normal air input falls back to a free basic kick at zero energy"))) return;
    ResolveAttack();
    if (!Check(Energy == 12.f, TEXT("Fallback aerial kick can rebuild energy"))) return;
    Reset(35.f, true); AirCombo = 3; Attack();
    if (!Check(CurrentAttack.Type == FistCombat::Move::AirCrashKick && bGroundImpactPending && Energy == 0.f,
        TEXT("The fourth normal air input uses and pays for the crash kick when affordable"))) return;
    Movement->SetMovementMode(MOVE_Walking); Landed(FHitResult());
    if (!Check(Energy == 0.f && First->Health < 10000.f, TEXT("Paid crash impact cannot generate energy"))) return;
    UE_LOG(LogTemp, Display, TEXT("ENERGY CHECK aerial passed: slam affordability, spend/cancel, free fourth-hit fallback, paid crash"));

    Reset(57.f);
    First->AttackClock = .6f;
    First->CurrentAttack = {FistCombat::Move::EnemyClaw, 1.25f,.6f,9.f,165.f,200.f,0.f,0};
    First->bHitResolved = false;
    Dodge();
    if (!Check(bCounterDodge && First->Health < 10000.f && Energy == 57.f,
        TEXT("A perfect counter does not generate energy"))) return;
    Reset(57.f);
    ApplyPhysicsDamage(1.f, FVector::ZeroVector);
    if (!Check(Energy == 57.f, TEXT("Physics collision damage does not grant or spend energy"))) return;
    ResetAfterRecovery();
    if (!Check(Energy == 57.f, TEXT("Recovery preserves the current energy amount"))) return;
    Reset(23.f); Targets(false); Tick(.2f);
    if (!Check(Energy == 23.f, TEXT("Energy has no passive regeneration"))) return;

    Reset(0.f);
    First->Energy = 0.f;
    First->StartAttack(true);
    if (!Check(First->CurrentAttack.Type == FistCombat::Move::EnemyClaw && First->AttackClock > 0.f && First->Energy == 0.f,
        TEXT("Enemy attacks remain independent of player energy costs"))) return;
    First->ResolveAttack();
    if (!Check(Health < 10000.f && First->Energy == 0.f && Energy == 0.f,
        TEXT("Enemy damage neither generates enemy energy nor player energy"))) return;
    First->Destroy(); Second->Destroy(); Floor->Destroy();
    UE_LOG(LogTemp, Display, TEXT("ENERGY CHECK PASSED: actual hit gains, one award per swing, capacity, charge/slam costs, rejected input, buffering, misses, interrupts, aerial fallback, counters/collisions, recovery, enemy independence"));
    FPlatformMisc::RequestExitWithStatus(false, 0);
#endif
}
