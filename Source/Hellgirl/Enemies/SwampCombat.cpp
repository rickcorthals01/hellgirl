// World II's enemies (Developer idea folder: "Levels , Enemies, Bosses.txt").
//   The rat: a quick bite that cannot be dodged away by a perfect dodge (low damage, once every few seconds), a very
//   fast punch (medium damage) that it may follow with a second, and a dodge roll away from Hellgirl's attacks.
//   The frog: always hopping, fast and high. A quick punch on the ground, a punch in the air when level with her, and a
//   slam straight down from high up: medium damage in a circle and a small blast that pushes her back; other enemies
//   in the splash take a little damage and are pushed away too. The Frog King (mini-boss) does the same, bigger.
// Numbers are in Rules/EnemyTuning.h; the moves themselves in Enemies/EnemyMoveset.cpp.
#include "Fighter/ArenaFighter.h"
#include "Enemies/EnemyMovesetState.h"
#include "Levels/ArenaGameMode.h"
#include "Rules/EnemyTuning.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "EngineUtils.h"

void AArenaFighter::StartRatRoll(const FVector& Direction)
{
    if (!IsAlive() || AttackClock > 0.f || EnemyMove != EEnemyMove::None) return;
    // Roll the way asked, or the other way if that is blocked.
    FVector Dir = Direction.GetSafeNormal2D();
    if (!IsEnemyGroundAheadSafe(Dir, EnemyTuning::RatRollDistance + 35.f))
    {
        Dir = FVector(-Dir.X, -Dir.Y, 0.f);
        if (!IsEnemyGroundAheadSafe(Dir, EnemyTuning::RatRollDistance + 35.f)) return;
    }
    EnemyMove = EEnemyMove::RatRoll;
    EnemyMoveStart = GetActorLocation();
    EnemyMoveTarget = GetActorLocation() + Dir * EnemyTuning::RatRollDistance;
    EnemyFacing = Dir;
    bEnemyMoveMotionStopped = false;
    EnemyMoveMotionProgress = 0.f;
    // A roll is a move with no hit: its clock drives the clip and the motion; it cannot be hurt while rolling.
    CurrentAttack = {FistCombat::Move::EnemyClaw, EnemyTuning::RatRollSeconds, .99f, 0.f, 0.f, 0.f, 0.f, 0};
    AttackClock = EnemyTuning::RatRollSeconds;
    bHitResolved = bSecondHitResolved = true;
    DodgeClock = .4f;
    EnemyMoveCooldown = EnemyTuning::RatRollSeconds + .2f;
    MoveLabel = TEXT("RAT / ROLL");
    MoveLabelClock = EnemyTuning::RatRollSeconds;
    SetActorRotation(Dir.Rotation());
    GetCharacterMovement()->StopMovementImmediately();
}

void AArenaFighter::UpdateRatTactics(float Dt, AArenaFighter* Player)
{
    if (!Player || EnemyMove == EEnemyMove::RatRoll) return;
    const FVector Delta = Player->GetActorLocation() - GetActorLocation();
    const float Distance = Delta.Size2D();
    const FVector Toward = Delta.GetSafeNormal2D();
    // Hellgirl winding up an attack at it: sometimes it rolls aside (and a little away), giving up its own attack if
    // that has not landed yet.
    if (RatRollClock <= 0.f && Distance < 300.f && Player->IsWindingUpAttack() && (AttackClock <= 0.f || IsWindingUpAttack())
        && FVector::DotProduct(Player->GetActorForwardVector(), -Toward) > .5f)
    {
        const bool Rolls = FMath::FRand() < EnemyTuning::RatRollChance;
        RatRollClock = Rolls ? EnemyTuning::RatRollCooldown : .6f;
        if (Rolls)
        {
            CancelEnemyMove();
            bRatPunchChain = false;
            const FVector Side = FVector(-Toward.Y, Toward.X, 0.f) * (FMath::RandBool() ? 1.f : -1.f);
            StartRatRoll(Side - Toward * .45f);
            if (EnemyMove == EEnemyMove::RatRoll) return;
        }
    }
    if (AttackClock > 0.f) return;
    SetActorRotation(Toward.Rotation());
    // The second punch follows straight on from the first (once the first has fully ended).
    if (bRatPunchChain && EnemyMove == EEnemyMove::None)
    {
        bRatPunchChain = false;
        EnemyMoveCooldown = 0.f;
        BeginEnemyMove(EEnemyMove::RatPunch2, Player);
        if (EnemyMove == EEnemyMove::RatPunch2) return;
    }
    if (CanBeginEnemyMove(Player))
    {
        if (RatBiteClock <= 0.f && Distance < EnemyTuning::RatBiteReach)
        {
            BeginEnemyMove(EEnemyMove::RatBite, Player);
            if (EnemyMove == EEnemyMove::RatBite) { RatBiteClock = EnemyTuning::RatBiteCooldown; return; }
        }
        if (Distance < EnemyTuning::RatPunchReach)
        {
            BeginEnemyMove(EEnemyMove::RatPunch, Player);
            if (EnemyMove == EEnemyMove::RatPunch) { bRatPunchChain = FMath::FRand() < EnemyTuning::RatSecondPunchChance; return; }
        }
    }
    if (Distance > 140.f && IsEnemyGroundAheadSafe(Toward)) AddMovementInput(Toward, 1.f);
}

