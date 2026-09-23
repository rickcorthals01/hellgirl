#include "Fighter/ArenaFighter.h"
#include "Enemies/EnemyMovesetState.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "DrawDebugHelpers.h"

namespace
{
bool UsesCastleMoves(const AArenaFighter* Fighter)
{
    return Fighter && (Fighter->EnemyType == EHellgirlEnemyType::Imps
        || Fighter->EnemyType == EHellgirlEnemyType::FlyingImps
        || Fighter->EnemyType == EHellgirlEnemyType::ImpCommander
        || Fighter->EnemyType == EHellgirlEnemyType::Goblins || Fighter->EnemyType == EHellgirlEnemyType::GoblinQueen);
}

bool FindEnemyFloor(const AArenaFighter* Fighter, const FVector& Position, FHitResult& Floor)
{
    if (!Fighter || !Fighter->GetWorld()) return false;
    FCollisionObjectQueryParams Types;
    Types.AddObjectTypesToQuery(ECC_WorldStatic);
    Types.AddObjectTypesToQuery(ECC_WorldDynamic);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(EnemyMoveFloor), false, Fighter);
    return Fighter->GetWorld()->LineTraceSingleByObjectType(Floor,
        Position + FVector(0.f, 0.f, 400.f), Position - FVector(0.f, 0.f, 1600.f), Types, Query)
        && Floor.ImpactNormal.Z >= Fighter->GetCharacterMovement()->GetWalkableFloorZ()
        && Floor.ImpactPoint.Z >= -45.f;
}

float SmoothMotion(float Value)
{
    const float T = FMath::Clamp(Value, 0.f, 1.f);
    return T * T * (3.f - 2.f * T);
}
}

bool AArenaFighter::IsEnemyGroundAheadSafe(const FVector& Direction, float Distance) const
{
    FHitResult CurrentFloor;
    if (!FindEnemyFloor(this, GetActorLocation(), CurrentFloor)) return false;
    const FVector Forward = Direction.GetSafeNormal2D();
    const FVector Side(-Forward.Y, Forward.X, 0.f);
    const float Travel = FMath::Max(0.f, Distance);
    const int32 Samples = FMath::Max(1, FMath::CeilToInt(Travel / 110.f));
    const float Step = Travel / Samples;
    const float Radius = GetCapsuleComponent()->GetScaledCapsuleRadius() * .8f;
    float PreviousZ = CurrentFloor.ImpactPoint.Z;
    for (int32 Index = 1; Index <= Samples; ++Index)
    {
        const FVector Point = GetActorLocation() + Forward * (Step * Index);
        FHitResult Floor;
        if (!FindEnemyFloor(this, Point, Floor)
            || FMath::Abs(Floor.ImpactPoint.Z - PreviousZ) > GetCharacterMovement()->MaxStepHeight + Step * .75f)
            return false;
        for (const float Sign : {-1.f, 1.f})
        {
            FHitResult Edge;
            if (!FindEnemyFloor(this, Point + Side * Radius * Sign, Edge)
                || FMath::Abs(Edge.ImpactPoint.Z - Floor.ImpactPoint.Z) > GetCharacterMovement()->MaxStepHeight + Radius)
                return false;
        }
        PreviousZ = Floor.ImpactPoint.Z;
    }
    return true;
}

bool AArenaFighter::CanBeginEnemyMove(const AArenaFighter* Player) const
{
    if (!bEnemy || !UsesCastleMoves(this) || !IsAlive() || !Player || !Player->IsAlive()
        || bCombatLaunched || KnockdownClock > 0.f || HitClock > 0.f || DodgeClock > 0.f
        || AttackClock > 0.f || EnemyMove != EEnemyMove::None || EnemyMoveCooldown > 0.f)
        return false;
    if (!bFlyingEnemy && !GetCharacterMovement()->IsMovingOnGround()) return false;
    if (FVector::DistSquared2D(GetActorLocation(), Player->GetActorLocation()) > FMath::Square(1800.f)) return false;
    FHitResult Obstacle;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(EnemyMoveSight), false, this);
    Query.AddIgnoredActor(Player);
    if (GetWorld()->LineTraceSingleByChannel(Obstacle, GetActorLocation(), Player->GetActorLocation(), ECC_Visibility, Query))
        return false;

    const float Now = GetWorld()->GetTimeSeconds();
    int32 Active = 0;
    for (TActorIterator<AArenaFighter> It(GetWorld()); It; ++It)
    {
        const AArenaFighter* Other = *It;
        if (Other == this || !Other->bEnemy || !UsesCastleMoves(Other)
            || FVector::DistSquared2D(Other->GetActorLocation(), Player->GetActorLocation()) > FMath::Square(1800.f))
            continue;
        // Per-actor timestamps live in this world, including the short time a
        // just-defeated attacker remains present. No static scheduler survives travel.
        if (Now - Other->EnemyLastAttackTime < .45f) return false;
        if (Other->IsAlive() && !Other->bCombatLaunched && Other->EnemyMove != EEnemyMove::None
            && Other->AttackClock > 0.f && Other->KnockdownClock <= 0.f && Other->HitClock <= 0.f)
            ++Active;
    }
    return Active < 2;
}

