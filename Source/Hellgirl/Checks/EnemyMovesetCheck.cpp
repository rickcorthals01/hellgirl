#include "Fighter/ArenaFighter.h"
#include "Levels/ArenaGameMode.h"
#include "Enemies/EnemySpawnPoint.h"
#include "Progress/CoinPickup.h"
#include "Bosses/ImpCommanderBehavior.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/PlatformMisc.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

// Run real attack timing, summon spawning, damage and movement against an
// isolated floor. Quiet actors are stepped explicitly to avoid random AI and
// frame-order dependencies. All rewards remain far from the possessed player.
void AArenaFighter::RunEnemyMovesetCheck(float Dt)
{
#if WITH_DEV_AUTOMATION_TESTS
    if (bEnemy || UGameplayStatics::GetPlayerPawn(this, 0) != this
        || !FParse::Param(FCommandLine::Get(), TEXT("HellgirlEnemyMovesetCheck"))
        || GetWorld()->GetTimeSeconds() < 1.f) return;
    static bool Finished = false;
    if (Finished) return;
    Finished = true;
    int32 Step = 0;
    auto Check = [&](bool Good, const TCHAR* Reason)
    {
        if (!Good)
        {
            UE_LOG(LogTemp, Error, TEXT("ENEMY MOVESET CHECK FAILED step=%d: %s"), Step, Reason);
            FPlatformMisc::RequestExitWithStatus(false, 1);
        }
        return Good;
    };
    auto* GameMode = Cast<AArenaGameMode>(UGameplayStatics::GetGameMode(this));
    if (!Check(GameMode != nullptr, TEXT("Test requires the arena game mode"))) return;
    GameMode->SetActorTickEnabled(false);
    for (AEnemySpawnPoint* Site : GameMode->GetSpawnSites())
    {
        Site->SetActorTickEnabled(false);
        Site->bEnabled = false;
    }
    for (TActorIterator<AArenaFighter> It(GetWorld()); It; ++It)
        if (It->bEnemy) It->Destroy();
    GetCharacterMovement()->StopMovementImmediately();
    GetCharacterMovement()->SetComponentTickEnabled(false);

    const FVector FloorCenter(0.f, -16000.f, 3500.f);
    const FVector TargetPosition = FloorCenter + FVector(0.f, 0.f, 138.f);
    const FVector BossPosition = TargetPosition - FVector(800.f, 0.f, 0.f);
    FActorSpawnParameters SpawnParameters;
    SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* TestFloor = GetWorld()->SpawnActor<AStaticMeshActor>(FloorCenter, FRotator::ZeroRotator, SpawnParameters);
    if (!Check(TestFloor != nullptr, TEXT("Isolated test floor spawned"))) return;
    TestFloor->SetMobility(EComponentMobility::Movable);
    TestFloor->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
    TestFloor->SetActorScale3D(FVector(100.f, 80.f, 1.f));
    TestFloor->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));

    auto Quiet = [](AArenaFighter* Fighter)
    {
        Fighter->SetActorTickEnabled(false);
        Fighter->GetCharacterMovement()->StopMovementImmediately();
        Fighter->GetCharacterMovement()->SetComponentTickEnabled(false);
    };
    auto Subject = [&](EHellgirlEnemyType Type, const FVector& Position)
    {
        auto* Fighter = GetWorld()->SpawnActor<AArenaFighter>(Position, FRotator::ZeroRotator, SpawnParameters);
        if (!Fighter) return Fighter;
        Fighter->MakeEnemy(1, Type == EHellgirlEnemyType::FlyingImps);
        Fighter->SetEnemyType(Type);
        Fighter->Health = Fighter->MaxHealth = Type == EHellgirlEnemyType::ImpCommander ? 450.f : 200.f;
        Fighter->HomePosition = Position - FVector(0.f, 0.f, 88.f);
        Fighter->EnemyMoveCooldown = 0.f;
        Fighter->EnemyLastAttackTime = -100.f;
        Fighter->GetCharacterMovement()->SetMovementMode(Type == EHellgirlEnemyType::FlyingImps ? MOVE_Flying : MOVE_Walking);
        Quiet(Fighter);
        return Fighter;
    };
    auto* Target = GetWorld()->SpawnActor<AArenaFighter>(TargetPosition, FRotator::ZeroRotator, SpawnParameters);
    if (!Check(Target != nullptr, TEXT("Unpossessed combat target spawned away from wallet owner"))) return;
    Target->MaxHealth = Target->Health = 1000.f;
    Target->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    Quiet(Target);

    auto Advance = [](AArenaFighter* Fighter, float Seconds)
    {
        while (Seconds > UE_KINDA_SMALL_NUMBER)
        {
            const float Slice = FMath::Min(Seconds, 1.f / 120.f);
            Fighter->UpdateEnemyMoveMotion(Slice);
            Fighter->UpdateAttackTiming(Slice);
            Seconds -= Slice;
        }
    };
    auto MakeSite = [&](AArenaFighter* Commander)
    {
        auto* Site = GetWorld()->SpawnActor<AEnemySpawnPoint>(Commander->HomePosition, FRotator::ZeroRotator, SpawnParameters);
        if (!Site) return Site;
        Site->SetActorTickEnabled(false);
        Site->bBoss = Site->bActivated = Site->bEnabled = true;
        Site->EnemyCount = 0;
        Site->RegisterReinforcement(Commander);
        return Site;
    };
    auto QuietAdds = [&](AArenaFighter* Commander)
    {
        for (const auto& Imp : Commander->GetBoss<UImpCommanderBehavior>()->Imps)
            if (Imp.IsValid()) Quiet(Imp.Get());
    };
    auto CountCoins = [&]()
    {
        int32 Count = 0;
        // Hearts are a separate random drop; only coins track one-per-defeat.
        for (TActorIterator<ACoinPickup> It(GetWorld()); It; ++It) if (!It->IsHeart()) ++Count;
        return Count;
    };

    // Steps 1, 2 and 4 covered the phase-two summon replaced on September 19;
    // BossDesignCheck covers the current protective-Imp shield and jump-slam refill.
    Step = 3;
    auto* Commander = Subject(EHellgirlEnemyType::ImpCommander, BossPosition);
    if (!Check(Commander != nullptr, TEXT("Commander subject spawned"))) return;
    Commander->bBossEncounter = true;
    auto* BossSite = MakeSite(Commander);
    if (!Check(BossSite != nullptr && BossSite->LivingEnemies() == 1, TEXT("Boss encounter owns its Commander"))) return;
    auto* Commanding = Commander->GetBoss<UImpCommanderBehavior>();
    if (!Check(Commanding != nullptr, TEXT("Commander has his boss behaviour"))) return;
    Commanding->SpawnImps();
    QuietAdds(Commander);
    if (!Check(Commanding->GetSummonedImpCount() == 3 && BossSite->LivingEnemies() == 4, TEXT("Protective Imps join the boss encounter"))) return;
    for (const auto& Imp : Commanding->Imps)
    {
        if (!Check(Imp.IsValid() && Imp->IsAlive() && Imp->EnemyType == EHellgirlEnemyType::Imps
            && !Imp->bFlyingEnemy && Imp->MaxHealth < Commander->MaxHealth && Imp->EncounterSite.Get() == BossSite
            && Imp->GetMesh()->GetSkeletalMeshAsset() != nullptr,
            TEXT("Each reinforcement has ordinary Imp identity, model, health and encounter ownership"))) return;
    }
    BossSite->RegisterReinforcement(Commanding->Imps[0].Get());
    if (!Check(BossSite->LivingEnemies() == 4, TEXT("Registering an existing reinforcement does not duplicate encounter counts"))) return;
    Commanding->SpawnImps();
    if (!Check(Commanding->GetSummonedImpCount() == 3 && BossSite->LivingEnemies() == 4, TEXT("Summoning cannot exceed the three-living-Imp cap"))) return;
    const int32 KillsBefore = GameMode->Kills;
    const int32 CoinsBefore = CountCoins();
    Commander->ApplyPhysicsDamage(10000.f, FVector(100.f, 0.f, 30.f));
    Commander->ApplyPhysicsDamage(10000.f, FVector(100.f, 0.f, 30.f));
    if (!Check(GameMode->Kills == KillsBefore + 1 && CountCoins() == CoinsBefore + 1, TEXT("Commander collision death awards one kill and one coin drop"))) return;
    BossSite->Tick(.01f);
    if (!Check(!BossSite->bCleared && BossSite->LivingEnemies() == 3, TEXT("Living summoned Imps keep the boss encounter uncleared"))) return;
    for (const auto& Imp : Commanding->Imps)
        if (Imp.IsValid() && Imp->IsAlive()) Imp->ApplyPhysicsDamage(10000.f, FVector(70.f, 0.f, 20.f));
    BossSite->Tick(.01f);
    if (!Check(BossSite->bCleared && BossSite->LivingEnemies() == 0, TEXT("The final reinforcement death clears the boss encounter"))) return;
    Commander->Destroy();
    BossSite->Destroy();

    Step = 5;
    for (const bool Flying : {false, true})
    {
        Target->SetActorLocation(TargetPosition, false, nullptr, ETeleportType::TeleportPhysics);
        Target->Health = Target->MaxHealth;
        const FVector Start = TargetPosition + FVector(450.f, 0.f, Flying ? 260.f : 0.f);
        auto* Attacker = Subject(Flying ? EHellgirlEnemyType::FlyingImps : EHellgirlEnemyType::Imps, Start);
        if (!Check(Attacker != nullptr, TEXT("Pounce/dive subject spawned"))) return;
        Attacker->BeginEnemyMove(Flying ? EEnemyMove::FlyingDive : EEnemyMove::ImpPounce, Target);
        if (!Check(Attacker->AttackClock > 0.f, TEXT("Pounce/dive commitment starts"))) return;
        const FVector LockedTarget = Attacker->EnemyMoveTarget;
        const FVector LockedFacing = Attacker->EnemyFacing;
        const float ContactTime = Attacker->CurrentAttack.Duration * Attacker->CurrentAttack.ContactFraction;
        if (!Check(!Target->CanCounter(Attacker), TEXT("Early pounce/dive wind-up cannot be countered"))) return;
        Target->SetActorLocation(TargetPosition + FVector(0.f, 550.f, 0.f), false, nullptr, ETeleportType::TeleportPhysics);
        const float TargetHealthBefore = Target->Health;
        Advance(Attacker, ContactTime + .04f);
        if (!Check(Attacker->EnemyMoveTarget.Equals(LockedTarget, .1f) && Attacker->EnemyFacing.Equals(LockedFacing, .001f)
            && FVector::Dist2D(Start, Attacker->GetActorLocation()) > 200.f
            && FMath::Abs(Attacker->GetActorLocation().Y - Start.Y) < 2.f,
            TEXT("Committed pounce/dive travels toward its original lane without tracking an evasive target"))) return;
        if (!Check(FMath::IsNearlyEqual(Target->Health, TargetHealthBefore) && Attacker->bHitResolved
            && Attacker->AttackClock > 0.f && !Attacker->CanBeginEnemyMove(Target),
            TEXT("An evaded pounce/dive misses once and leaves punishable recovery"))) return;
        const float RecoveryZ = Attacker->GetActorLocation().Z;
        Advance(Attacker, .3f);
        if (Flying && !Check(Start.Z - RecoveryZ > 120.f && FMath::Abs(Attacker->GetActorLocation().Z - RecoveryZ) < 3.f,
            TEXT("Flying Imp descends and remains low through its recovery"))) return;
        Attacker->Destroy();
    }

    Step = 6;
    for (const bool Flying : {false, true})
    {
        Target->ResetAfterRecovery();
        Target->SetActorLocation(TargetPosition, false, nullptr, ETeleportType::TeleportPhysics);
        Target->Health = Target->MaxHealth;
        Target->Stamina = 100.f;
        Target->HitClock = Target->KnockdownClock = Target->DodgeCooldown = 0.f;
        Target->bCounterDodge = false;
        auto* Attacker = Subject(Flying ? EHellgirlEnemyType::FlyingImps : EHellgirlEnemyType::Imps,
            TargetPosition + FVector(450.f, 0.f, Flying ? 260.f : 0.f));
        if (!Check(Attacker != nullptr, TEXT("Counter subject spawned"))) return;
        Attacker->BeginEnemyMove(Flying ? EEnemyMove::FlyingDive : EEnemyMove::ImpPounce, Target);
        if (!Check(Attacker->AttackClock > 0.f, TEXT("Counter subject commits its move"))) return;
        Advance(Attacker, Attacker->CurrentAttack.Duration * Attacker->CurrentAttack.ContactFraction - .08f);
        if (!Check(Target->CanCounter(Attacker), TEXT("A close pounce/dive is counterable just before contact"))) return;
        const float AttackerHealthBefore = Attacker->Health;
        const float TargetHealthBefore = Target->Health;
        Target->Dodge();
        if (!Check(Target->bCounterDodge && Attacker->Health < AttackerHealthBefore
            && Attacker->IsCombatLaunched() && Attacker->AttackClock <= 0.f,
            TEXT("Actual dodge input counters and interrupts the pounce/dive"))) return;
        Advance(Attacker, .5f);
        if (!Check(FMath::IsNearlyEqual(Target->Health, TargetHealthBefore), TEXT("Cancelled pounce/dive cannot land a delayed hit"))) return;
        Attacker->Destroy();
    }

    Step = 7;
    Target->ResetAfterRecovery();
    Target->HitClock = Target->KnockdownClock = 0.f;
    Target->SetActorLocation(TargetPosition, false, nullptr, ETeleportType::TeleportPhysics);
    TArray<AArenaFighter*> Coordinated;
    for (int32 Index = 0; Index < 3; ++Index)
    {
        const float Angle = Index * 2.f * PI / 3.f;
        auto* Imp = Subject(EHellgirlEnemyType::Imps, TargetPosition + FVector(FMath::Cos(Angle) * 160.f, FMath::Sin(Angle) * 160.f, 0.f));
        if (!Check(Imp != nullptr, TEXT("Coordinated Imp subject spawned"))) return;
        Coordinated.Add(Imp);
    }
    if (!Check(Coordinated[0]->CanBeginEnemyMove(Target), TEXT("An idle group can begin its first attack"))) return;
    Coordinated[0]->BeginEnemyMove(EEnemyMove::ImpClaw, Target);
    if (!Check(!Coordinated[1]->CanBeginEnemyMove(Target), TEXT("Another attack cannot start inside the group spacing window"))) return;
    Coordinated[0]->EnemyLastAttackTime = GetWorld()->GetTimeSeconds() - .46f;
    if (!Check(Coordinated[1]->CanBeginEnemyMove(Target), TEXT("A second commitment is permitted after attack spacing"))) return;
    Coordinated[1]->BeginEnemyMove(EEnemyMove::ImpClaw, Target);
    Coordinated[1]->EnemyLastAttackTime = GetWorld()->GetTimeSeconds() - .46f;
    if (!Check(!Coordinated[2]->CanBeginEnemyMove(Target), TEXT("A third simultaneous commitment is rejected even after spacing"))) return;
    Advance(Coordinated[0], 1.3f);
    if (!Check(Coordinated[2]->CanBeginEnemyMove(Target), TEXT("Finishing recovery frees one of the two commitment slots"))) return;
    for (auto* Imp : Coordinated) Imp->Destroy();

    auto ResetTarget = [&]()
    {
        Target->ResetAfterRecovery();
        Target->SetActorLocation(TargetPosition, false, nullptr, ETeleportType::TeleportPhysics);
        Target->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
        Target->Health = Target->MaxHealth;
        Target->DodgeClock = Target->DodgeCooldown = Target->HitClock = Target->KnockdownClock = 0.f;
        Target->bBlockHeld = Target->bCounterDodge = false;
    };
    auto BlockingWall = [&](const FVector& Position)
    {
        auto* Wall = GetWorld()->SpawnActor<AStaticMeshActor>(Position, FRotator::ZeroRotator, SpawnParameters);
        if (!Wall) return Wall;
        Wall->SetMobility(EComponentMobility::Movable);
        Wall->GetStaticMeshComponent()->SetStaticMesh(TestFloor->GetStaticMeshComponent()->GetStaticMesh());
        Wall->SetActorScale3D(FVector(.15f, 3.f, 3.f));
        Wall->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
        return Wall;
    };

    Step = 8;
    for (const EEnemyMove Move : {EEnemyMove::ImpPounce, EEnemyMove::FlyingDive, EEnemyMove::CommanderCleave,
        EEnemyMove::CommanderRush, EEnemyMove::CommanderSlam})
    {
        ResetTarget();
        const bool Flying = Move == EEnemyMove::FlyingDive;
        const EHellgirlEnemyType Type = Flying ? EHellgirlEnemyType::FlyingImps
            : Move == EEnemyMove::ImpPounce ? EHellgirlEnemyType::Imps : EHellgirlEnemyType::ImpCommander;
        const float Distance = Move == EEnemyMove::CommanderRush ? 750.f
            : (Move == EEnemyMove::ImpPounce || Flying) ? 450.f : 250.f;
        auto* Attacker = Subject(Type, TargetPosition + FVector(Distance, 0.f, Flying ? 260.f : 0.f));
        if (!Check(Attacker != nullptr, TEXT("Stationary-target contact subject spawned"))) return;
        Attacker->BeginEnemyMove(Move, Target);
        if (!Check(Attacker->AttackClock > 0.f, TEXT("Damaging commitment starts before the contact check"))) return;
        const float ContactTime = Attacker->CurrentAttack.Duration * Attacker->CurrentAttack.ContactFraction;
        const float ExpectedDamage = Attacker->CurrentAttack.Damage;
        const float HealthBefore = Target->Health;
        Advance(Attacker, ContactTime - .02f);
        if (!Check(FMath::IsNearlyEqual(Target->Health, HealthBefore), TEXT("Pounce, dive and Commander strikes do not damage before contact"))) return;
        Advance(Attacker, .04f);
        if (!Check(ExpectedDamage > 0.f && FMath::IsNearlyEqual(Target->Health, HealthBefore - ExpectedDamage)
            && Attacker->bHitResolved && Attacker->AttackClock > 0.f,
            *FString::Printf(TEXT("Move %d damages a stationary target once at actual contact"), static_cast<int32>(Move)))) return;
        Advance(Attacker, Attacker->CurrentAttack.Duration);
        if (!Check(FMath::IsNearlyEqual(Target->Health, HealthBefore - ExpectedDamage), TEXT("Completed recovery cannot repeat strike damage"))) return;
        Attacker->Destroy();
    }

    Step = 9;
    for (int32 Scenario = 0; Scenario < 3; ++Scenario)
    {
        ResetTarget();
        auto* Slammer = Subject(EHellgirlEnemyType::ImpCommander, TargetPosition + FVector(200.f, 0.f, 0.f));
        if (!Check(Slammer != nullptr, TEXT("Slam coverage subject spawned"))) return;
        Slammer->BeginEnemyMove(EEnemyMove::CommanderSlam, Target);
        if (!Check(Slammer->AttackClock > 0.f, TEXT("Commander commits slam before the target moves or a wall appears"))) return;
        const float HealthBefore = Target->Health;
        const float ExpectedDamage = Slammer->CurrentAttack.Damage;
        AStaticMeshActor* Wall = nullptr;
        if (Scenario == 0)
        {
            Target->SetActorLocation(Slammer->GetActorLocation() + FVector(200.f, 0.f, 0.f), false, nullptr, ETeleportType::TeleportPhysics);
            if (!Check(FVector::DotProduct(Slammer->GetActorForwardVector(),
                (Target->GetActorLocation() - Slammer->GetActorLocation()).GetSafeNormal2D()) < -.9f,
                TEXT("Area-hit subject is behind the Commander's locked facing"))) return;
        }
        else if (Scenario == 1)
            Target->SetActorLocation(Slammer->GetActorLocation() + FVector(600.f, 0.f, 0.f), false, nullptr, ETeleportType::TeleportPhysics);
        else
        {
            Wall = BlockingWall(TargetPosition + FVector(100.f, 0.f, 0.f));
            if (!Check(Wall != nullptr, TEXT("Wall appears between an in-range target and a committed slam"))) return;
        }
        Advance(Slammer, Slammer->CurrentAttack.Duration * Slammer->CurrentAttack.ContactFraction + .04f);
        const float ExpectedHealth = Scenario == 0 ? HealthBefore - ExpectedDamage : HealthBefore;
        if (!Check(FMath::IsNearlyEqual(Target->Health, ExpectedHealth) && Slammer->bHitResolved,
            Scenario == 0 ? TEXT("Commander slam hits a target behind its facing")
                : Scenario == 1 ? TEXT("Commander slam misses beyond its five-hundred-unit radius")
                : TEXT("A wall blocks slam damage even when the target is inside its radius"))) return;
        if (Wall) Wall->Destroy();
        Slammer->Destroy();
    }

    Step = 10;
    ResetTarget();
    auto* Rusher = Subject(EHellgirlEnemyType::ImpCommander, TargetPosition + FVector(750.f, 0.f, 0.f));
    if (!Check(Rusher != nullptr, TEXT("Blocked-rush subject spawned"))) return;
    Rusher->BeginEnemyMove(EEnemyMove::CommanderRush, Target);
    if (!Check(Rusher->AttackClock > 0.f, TEXT("Rush commits before obstruction appears"))) return;
    auto* RushWall = BlockingWall(TargetPosition + FVector(300.f, 0.f, 0.f));
    if (!Check(RushWall != nullptr, TEXT("Rush obstruction spawned"))) return;
    const float HealthBeforeRush = Target->Health;
    Advance(Rusher, Rusher->CurrentAttack.Duration * Rusher->CurrentAttack.ContactFraction + .04f);
    const FVector BlockedRushPosition = Rusher->GetActorLocation();
    if (!Check(Rusher->bEnemyMoveMotionStopped && BlockedRushPosition.X > RushWall->GetActorLocation().X
        && FMath::IsNearlyEqual(Target->Health, HealthBeforeRush), TEXT("Commander rush stops on the near side of a wall without damaging through it"))) return;
    Advance(Rusher, .3f);
    if (!Check(Rusher->GetActorLocation().Equals(BlockedRushPosition, .5f), TEXT("Blocked rush cannot resume moving during recovery"))) return;
    RushWall->Destroy();
    Rusher->Destroy();
    Target->Destroy();
    TestFloor->Destroy();

    UE_LOG(LogTemp, Display, TEXT("ENEMY MOVESET CHECK PASSED: protective-Imp cap, encounter ownership, exactly-once rewards, locked pounce/dive, miss/recovery, dodge counters, two-attack cap and spacing, five damaging moves, slam rear/range/wall checks, blocked rush"));
    FPlatformMisc::RequestExitWithStatus(false, 0);
#endif
}
