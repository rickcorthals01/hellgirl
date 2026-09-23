#include "Bosses/ImpCommanderBehavior.h"
#include "Fighter/ArenaFighter.h"
#include "Enemies/EnemySpawnPoint.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "DrawDebugHelpers.h"

int32 UImpCommanderBehavior::GetSummonedImpCount() const
{
    int32 Count = 0;
    for (const auto& Imp : Imps)
        if (Imp.IsValid() && Imp->IsAlive()) ++Count;
    return Count;
}

float UImpCommanderBehavior::FilterDamage(float Damage) const
{
    return Fighter()->bBossEncounter && GetSummonedImpCount()>0 ? Damage*.1f : Damage;
}

void UImpCommanderBehavior::TickTactics(float Dt, AArenaFighter* Player)
{
    AArenaFighter* Commander = Fighter();
    if (!Commander->IsAlive() || !Player || !Player->IsAlive()) return;
    JumpClock=FMath::Max(0.f,JumpClock-Dt);
    if (!bStarted && Commander->bBossEncounter) { SpawnImps(); if (GetSummonedImpCount()==3) bStarted=true; }
    if (Commander->AttackClock > 0.f || Commander->KnockdownClock > 0.f || Commander->HitClock > 0.f || Commander->bCombatLaunched) return;
    const FVector Delta = Player->GetActorLocation() - Commander->GetActorLocation();
    const float Distance = Delta.Size2D();
    const FVector Toward = Delta.GetSafeNormal2D();
    Commander->SetActorRotation(Toward.Rotation());
    if (Commander->CanBeginEnemyMove(Player))
    {
        if (JumpClock<=0.f && Distance<=1000.f)
        {
            Commander->BeginEnemyMove(EEnemyMove::CommanderJumpSlam,Player);
            if (Commander->EnemyMove==EEnemyMove::CommanderJumpSlam) JumpClock=30.f;
            return;
        }
        if (Distance <= 780.f)
        {
            const EEnemyMove Move = Distance > 480.f ? EEnemyMove::CommanderRush
                : AttackCycle % 2 == 0 ? EEnemyMove::CommanderSlam : EEnemyMove::CommanderRush;
            if (Move != EEnemyMove::CommanderCleave || Distance <= 310.f)
            {
                Commander->BeginEnemyMove(Move,Player);
                if (Commander->EnemyMove == Move) ++AttackCycle;
                return;
            }
        }
    }
    // Measured approach leaves room to punish recovery and deal with the adds.
    const FVector Travel = Distance > 285.f ? Toward : FVector::CrossProduct(FVector::UpVector,Toward)*Commander->EnemyOrbitSign;
    if (Commander->IsEnemyGroundAheadSafe(Travel)) Commander->AddMovementInput(Travel,Distance > 500.f ? 1.f : .55f);
}

bool UImpCommanderBehavior::ResolveMove(EEnemyMove Move)
{
    // The jump-slam still hits normally; landing also calls fresh Imps.
    if (Move == EEnemyMove::CommanderJumpSlam && Fighter()->bBossEncounter) SpawnImps();
    return false;
}

void UImpCommanderBehavior::SpawnImps()
{
    AArenaFighter* Commander = Fighter();
    if (!Commander->bEnemy || !Commander->IsAlive() || !Commander->bBossEncounter) return;
    AEnemySpawnPoint* Site = Commander->EncounterSite.Get();
    // Summons must belong to a live encounter so the exit cannot unlock early.
    if (!Site || Site->bCleared || !Site->bActivated || !Site->bBoss) return;
    Imps.RemoveAll([](const auto& Imp) { return !Imp.IsValid() || !Imp->IsAlive(); });
    const int32 Available = FMath::Clamp(ImpLimit,1,6) - Imps.Num();
    if (Available <= 0) return;
    FCollisionObjectQueryParams FloorTypes;
    FloorTypes.AddObjectTypesToQuery(ECC_WorldStatic);
    FloorTypes.AddObjectTypesToQuery(ECC_WorldDynamic);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(CommanderSummonFloor),false,Commander);
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
        const FVector P = Commander->GetActorLocation() + FVector(FMath::Cos(Angle)*Radius,FMath::Sin(Angle)*Radius,0);
        FHitResult Floor;
        if (!GetWorld()->LineTraceSingleByObjectType(Floor,P+FVector(0,0,1000),P-FVector(0,0,800),FloorTypes,Query)
            || Floor.ImpactNormal.Z < Defaults->GetCharacterMovement()->GetWalkableFloorZ()) continue;
        const FVector Location(Floor.ImpactPoint.X,Floor.ImpactPoint.Y,Floor.ImpactPoint.Z+CapsuleHalf+12.f);
        if (GetWorld()->OverlapBlockingTestByChannel(Location,FQuat::Identity,ECC_Pawn,
            FCollisionShape::MakeCapsule(CapsuleRadius,CapsuleHalf),Query)) continue;
        FActorSpawnParameters Params;
        Params.Owner = Commander;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding;
        auto* Imp = GetWorld()->SpawnActor<AArenaFighter>(Location,FRotator::ZeroRotator,Params);
        if (!Imp) continue;
        Imp->MakeEnemy(1,false);
        Imp->SetEnemyType(EHellgirlEnemyType::Imps);
        Imp->HomePosition = Commander->HomePosition;
        Imp->bGuardHome = false;
        // New arrivals visibly emerge before joining the coordinated attacks.
        Imp->EnemyMoveCooldown = 1.f + Created*.35f;
        Imp->EnemyOrbitSign = Created%2 ? -1.f : 1.f;
        Imp->Tags.Add(TEXT("CommanderReinforcement"));
        if (!Site->RegisterReinforcement(Imp)) { Imp->Destroy(); continue; }
        Imps.Add(Imp);
        ++Created;
        DrawDebugSphere(GetWorld(),Location,110.f,16,FColor(255,105,35),false,.65f,0,5.f);
    }
    UE_LOG(LogTemp,Display,TEXT("IMP COMMANDER: summoned %d Imps, %d alive"),Created,GetSummonedImpCount());
}

FString UImpCommanderBehavior::GetHudStatus() const
{
    const int32 Count = GetSummonedImpCount();
    return FString::Printf(TEXT("PROTECTIVE IMPS: %d / 3 / %s"),Count,Count>0 ? TEXT("90% DAMAGE REDUCTION") : TEXT("VULNERABLE"));
}