void AArenaFighter::BeginEnemyMove(EEnemyMove Move, AArenaFighter* Player)
{
    if (Move == EEnemyMove::None || !CanBeginEnemyMove(Player)) return;
    FistCombat::AttackSpec Spec{FistCombat::Move::EnemyClaw, 1.2f, .6f, AttackDamage, 185.f, 180.f, 0.f, 0};
    float Recovery = .65f;
    const TCHAR* Label = TEXT("IMP / CLAW");
    switch (Move)
    {
    case EEnemyMove::GoblinSlash:
        Spec = {FistCombat::Move::EnemyClaw,1.65f,.65f,10.f,185.f,100.f,0.f,0}; Recovery=.9f; Label=TEXT("GOBLIN / DAGGER SLASH"); break;
    case EEnemyMove::QueenMelee:
        Spec = {FistCombat::Move::EnemyClaw,1.6f,.6f,20.f,250.f,200.f,0.f,0}; Recovery=.7f; Label=TEXT("QUEEN / SHADOW ATTACK"); break;
    case EEnemyMove::QueenClaw:
        Spec = {FistCombat::Move::EnemyClaw,1.8f,.6f,20.f,2400.f,0.f,0.f,0}; Recovery=1.2f; Label=TEXT("QUEEN / SHADOW CLAW"); break;
    case EEnemyMove::CommanderJumpSlam:
        Spec = {FistCombat::Move::EnemyClaw,3.f,.72f,28.f,380.f,350.f,0.f,0}; Recovery=1.6f; Label=TEXT("COMMANDER / JUMP SLAM"); break;
    case EEnemyMove::ImpPounce:
        Spec = {FistCombat::Move::EnemyClaw, 1.55f, .6f, AttackDamage * 1.2f, 200.f, 260.f, 0.f, 0};
        Recovery = 1.15f; Label = TEXT("IMP / POUNCE"); break;
    case EEnemyMove::FlyingDive:
        Spec = {FistCombat::Move::EnemyClaw, 2.2f, .6f, AttackDamage * 1.3f, 220.f, 280.f, 0.f, 0};
        Recovery = 1.6f; Label = TEXT("FLYING IMP / DIVE"); break;
    case EEnemyMove::CommanderCleave:
        Spec = {FistCombat::Move::EnemyClaw, 2.f, .6f, 22.f, 330.f, 300.f, 0.f, 0};
        Recovery = .85f; Label = TEXT("COMMANDER / CLEAVE"); break;
    case EEnemyMove::CommanderRush:
        Spec = {FistCombat::Move::EnemyClaw, 2.6f, .6f, 26.f, 270.f, 420.f, 0.f, 0};
        Recovery = 1.4f; Label = TEXT("COMMANDER / RUSH"); break;
    case EEnemyMove::CommanderSlam:
        Spec = {FistCombat::Move::EnemyClaw, 2.8f, .6f, 20.f, 500.f, 350.f, 0.f, 0};
        Recovery = 1.8f; Label = TEXT("COMMANDER / GROUND SLAM"); break;
    default: break;
    }

    const FVector Start = GetActorLocation();
    const FVector Delta = Player->GetActorLocation() - Start;
    FVector Facing = Delta.GetSafeNormal2D();
    if (Facing.IsNearlyZero()) Facing = GetActorForwardVector();
    FVector Endpoint = Start;
    if (Move == EEnemyMove::ImpPounce || Move == EEnemyMove::CommanderRush || Move == EEnemyMove::FlyingDive || Move == EEnemyMove::CommanderJumpSlam)
    {
        const float StopDistance = Move == EEnemyMove::CommanderJumpSlam ? 0.f : Move == EEnemyMove::CommanderRush ? 175.f : (Move == EEnemyMove::FlyingDive ? 130.f : 125.f);
        const float MaximumTravel = Move == EEnemyMove::CommanderJumpSlam ? 1000.f : Move == EEnemyMove::CommanderRush ? 650.f : (Move == EEnemyMove::FlyingDive ? 550.f : 360.f);
        const float Travel = FMath::Clamp(static_cast<float>(Delta.Size2D()) - StopDistance, 0.f, MaximumTravel);
        if (!IsEnemyGroundAheadSafe(Facing, Travel + 35.f)) return;
        Endpoint += Facing * Travel;
        FHitResult Floor;
        if (!FindEnemyFloor(this, Endpoint, Floor)) return;
        Endpoint.Z = Floor.ImpactPoint.Z + GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 6.f;
    }
    else if (!IsEnemyGroundAheadSafe(Facing, 0.f)) return;

    EnemyMove = Move;
    EnemyMoveStart = Start;
    EnemyMoveTarget = Endpoint;
    EnemyFacing = Facing;
    bEnemyMoveMotionStopped = false;
    EnemyMoveMotionProgress = 0.f;
    ++EnemyAttackSerial;
    EnemyLastAttackTime = GetWorld()->GetTimeSeconds();
    EnemyMoveCooldown = Spec.Duration + Recovery;
    EnemyDecisionClock = .2f;
    CurrentAttack = Spec;
    AttackClock = Spec.Duration;
    bHitResolved = false;
    BufferClock = ComboClock = 0.f;
    MoveLabel = Label;
    MoveLabelClock = Spec.Duration + .4f;
    SetActorRotation(Facing.Rotation());
    ConsumeMovementInputVector();
    GetCharacterMovement()->StopMovementImmediately();
    if (Move == EEnemyMove::FlyingDive) GetCharacterMovement()->SetMovementMode(MOVE_Flying);
}

