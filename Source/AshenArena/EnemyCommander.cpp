#include "ArenaFighter.h"
#include "EnemySpawnPoint.h"
#include "Components/CapsuleComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "DrawDebugHelpers.h"

int32 AArenaFighter::GetSummonedImpCount() const
{
    int32 Count = 0;
    for (const auto& Imp : CommanderImps)
        if (Imp.IsValid() && Imp->IsAlive()) ++Count;
    return Count;
}

void AArenaFighter::UpdateCommanderTactics(float Dt, AArenaFighter* Player)
{
    if (!IsAlive() || !Player || !Player->IsAlive()) return;
    CommanderJumpClock=FMath::Max(0.f,CommanderJumpClock-Dt);
    if (!bCommanderStarted && bBossEncounter) { SpawnCommanderImps(); if (GetSummonedImpCount()==3) bCommanderStarted=true; }
    if (AttackClock > 0.f || KnockdownClock > 0.f || HitClock > 0.f || bCombatLaunched) return;
    const FVector Delta = Player->GetActorLocation() - GetActorLocation();
    const float Distance = Delta.Size2D();
    const FVector Toward = Delta.GetSafeNormal2D();
    SetActorRotation(Toward.Rotation());
    if (CanBeginEnemyMove(Player))
    {
        if (CommanderJumpClock<=0.f && Distance<=1000.f)
        {
            BeginEnemyMove(EEnemyMove::CommanderJumpSlam,Player);
            if (EnemyMove==EEnemyMove::CommanderJumpSlam) CommanderJumpClock=30.f;
            return;
        }
        if (Distance <= 780.f)
        {
            const EEnemyMove Move = Distance > 480.f ? EEnemyMove::CommanderRush
                : CommanderAttackCycle % 2 == 0 ? EEnemyMove::CommanderSlam : EEnemyMove::CommanderRush;
            if (Move != EEnemyMove::CommanderCleave || Distance <= 310.f)
            {
                BeginEnemyMove(Move,Player);
                if (EnemyMove == Move) ++CommanderAttackCycle;
                return;
            }
        }
    }
    // Measured approach leaves room to punish recovery and deal with the adds.
    const FVector Travel = Distance > 285.f ? Toward : FVector::CrossProduct(FVector::UpVector,Toward)*EnemyOrbitSign;
    if (IsEnemyGroundAheadSafe(Travel)) AddMovementInput(Travel,Distance > 500.f ? 1.f : .55f);
}

void AArenaFighter::SpawnCommanderImps()
{
    if (!bEnemy || EnemyType != EHellgirlEnemyType::ImpCommander || !IsAlive() || !bBossEncounter) return;
    AEnemySpawnPoint* Site = EncounterSite.Get();
    // Summons must belong to a live encounter so the exit cannot unlock early.
    if (!Site || Site->bCleared || !Site->bActivated || !Site->bBoss) return;
    CommanderImps.RemoveAll([](const auto& Imp) { return !Imp.IsValid() || !Imp->IsAlive(); });
    const int32 Available = FMath::Clamp(CommanderImpLimit,1,6) - CommanderImps.Num();
    if (Available <= 0) return;
    FCollisionObjectQueryParams FloorTypes;
    FloorTypes.AddObjectTypesToQuery(ECC_WorldStatic);
    FloorTypes.AddObjectTypesToQuery(ECC_WorldDynamic);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(CommanderSummonFloor),false,this);
    const auto* Defaults = GetDefault<AArenaFighter>();
    const float CapsuleHalf = Defaults->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
    const float CapsuleRadius = Defaults->GetCapsuleComponent()->GetUnscaledCapsuleRadius();
    int32 Created = 0;
    // Search a bounded set of positions around the existing boss rift. Never
    // spawn inside a wall, pawn, steep slope or gap, and never count a failure.
    for (int32 Attempt = 0; Attempt < 18 && Created < Available; ++Attempt)
    {
        const float Angle = FMath::DegreesToRadians(120.f*(Attempt%3) + 25.f*(Attempt/3));
        const float Radius = 620.f + 75.f*(Attempt/6);
        const FVector P = GetActorLocation() + FVector(FMath::Cos(Angle)*Radius,FMath::Sin(Angle)*Radius,0);
        FHitResult Floor;
        if (!GetWorld()->LineTraceSingleByObjectType(Floor,P+FVector(0,0,1000),P-FVector(0,0,800),FloorTypes,Query)
            || Floor.ImpactNormal.Z < Defaults->GetCharacterMovement()->GetWalkableFloorZ()) continue;
        const FVector Location(Floor.ImpactPoint.X,Floor.ImpactPoint.Y,Floor.ImpactPoint.Z+CapsuleHalf+12.f);
        if (GetWorld()->OverlapBlockingTestByChannel(Location,FQuat::Identity,ECC_Pawn,
            FCollisionShape::MakeCapsule(CapsuleRadius,CapsuleHalf),Query)) continue;
        FActorSpawnParameters Params;
        Params.Owner = this;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding;
        auto* Imp = GetWorld()->SpawnActor<AArenaFighter>(Location,FRotator::ZeroRotator,Params);
        if (!Imp) continue;
        Imp->MakeEnemy(1,false);
        Imp->SetEnemyType(EHellgirlEnemyType::Imps);
        Imp->HomePosition = HomePosition;
        Imp->bGuardHome = false;
        // New arrivals visibly emerge before joining the coordinated attacks.
        Imp->EnemyMoveCooldown = 1.f + Created*.35f;
        Imp->EnemyOrbitSign = Created%2 ? -1.f : 1.f;
        Imp->Tags.Add(TEXT("CommanderReinforcement"));
        if (!Site->RegisterReinforcement(Imp)) { Imp->Destroy(); continue; }
        CommanderImps.Add(Imp);
        ++Created;
        DrawDebugSphere(GetWorld(),Location,110.f,16,FColor(255,105,35),false,.65f,0,5.f);
    }
    if (Created == 0) CommanderSummonClock = 4.f;
    UE_LOG(LogTemp,Display,TEXT("IMP COMMANDER: summoned %d Imps, %d alive"),Created,GetSummonedImpCount());
}

void AArenaFighter::DrawEnemyMoveTelegraph()
{
    if (EnemyMove == EEnemyMove::None || AttackClock <= 0.f || !IsAlive()) return;
    const bool WindingUp = !bHitResolved;
    const FColor Cue = WindingUp ? FColor(255,160,55) : FColor(150,170,180);
    if (EnemyMove == EEnemyMove::CommanderSummon && WindingUp)
    {
        for (int32 I=0; I<3; ++I)
        {
            const float Angle = 2.f*PI*I/3.f;
            const FVector P = HomePosition+FVector(FMath::Cos(Angle)*620.f,FMath::Sin(Angle)*620.f,12.f);
            DrawDebugCircle(GetWorld(),P,110.f,28,FColor(255,100,35),false,-1.f,0,5.f,FVector::ForwardVector,FVector::RightVector,false);
            DrawDebugLine(GetWorld(),P,P+FVector(0,0,150),Cue,false,-1.f,0,3.f);
        }
    }
    else if (EnemyMove == EEnemyMove::CommanderJumpSlam)
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