void AArenaFighter::UpdateFrogTactics(float Dt, AArenaFighter* Player)
{
    auto* Movement = GetCharacterMovement();
    const bool Ground = Movement->IsMovingOnGround();
    EnemyAirTime = Ground ? 0.f : EnemyAirTime + Dt;
    if (!Player || AttackClock > 0.f) return;
    const bool King = EnemyType == EHellgirlEnemyType::FrogKing;
    const FVector Delta = Player->GetActorLocation() - GetActorLocation();
    const float Distance = Delta.Size2D();
    const FVector Toward = Delta.GetSafeNormal2D().IsNearlyZero() ? GetActorForwardVector() : Delta.GetSafeNormal2D();
    if (!Ground)
    {
        // In the air: from high up, slam down onto her; level with her, punch.
        FHitResult Floor;
        FCollisionQueryParams Query(SCENE_QUERY_STAT(FrogHeight), false, this);
        const float Above = GetWorld()->LineTraceSingleByChannel(Floor, GetActorLocation(), GetActorLocation() - FVector(0, 0, 2000.f), ECC_Visibility, Query)
            ? static_cast<float>(GetActorLocation().Z - Floor.ImpactPoint.Z) - GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 0.f;
        if (FrogSlamClock <= 0.f && Movement->Velocity.Z < 250.f && Above > 130.f && Distance < (King ? 750.f : 480.f) && CanBeginEnemyMove(Player))
        {
            BeginEnemyMove(EEnemyMove::FrogSlam, Player);
            if (EnemyMove == EEnemyMove::FrogSlam) { FrogSlamClock = King ? EnemyTuning::FrogKingSlamCooldown : EnemyTuning::FrogSlamCooldown; return; }
        }
        if (FMath::Abs(Delta.Z) < 110.f && Distance < 200.f && CanBeginEnemyMove(Player)) BeginEnemyMove(EEnemyMove::FrogAirPunch, Player);
        return;
    }
    SetActorRotation(Toward.Rotation());
    if (Distance < EnemyTuning::FrogPunchReach && CanBeginEnemyMove(Player))
    {
        BeginEnemyMove(EEnemyMove::FrogPunch, Player);
        if (EnemyMove == EEnemyMove::FrogPunch) return;
    }
    FrogHopClock -= Dt;
    if (FrogHopClock > 0.f) return;
    // Hop: straight at her from afar; around her when close; up and over her to set up a slam when it is ready.
    const FVector Side(-Toward.Y * EnemyOrbitSign, Toward.X * EnemyOrbitSign, 0.f);
    FVector Dir = Distance > 750.f ? Toward : (Toward * (Distance > 320.f ? .6f : -.15f) + Side * .8f).GetSafeNormal2D();
    float Forward = EnemyTuning::FrogHopForward * FMath::FRandRange(.7f, 1.1f);
    if (FrogSlamClock <= 0.f && Distance < 650.f && Distance > 150.f)
    {
        Dir = Toward;
        Forward = Distance * .85f; // lands near her, slamming on the way down
    }
    if (!IsEnemyGroundAheadSafe(Dir, Forward * .9f))
    {
        Dir = Toward;
        EnemyOrbitSign *= -1.f;
    }
    const float Up = EnemyTuning::FrogHopUp * (King ? 1.2f : 1.f) * FMath::FRandRange(.85f, 1.1f);
    LaunchCharacter(Dir * Forward + FVector(0.f, 0.f, Up), true, true);
    SetActorRotation(Dir.Rotation());
    FrogHopClock = FMath::FRandRange(EnemyTuning::FrogHopMin, EnemyTuning::FrogHopMax) * (King ? .8f : 1.f);
    if (FMath::FRand() < .25f) EnemyOrbitSign *= -1.f;
}

void AArenaFighter::FrogSlamSplash()
{
    const float Radius = CurrentAttack.Range;
    for (TActorIterator<AArenaFighter> It(GetWorld()); It; ++It)
    {
        AArenaFighter* Other = *It;
        if (Other == this || !Other->bEnemy || !Other->IsAlive()) continue;
        const FVector Delta = Other->GetActorLocation() - GetActorLocation();
        if (Delta.Size2D() > Radius || FMath::Abs(Delta.Z) > 200.f) continue;
        const FVector Away = Delta.GetSafeNormal2D().IsNearlyZero() ? FVector(1.f, 0.f, 0.f) : Delta.GetSafeNormal2D();
        Other->ReceiveHit(EnemyTuning::FrogSlamEnemyDamage, Away, EnemyTuning::FrogSlamEnemyPush, 0.f);
        ++Other->SplashHitsTaken;
    }
    // A splash on the water (or the mud) where it landed.
    if (auto* Mode = Cast<AArenaGameMode>(UGameplayStatics::GetGameMode(this)); Mode && Mode->bSwamp)
        Mode->SwampSplash(GetActorLocation(), EnemyType == EHellgirlEnemyType::FrogKing ? 3.2f : 2.4f, 14);
}