void AArenaFighter::CancelEnemyMove()
{
    if (EnemyMove == EEnemyMove::None) return;
    EnemyMove = EEnemyMove::None;
    EnemyMoveMotionProgress = 0.f;
    bEnemyMoveMotionStopped = true;
    EnemyMoveCooldown = FMath::Max(EnemyMoveCooldown, .65f);
    EnemyDecisionClock = FMath::Max(EnemyDecisionClock, .2f);
    AttackClock = BufferClock = 0.f;
    bHitResolved = true;
    ConsumeMovementInputVector();
    // Interruption callers run this before installing their impulse. The
    // guards also protect a launch which has already taken over movement.
    if (!bCombatLaunched && KnockdownClock <= 0.f && HitClock <= 0.f)
        GetCharacterMovement()->StopMovementImmediately();
}

void AArenaFighter::UpdateEnemyMoveMotion(float Dt)
{
    if (!bEnemy || !UsesCastleMoves(this)) return;
    EnemyMoveCooldown = FMath::Max(0.f, EnemyMoveCooldown - Dt);
    EnemyDecisionClock = FMath::Max(0.f, EnemyDecisionClock - Dt);
    if (EnemyMove == EEnemyMove::None) return;
    if (!IsAlive() || bCombatLaunched || KnockdownClock > 0.f || HitClock > 0.f || AttackClock <= 0.f)
    {
        CancelEnemyMove();
        return;
    }
    SetActorRotation(EnemyFacing.Rotation());
    ConsumeMovementInputVector();
    GetCharacterMovement()->StopMovementImmediately();
    if (bEnemyMoveMotionStopped) return;
    if (EnemyMove == EEnemyMove::CommanderJumpSlam)
    {
        const float Progress=FMath::Clamp((CurrentAttack.Duration-AttackClock+Dt)/CurrentAttack.Duration,0.f,1.f);
        const float T=FMath::Clamp(Progress/CurrentAttack.ContactFraction,0.f,1.f);
        const FVector Desired=FMath::Lerp(EnemyMoveStart,EnemyMoveTarget,T)+FVector(0,0,FMath::Sin(T*PI)*650.f);
        FHitResult Hit; SetActorLocation(Desired,true,&Hit);
        GetCharacterMovement()->SetMovementMode(T<1.f ? MOVE_Flying : MOVE_Falling);
        return;
    }
    float StartFraction = 0.f, EndFraction = 0.f;
    switch (EnemyMove)
    {
    case EEnemyMove::ImpPounce: StartFraction = .35f; EndFraction = .55f; break;
    case EEnemyMove::FlyingDive: StartFraction = .34f; EndFraction = .56f; break;
    case EEnemyMove::CommanderRush: StartFraction = .34f; EndFraction = .57f; break;
    default: return;
    }
    const float NextProgress = FMath::Clamp((CurrentAttack.Duration - AttackClock + Dt) / CurrentAttack.Duration, 0.f, 1.f);
    const float Motion = SmoothMotion((NextProgress - StartFraction) / (EndFraction - StartFraction));
    if (Motion <= EnemyMoveMotionProgress) return;
    FVector Step = (EnemyMoveTarget - EnemyMoveStart) * (Motion - EnemyMoveMotionProgress);
    const FVector Next = GetActorLocation() + Step;
    FHitResult Floor;
    if (!IsEnemyGroundAheadSafe(Step, Step.Size2D() + 20.f) || !FindEnemyFloor(this, Next, Floor))
    {
        bEnemyMoveMotionStopped = true;
        return;
    }
    const float FloorZ = Floor.ImpactPoint.Z + GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 3.f;
    if (EnemyMove == EEnemyMove::FlyingDive) Step.Z = FMath::Max(Next.Z, FloorZ) - GetActorLocation().Z;
    else Step.Z = FloorZ - GetActorLocation().Z;
    FHitResult Block;
    AddActorWorldOffset(Step, true, &Block);
    EnemyMoveMotionProgress = Motion;
    if (Block.bBlockingHit) bEnemyMoveMotionStopped = true;
    // Recovery deliberately holds the diver near the ground. Hover resumes
    // only after its attack clock ends, giving the player a punish window.
}

