#include "Fighter/ArenaFighter.h"
#include "Levels/ArenaGameMode.h"
#include "Fighter/CombatImpactBudget.h"
#include "Enemies/EnemySpawnPoint.h"
#include "Components/CapsuleComponent.h"
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

// Real accepted attacks, contact queries and queued impact processing. The
// isolated unpossessed player cannot collect the fixture's death rewards.
void AArenaFighter::RunCombatBalanceCheck(float Dt)
{
#if WITH_DEV_AUTOMATION_TESTS
    if (bEnemy || UGameplayStatics::GetPlayerPawn(this, 0) != this
        || !FParse::Param(FCommandLine::Get(), TEXT("HellgirlCombatBalanceCheck"))
        || GetWorld()->GetTimeSeconds() < 1.f) return;
    static bool Done = false;
    if (Done) return;
    Done = true;
    int32 Step = 0;
    auto Check = [&](bool Good, const TCHAR* Reason)
    {
        if (!Good)
        {
            UE_LOG(LogTemp, Error, TEXT("COMBAT BALANCE CHECK FAILED step=%d: %s"), Step, Reason);
            FPlatformMisc::RequestExitWithStatus(false, 1);
        }
        return Good;
    };
    auto* GameMode = Cast<AArenaGameMode>(UGameplayStatics::GetGameMode(this));
    if (!Check(GameMode != nullptr, TEXT("Arena game mode exists"))) return;
    GameMode->SetActorTickEnabled(false);
    for (auto Site : GameMode->GetSpawnSites()) Site->SetActorTickEnabled(false);
    for (TActorIterator<AArenaFighter> It(GetWorld()); It; ++It)
        if (It->bEnemy) It->Destroy();
    GetCharacterMovement()->StopMovementImmediately();
    GetCharacterMovement()->SetComponentTickEnabled(false);

    const FVector Origin(0.f, -18000.f, 3638.f);
    FActorSpawnParameters Parameters;
    Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Floor = GetWorld()->SpawnActor<AStaticMeshActor>(Origin - FVector(0.f, 0.f, 138.f), FRotator::ZeroRotator, Parameters);
    if (!Check(Floor != nullptr, TEXT("Isolated balance-test floor spawned"))) return;
    Floor->SetMobility(EComponentMobility::Movable);
    Floor->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
    Floor->SetActorScale3D(FVector(100.f, 80.f, 1.f));
    Floor->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
    auto Quiet = [](AArenaFighter* Fighter)
    {
        Fighter->SetActorTickEnabled(false);
        Fighter->GetCharacterMovement()->SetComponentTickEnabled(false);
        Fighter->GetCharacterMovement()->StopMovementImmediately();
    };
    auto Enemy = [&](EHellgirlEnemyType Type, const FVector& Position)
    {
        auto* Fighter = GetWorld()->SpawnActor<AArenaFighter>(Position, FRotator::ZeroRotator, Parameters);
        if (!Fighter) return Fighter;
        const bool Flying = Type == EHellgirlEnemyType::FlyingImps || Type == EHellgirlEnemyType::SuccubusBoss;
        Fighter->MakeEnemy(1, Flying);
        Fighter->SetEnemyType(Type);
        if (Type == EHellgirlEnemyType::ImpCommander || Type == EHellgirlEnemyType::GulpBoss || Type == EHellgirlEnemyType::SuccubusBoss)
        {
            Fighter->AttackRange = 240.f;
            Fighter->AttackDamage = 22.f;
        }
        Fighter->GetCharacterMovement()->SetMovementMode(Flying ? MOVE_Flying : MOVE_Walking);
        Fighter->EnemyMoveCooldown = 0.f;
        Fighter->EnemyLastAttackTime = -100.f;
        Fighter->HomePosition = Position - FVector(0.f, 0.f, 88.f);
        Quiet(Fighter);
        return Fighter;
    };
    auto* Player = GetWorld()->SpawnActor<AArenaFighter>(Origin, FRotator::ZeroRotator, Parameters);
    if (!Check(Player != nullptr, TEXT("Unpossessed player fixture spawned"))) return;
    Quiet(Player);
    auto ResetPlayer = [&]()
    {
        Player->ResetAfterRecovery();
        Player->SetActorLocation(Origin, false, nullptr, ETeleportType::TeleportPhysics);
        Player->SetActorRotation(FRotator::ZeroRotator);
        Player->Health = Player->MaxHealth = 1000.f;
        Player->Energy = Player->MaxEnergy;
        Player->Stamina = 100.f;
        Player->Riposte = 100.f;
        Player->DodgeClock = Player->DodgeCooldown = Player->HitClock = Player->KnockdownClock = 0.f;
        Player->PostDodgeClock = Player->ComboClock = Player->BufferClock = 0.f;
        Player->AirCombo = Player->Combo = 0;
        Player->bAirFinisherUsed = Player->bCounterDodge = Player->bBlockHeld = false;
        Player->AirTarget.Reset(); Player->CombatTarget.Reset();
        Player->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
        Player->GetCharacterMovement()->ClearAccumulatedForces();
        Player->GetCharacterMovement()->Velocity = FVector(1800.f, 0.f, 0.f);
    };
    auto CommitArea = [&](FistCombat::Move Move)
    {
        if (Move == FistCombat::Move::ChargedStrike)
        {
            Player->PendingCharge = 1.f;
            Player->StartAttack(true);
        }
        else
        {
            Player->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
            Player->AirCombo = Move == FistCombat::Move::AirCrashKick ? 3 : 0;
            Player->StartAttack(Move == FistCombat::Move::AirSlam);
        }
        return Check(Player->CurrentAttack.Type == Move && Player->AttackClock > 0.f
            && Player->ActiveAttackImpactBudget.IsValid(), TEXT("Accepted area attack creates its impact context before contact"));
    };
    auto Contact = [&]()
    {
        if (FistCombat::IsGroundImpact(Player->CurrentAttack.Type))
            Player->Landed(FHitResult(Floor, Floor->GetStaticMeshComponent(), Origin - FVector(0.f, 0.f, 88.f), FVector::UpVector));
        else
            Player->UpdateAttackTiming(Player->CurrentAttack.Duration * Player->CurrentAttack.ContactFraction + .01f);
    };
    auto Crash = [&](AArenaFighter* Source, AArenaFighter* Victim, bool ReplaceCurrentContext = false)
    {
        // Supply one blocking contact with enough relative speed to reach the
        // ordinary damage cap. Movement sweeps are covered by the physics test.
        Source->GetCharacterMovement()->Velocity = FVector(1600.f, 0.f, 300.f);
        Victim->GetCharacterMovement()->Velocity = FVector::ZeroVector;
        FHitResult Hit(Victim, Victim->GetCapsuleComponent(), Victim->GetActorLocation(), -FVector::ForwardVector);
        Hit.bBlockingHit = true;
        const auto CapturedContext = Source->CombatLaunchImpactBudget;
        Source->MoveBlockedBy(Hit);
        if (!Check(Source->QueuedCombatImpacts.Num() == 1
            && Source->QueuedCombatImpacts[0].Budget == CapturedContext, TEXT("Blocking contact captures the launch's impact context"))) return false;
        if (ReplaceCurrentContext) Source->CombatLaunchImpactBudget = MakeShared<FCombatImpactBudget>();
        Source->UpdateCombatPhysics(.01f);
        if (ReplaceCurrentContext) Source->CombatLaunchImpactBudget = CapturedContext;
        return true;
    };
    auto AdvanceEnemy = [](AArenaFighter* Fighter, float Seconds)
    {
        while (Seconds > UE_KINDA_SMALL_NUMBER)
        {
            const float Slice = FMath::Min(Seconds, 1.f / 120.f);
            Fighter->UpdateEnemyMoveMotion(Slice);
            Fighter->UpdateAttackTiming(Slice);
            Seconds -= Slice;
        }
    };

    Step = 1;
    for (const auto Move : {FistCombat::Move::ChargedStrike, FistCombat::Move::AirSlam, FistCombat::Move::AirCrashKick})
    {
        ResetPlayer();
        const float Radius = Move == FistCombat::Move::ChargedStrike ? FistCombat::Charged(1.f).Range
            : FistCombat::Select(Move == FistCombat::Move::AirSlam, 3, true, false).Range;
        const FVector Positions[] = {Origin + FVector(85.f, 0.f, 0.f), Origin + FVector(0.f, Radius * .55f, 0.f),
            Origin - FVector(Radius * .95f, 0.f, 0.f)};
        TArray<AArenaFighter*> Group;
        for (const FVector& Position : Positions)
        {
            auto* Imp = Enemy(EHellgirlEnemyType::Imps, Position);
            if (!Check(Imp != nullptr && Imp->Health == 67.f, TEXT("Area fixture uses fresh ordinary sixty-seven-health Imps"))) return;
            Group.Add(Imp);
        }
        if (!CommitArea(Move)) return;
        const auto AttackContext = Player->ActiveAttackImpactBudget;
        Contact();
        TArray<float> AfterContact;
        for (auto* Imp : Group)
        {
            if (!Check(Imp->IsAlive() && Imp->Health < 67.f && Imp->IsCombatLaunched()
                && Imp->CombatLaunchImpactBudget == AttackContext, TEXT("Full-riposte area contact damages and launches each Imp with one shared budget"))) return;
            AfterContact.Add(Imp->Health);
        }
        if (!Check(AfterContact[2] > AfterContact[1] && AfterContact[1] > AfterContact[0], TEXT("Area edge damage is lower than centre damage"))) return;
        if (!Crash(Group[0], Group[1], true)) return;
        if (!Check(Group[1]->CombatLaunchImpactBudget == AttackContext, TEXT("Queued contact propagates its captured context even if the source's current context changed"))) return;
        if (!Crash(Group[1], Group[2])) return;
        if (!Check(Group[2]->CombatLaunchImpactBudget == AttackContext, TEXT("Secondary collision launch inherits the original area budget"))) return;
        if (!Crash(Group[2], Group[0])) return;
        for (int32 Index = 0; Index < Group.Num(); ++Index)
            if (!Check(Group[Index]->IsAlive() && FMath::IsNearlyEqual(AfterContact[Index] - Group[Index]->Health, 6.f, .001f),
                TEXT("Centre/edge Imps survive one full-riposte area attack plus a complete collision chain; each loses at most six extra health"))) return;
        const float ExhaustedHealth = Group[1]->Health;
        Group[1]->ResetAfterRecovery();
        Group[2]->ApplyCombatLaunch(FVector(1200.f, 0.f, 300.f), 0, nullptr, AttackContext);
        if (!Crash(Group[2], Group[1])) return;
        if (!Check(FMath::IsNearlyEqual(Group[1]->Health, ExhaustedHealth) && Group[1]->IsCombatLaunched()
            && Group[1]->CombatLaunchImpactBudget == AttackContext, TEXT("Exhausted collision budget preserves knockback without granting more damage"))) return;

        ResetPlayer();
        for (int32 Index = 0; Index < Group.Num(); ++Index)
        {
            Group[Index]->ResetAfterRecovery();
            Group[Index]->Health = Group[Index]->MaxHealth;
            Group[Index]->HitClock = Group[Index]->KnockdownClock = 0.f;
            Group[Index]->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
            Group[Index]->SetActorLocation(Positions[Index], false, nullptr, ETeleportType::TeleportPhysics);
        }
        if (!CommitArea(Move)) return;
        const auto NextContext = Player->ActiveAttackImpactBudget;
        if (!Check(NextContext != AttackContext, TEXT("A newly accepted area attack owns a fresh impact budget"))) return;
        Contact();
        const float BeforeFreshImpact = Group[0]->Health;
        if (!Crash(Group[0], Group[1])) return;
        if (!Check(FMath::IsNearlyEqual(BeforeFreshImpact - Group[0]->Health, 6.f, .001f)
            && Group[1]->CombatLaunchImpactBudget == NextContext, TEXT("The next attack can spend its own six-damage allowance"))) return;
        for (auto* Imp : Group) Imp->Destroy();
    }

    Step = 2;
    ResetPlayer();
    auto* Flyer = Enemy(EHellgirlEnemyType::FlyingImps, Origin + FVector(120.f, 0.f, 140.f));
    auto* Bystander = Enemy(EHellgirlEnemyType::Imps, Origin + FVector(240.f, 0.f, 0.f));
    if (!Check(Flyer && Bystander, TEXT("Prelanding collision fixtures spawned"))) return;
    Player->AirTarget = Flyer;
    if (!CommitArea(FistCombat::Move::AirSlam)) return;
    const auto AirContext = Player->ActiveAttackImpactBudget;
    if (!Check(Player->bGroundImpactPending && Flyer->IsCombatLaunched()
        && Flyer->CombatLaunchImpactBudget == AirContext, TEXT("The downward flyer launch already owns the pending slam's impact budget"))) return;
    if (!Crash(Flyer, Bystander)) return;
    Flyer->SetActorLocation(Origin + FVector(85.f, 0.f, 0.f), false, nullptr, ETeleportType::TeleportPhysics);
    Flyer->HitClock = Bystander->HitClock = 0.f;
    Contact();
    const float AfterLanding = Flyer->Health;
    Flyer->ApplyCombatLaunch(FVector(1200.f, 0.f, 300.f), 0, nullptr, AirContext);
    if (!Crash(Flyer, Bystander)) return;
    if (!Check(Flyer->IsAlive() && Bystander->IsAlive() && FMath::IsNearlyEqual(Flyer->Health, AfterLanding),
        TEXT("Prelanding and postlanding collisions share one allowance instead of resetting it at slam contact"))) return;
    Flyer->Destroy(); Bystander->Destroy();

    Step = 3;
    auto* OrdinarySource = Enemy(EHellgirlEnemyType::Imps, Origin + FVector(1000.f, 0.f, 0.f));
    auto* OrdinaryVictim = Enemy(EHellgirlEnemyType::Imps, Origin + FVector(1300.f, 0.f, 0.f));
    if (!Check(OrdinarySource && OrdinaryVictim, TEXT("Ordinary collision fixtures spawned"))) return;
    OrdinarySource->Health = OrdinaryVictim->Health = 200.f;
    OrdinarySource->ApplyCombatLaunch(FVector(1600.f, 0.f, 300.f));
    if (!Crash(OrdinarySource, OrdinaryVictim)) return;
    if (!Check(OrdinarySource->Health == 165.f && OrdinaryVictim->Health == 165.f
        && !OrdinaryVictim->CombatLaunchImpactBudget.IsValid(), TEXT("A launch without an area budget retains ordinary collision damage"))) return;
    OrdinarySource->Destroy(); OrdinaryVictim->Destroy();

    Step = 4;
    auto BeginBossAttack = [&](AArenaFighter* Boss)
    {
        Boss->SetActorRotation((Player->GetActorLocation() - Boss->GetActorLocation()).GetSafeNormal2D().Rotation());
        if (Boss->EnemyType == EHellgirlEnemyType::ImpCommander) Boss->BeginEnemyMove(EEnemyMove::CommanderCleave, Player);
        else Boss->StartAttack(false);
    };
    for (const auto Type : {EHellgirlEnemyType::ImpCommander, EHellgirlEnemyType::GulpBoss, EHellgirlEnemyType::SuccubusBoss})
    {
        ResetPlayer();
        Player->GetCharacterMovement()->StopMovementImmediately();
        auto* Boss = Enemy(Type, Origin + FVector(180.f, 0.f, Type == EHellgirlEnemyType::SuccubusBoss ? 70.f : 0.f));
        if (!Check(Boss != nullptr, TEXT("Boss armor fixture spawned"))) return;
        Boss->Health = Boss->MaxHealth = 450.f;
        BeginBossAttack(Boss);
        if (!Check(Boss->IsBossAttackArmored() && Boss->AttackClock > 0.f, TEXT("Every boss type gains armor while its attack is active"))) return;
        AdvanceEnemy(Boss, .15f);
        const float ProtectedClock = Boss->AttackClock;
        const auto ProtectedMove = Boss->EnemyMove;
        const FVector ProtectedVelocity(80.f, 40.f, 0.f);
        Boss->GetCharacterMovement()->Velocity = ProtectedVelocity;
        Boss->ReceiveHit(1.f, FVector::ForwardVector, 1500.f, 2.f);
        Boss->ApplyPhysicsDamage(1.f, FVector(1500.f, 0.f, 450.f));
        Boss->ApplyCombatLaunch(FVector(1500.f, 0.f, 450.f));
        if (!Check(Boss->Health == 448.f && Boss->AttackClock == ProtectedClock && Boss->EnemyMove == ProtectedMove
            && Boss->HitClock == 0.f && Boss->KnockdownClock == 0.f && !Boss->IsCombatLaunched()
            && Boss->GetCharacterMovement()->Velocity.Equals(ProtectedVelocity),
            TEXT("Nonlethal direct hits, physics damage and launch attempts preserve boss attack, velocity and hit-stun state"))) return;
        const float ContactTime = Boss->CurrentAttack.Duration * Boss->CurrentAttack.ContactFraction;
        AdvanceEnemy(Boss, ContactTime - .20f);
        if (!Check(!Player->CanCounter(Boss), TEXT("Boss armor prevents a perfect-dodge counter near contact"))) return;
        const float BeforeDodge = Boss->AttackClock;
        Player->Dodge();
        if (!Check(!Player->bCounterDodge && Player->DodgeClock > 0.f && Boss->Health == 448.f
            && Boss->AttackClock == BeforeDodge && !Boss->bHitResolved, TEXT("Dodge against a boss stays an ordinary dodge and cannot interrupt its attack"))) return;
        Player->DodgeClock = 0.f; // Test the continuing contact after invulnerability has ended.
        Player->GetCharacterMovement()->StopMovementImmediately();
        const float TargetHealth = Player->Health;
        AdvanceEnemy(Boss, .1f);
        if (!Check(Player->Health < TargetHealth && Boss->bHitResolved, TEXT("The protected boss still reaches its real damaging contact"))) return;
        AdvanceEnemy(Boss, Boss->CurrentAttack.Duration);
        if (!Check(Boss->AttackClock <= 0.f && !Boss->IsBossAttackArmored(), TEXT("Boss armor ends after the complete attack and recovery"))) return;
        Boss->ResetAfterRecovery();
        Boss->HitClock = Boss->KnockdownClock = Boss->EnemyMoveCooldown = 0.f;
        Boss->EnemyLastAttackTime = -100.f;
        Boss->GetCharacterMovement()->SetMovementMode(Type == EHellgirlEnemyType::SuccubusBoss ? MOVE_Flying : MOVE_Walking);
        BeginBossAttack(Boss);
        if (!Check(Boss->IsBossAttackArmored(), TEXT("Boss can begin another protected attack"))) return;
        Boss->ApplyPhysicsDamage(10000.f, FVector(100.f, 0.f, 30.f));
        if (!Check(!Boss->IsAlive() && Boss->IsDeathRagdollActive() && Boss->AttackClock == 0.f,
            TEXT("Lethal damage still ends an armored boss attack and starts death physics"))) return;
        Boss->Destroy();
    }

    Step = 5;
    ResetPlayer();
    auto* AirBoss = Enemy(EHellgirlEnemyType::SuccubusBoss, Origin + FVector(120.f, 0.f, 140.f));
    if (!Check(AirBoss != nullptr, TEXT("Flying boss slam fixture spawned"))) return;
    AirBoss->Health = AirBoss->MaxHealth = 450.f;
    BeginBossAttack(AirBoss);
    const float AirBossClock = AirBoss->AttackClock;
    Player->AirTarget = AirBoss;
    if (!CommitArea(FistCombat::Move::AirSlam)) return;
    if (!Check(AirBoss->AttackClock == AirBossClock && !AirBoss->bHitResolved && !AirBoss->IsCombatLaunched()
        && AirBoss->KnockdownClock == 0.f && AirBoss->GetCharacterMovement()->IsFlying(),
        TEXT("Starting a slam cannot pull an attacking flying boss down or pre-cancel its hit"))) return;
    Contact();
    if (!Check(AirBoss->Health < 450.f && AirBoss->AttackClock == AirBossClock && AirBoss->IsBossAttackArmored(),
        TEXT("Landing slam damages the armored flying boss without interrupting its attack"))) return;
    AirBoss->Destroy();

    Step = 6;
    ResetPlayer();
    auto* Interruptible = Enemy(EHellgirlEnemyType::Imps, Origin + FVector(160.f, 0.f, 0.f));
    if (!Check(Interruptible != nullptr, TEXT("Ordinary interruption fixture spawned"))) return;
    for (int32 Kind = 0; Kind < 3; ++Kind)
    {
        Interruptible->ResetAfterRecovery();
        Interruptible->HitClock = Interruptible->KnockdownClock = Interruptible->EnemyMoveCooldown = 0.f;
        Interruptible->EnemyLastAttackTime = -100.f;
        Interruptible->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
        Interruptible->BeginEnemyMove(EEnemyMove::ImpClaw, Player);
        if (!Check(Interruptible->AttackClock > 0.f && !Interruptible->IsBossAttackArmored(), TEXT("Ordinary Imps do not acquire boss armor"))) return;
        if (Kind == 0) Interruptible->ReceiveHit(1.f, FVector::ForwardVector, 100.f);
        else if (Kind == 1) Interruptible->ApplyPhysicsDamage(1.f, FVector(100.f, 0.f, 30.f));
        else Interruptible->ApplyCombatLaunch(FVector(1000.f, 0.f, 300.f));
        if (!Check(Interruptible->AttackClock == 0.f && Interruptible->EnemyMove == EEnemyMove::None,
            TEXT("Direct hits, physics hits and launches still interrupt ordinary Imps"))) return;
    }
    Interruptible->Destroy(); Player->Destroy(); Floor->Destroy();
    UE_LOG(LogTemp, Display, TEXT("COMBAT BALANCE CHECK PASSED: three full-riposte area attacks, radial falloff, shared six-damage impact limits, queue capture and chain inheritance, fresh contexts, prelanding flyer budget, ordinary physics unchanged, all boss armor and lethal deaths, ordinary enemy interruptions"));
    FPlatformMisc::RequestExitWithStatus(false, 0);
#endif
}