void AArenaFighter::DrawEnemyMoveTelegraph()
{
    if (EnemyMove == EEnemyMove::None || AttackClock <= 0.f || !IsAlive()) return;
    const bool WindingUp = !bHitResolved;
    const FColor Cue = WindingUp ? FColor(255,160,55) : FColor(150,170,180);
    if (EnemyMove == EEnemyMove::CommanderJumpSlam)
    {
        const float UntilHit=AttackClock-CurrentAttack.Duration*(1.f-CurrentAttack.ContactFraction);
        if (UntilHit>0.f && UntilHit<=(bBossEncounter ? .45f : 1.2f))
            DrawDebugCircle(GetWorld(),EnemyMoveTarget-FVector(0,0,GetCapsuleComponent()->GetScaledCapsuleHalfHeight()-8),CurrentAttack.Range,48,FColor::Red,false,-1.f,0,6.f,FVector::ForwardVector,FVector::RightVector,false);
    }
    else if (EnemyMove == EEnemyMove::CommanderSlam)
    {
        const FVector Floor = GetActorLocation()-FVector(0,0,GetCapsuleComponent()->GetScaledCapsuleHalfHeight()-10.f);
        DrawDebugCircle(GetWorld(),Floor,CurrentAttack.Range,48,Cue,false,-1.f,0,5.f,FVector::ForwardVector,FVector::RightVector,false);
    }
    else
        DrawDebugDirectionalArrow(GetWorld(),GetActorLocation(),GetActorLocation()+GetActorForwardVector()*CurrentAttack.Range,35.f,Cue,false,-1.f,0,4.f);
    DrawDebugString(GetWorld(),GetActorLocation()+FVector(0,0,GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+90.f),
        WindingUp ? MoveLabel : TEXT("RECOVERING"),nullptr,Cue,0.f,true,1.f);
}

bool AArenaFighter::UpdateImpTactics(float Dt, AArenaFighter* Player)
{
    if (EnemyType != EHellgirlEnemyType::Imps && EnemyType != EHellgirlEnemyType::FlyingImps) return false;
    if (!IsAlive() || !Player || !Player->IsAlive() || bCombatLaunched || KnockdownClock > 0.f || HitClock > 0.f)
        return true;
    if (EnemyMove != EEnemyMove::None || AttackClock > 0.f) return true;
    GetCharacterMovement()->bOrientRotationToMovement = false;
    const FVector Delta = Player->GetActorLocation() - GetActorLocation();
    const float Distance = Delta.Size2D();
    const FVector Toward = Delta.GetSafeNormal2D().IsNearlyZero() ? GetActorForwardVector() : Delta.GetSafeNormal2D();
    if (EnemyAttackSerial == 0) EnemyOrbitSign = GetUniqueID() % 2 ? 1.f : -1.f;
    const bool DecisionReady = EnemyDecisionClock <= 0.f;
    if (DecisionReady)
    {
        EnemyDecisionClock = .18f + .03f * (GetUniqueID() % 5);
        if (CanBeginEnemyMove(Player))
        {
            if (EnemyType == EHellgirlEnemyType::FlyingImps)
            {
                FHitResult Floor;
                if (Distance <= 680.f && FindEnemyFloor(this, GetActorLocation(), Floor)
                    && GetActorLocation().Z - Floor.ImpactPoint.Z >= 230.f && FMath::Abs(Delta.Z) < 480.f)
                    BeginEnemyMove(EEnemyMove::FlyingDive, Player);
            }
            else if (FMath::Abs(Delta.Z) < 140.f)
            {
                if (Distance <= 185.f) BeginEnemyMove(EEnemyMove::ImpClaw, Player);
                else if (Distance >= 255.f && Distance <= 550.f && EnemyAttackSerial % 3 != 1)
                    BeginEnemyMove(EEnemyMove::ImpPounce, Player);
            }
        }
        if (EnemyMove != EEnemyMove::None) return true;
    }

    // Available attackers close in; those waiting for a slot circulate at a
    // little distance. Keeping a persistent orbit sign avoids left/right jitter.
    const bool Flying = EnemyType == EHellgirlEnemyType::FlyingImps;
    const bool PreferClaw = !Flying && EnemyMoveCooldown <= 0.f && EnemyAttackSerial % 3 == 1;
    const float OrbitRadius = Flying ? 440.f : (PreferClaw ? 135.f : 275.f);
    const FVector Tangent(-Toward.Y, Toward.X, 0.f);
    const float Radial = FMath::Clamp((Distance - OrbitRadius) / 220.f, -.7f, 1.f);
    const float Circling = Distance < 780.f ? (PreferClaw ? .18f : .65f) : .05f;
    FVector Direction = Toward * Radial + Tangent * EnemyOrbitSign * Circling;
    for (TActorIterator<AArenaFighter> It(GetWorld()); It; ++It)
    {
        if (*It == this || !It->bEnemy || !It->IsAlive()) continue;
        const FVector Separation = GetActorLocation() - It->GetActorLocation();
        const float Gap = Separation.Size2D();
        if (Gap > 1.f && Gap < 180.f && FMath::Abs(Separation.Z) < 170.f)
            Direction += Separation.GetSafeNormal2D() * ((180.f - Gap) / 180.f);
    }
    Direction = Direction.GetSafeNormal2D();
    if (!IsEnemyGroundAheadSafe(Direction, 130.f))
    {
        const FVector Alternative = (Toward * .2f - Tangent * EnemyOrbitSign).GetSafeNormal2D();
        if (IsEnemyGroundAheadSafe(Alternative, 130.f))
        {
            Direction = Alternative;
            if (DecisionReady) EnemyOrbitSign *= -1.f;
        }
        else Direction = FVector::ZeroVector;
    }
    SetActorRotation(FMath::RInterpTo(GetActorRotation(), Toward.Rotation(), Dt, 6.f));
    if (Flying)
    {
        FHitResult Floor;
        if (!FindEnemyFloor(this, GetActorLocation(), Floor))
        {
            GetCharacterMovement()->StopMovementImmediately();
            return true;
        }
        const float HoverZ = Floor.ImpactPoint.Z + 310.f;
        Direction.Z = FMath::Clamp((HoverZ - GetActorLocation().Z) / 150.f, -1.f, 1.f);
        GetCharacterMovement()->SetMovementMode(MOVE_Flying);
    }
    AddMovementInput(Direction.GetSafeNormal(), Flying ? .9f : .85f);
    return true;
}
