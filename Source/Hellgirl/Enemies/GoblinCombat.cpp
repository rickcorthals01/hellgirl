#include "Fighter/ArenaFighter.h"
#include "Levels/ArenaGameMode.h"
#include "Enemies/EnemySpawnPoint.h"
#include "Enemies/ShadowClaw.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"

float AArenaFighter::FilterEnemyDamage(float Damage)
{
    if (!bEnemy) return Damage;
    if (bQueenHidden || bStorySurrendered) return 0.f;
    if (EnemyType==EHellgirlEnemyType::ImpCommander && bBossEncounter && GetSummonedImpCount()>0) Damage*=.1f;
    if (EnemyType==EHellgirlEnemyType::GoblinQueen && bBossEncounter && !bQueenFinalReturned)
    {
        const float Threshold=GoblinPhase==0 ? .7f : GoblinPhase==1 ? .3f : .05f;
        // Prevent burst attacks and ultimates skipping mandatory phases.
        Damage=FMath::Min(Damage,FMath::Max(0.f,Health-MaxHealth*Threshold));
    }
    return Damage;
}
void AArenaFighter::SpawnGoblinPack(int32 Count,bool FirstPack)
{
    auto* Site=EncounterSite.Get(); if (!Site || Site->bCleared) return;
    for (int32 I=0,Attempts=0;I<Count && Attempts<32;++Attempts)
    {
        const float Angle=Attempts*2.39996f+GetWorld()->GetTimeSeconds();
        const FVector P=HomePosition+FVector(FMath::Cos(Angle)*550.f,FMath::Sin(Angle)*650.f,0);
        FHitResult Floor; FCollisionQueryParams Q; Q.AddIgnoredActor(this);
        if (!GetWorld()->LineTraceSingleByChannel(Floor,P+FVector(0,0,800),P-FVector(0,0,1000),ECC_Visibility,Q) || Floor.ImpactNormal.Z<.7f) continue;
        FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;
        auto* Goblin=GetWorld()->SpawnActor<AArenaFighter>(Floor.ImpactPoint+FVector(0,0,110),FRotator::ZeroRotator,Params);
        if (!Goblin) continue;
        Goblin->MakeEnemy(1,false); Goblin->SetEnemyType(EHellgirlEnemyType::Goblins); Goblin->HomePosition=HomePosition;
        if (!Site->RegisterReinforcement(Goblin)) { Goblin->Destroy(); continue; }
        QueenGoblins.Add(Goblin); if (FirstPack) QueenFirstPack.Add(Goblin);
        else if (GoblinPhase==1 && bQueenHidden) QueenSecondPack.Add(Goblin);
        ++I;
    }
}
void AArenaFighter::UpdateQueenPhases(float Dt)
{
    if (!bBossEncounter) return;
    for (int32 I=QueenGoblins.Num()-1;I>=0;--I)
        if (!QueenGoblins[I].IsValid() || !QueenGoblins[I]->IsAlive()) { if (GoblinPhase==3) ++GoblinFinalKills; QueenGoblins.RemoveAt(I); }
    auto Hide=[&]() {
        CancelEnemyMove(); AttackClock=0.f; bQueenHidden=true; SetActorHiddenInGame(true); SetActorEnableCollision(false);
        GetCharacterMovement()->StopMovementImmediately(); GetCharacterMovement()->ClearAccumulatedForces(); GetCharacterMovement()->DisableMovement(); AttackFlash->SetVisibility(false);
    };
    if (GoblinPhase==0 && Health<=MaxHealth*.7f+.01f)
    {
        GoblinPhase=1; Hide(); SpawnGoblinPack(3,true); SpawnGoblinPack(3); GoblinSpawnClock=2.f; return;
    }
    if (GoblinPhase==1 && !bQueenHidden && Health<=MaxHealth*.3f+.01f) { GoblinPhase=2; SpawnGoblinPack(3); if (auto* GM=Cast<AArenaGameMode>(UGameplayStatics::GetGameMode(this))) GM->QueueStory(TEXT("QueenLowHealth")); }
    if (GoblinPhase==2 && Health<=MaxHealth*.05f+.01f) { GoblinPhase=3; GoblinFinalKills=0; Hide(); GoblinSpawnClock=0.f; }
    if (GoblinPhase==3)
    {
        GoblinSpawnClock-=Dt;
        if (GoblinSpawnClock<=0.f && QueenGoblins.Num()<18) { SpawnGoblinPack(3); GoblinSpawnClock=4.f; }
    }
    if (bQueenHidden)
    {
        bool FirstPackDead=true;
        for (const auto& G:QueenFirstPack) if (G.IsValid() && G->IsAlive()) FirstPackDead=false;
        bool SecondPackDead=true;
        for (const auto& G:QueenSecondPack) if (G.IsValid() && G->IsAlive()) SecondPackDead=false;
        if (GoblinPhase==1) GoblinSpawnClock-=Dt;
        if ((GoblinPhase==1 && (FirstPackDead || SecondPackDead) && GoblinSpawnClock<=0.f) || (GoblinPhase==3 && GoblinFinalKills>=10))
        {
            if (GoblinPhase==3) { Health=MaxHealth*.5f; bQueenFinalReturned=true; }
            const FVector P=HomePosition+FVector(450,600,700); FHitResult Floor; FCollisionQueryParams Q; Q.AddIgnoredActor(this);
            if (GetWorld()->LineTraceSingleByChannel(Floor,P,P-FVector(0,0,1600),ECC_Visibility,Q))
                SetActorLocation(Floor.ImpactPoint+FVector(0,0,GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+10));
            bQueenHidden=false; SetActorHiddenInGame(false); SetActorEnableCollision(true); GetCharacterMovement()->SetMovementMode(MOVE_Falling);
            EnemyMoveCooldown=1.f;
            if (GoblinPhase==3) if (auto* GM=Cast<AArenaGameMode>(UGameplayStatics::GetGameMode(this))) GM->QueueStory(TEXT("QueenReturn"));
        }
    }
}
void AArenaFighter::UpdateGoblinTactics(float Dt,AArenaFighter* Player)
{
    if (!Player || AttackClock>0.f || EnemyMoveCooldown>0.f || bQueenHidden) return;
    const FVector Delta=Player->GetActorLocation()-GetActorLocation(); const float Distance=Delta.Size2D();
    const bool Queen=EnemyType==EHellgirlEnemyType::GoblinQueen;
    SetActorRotation(Delta.GetSafeNormal2D().Rotation());
    if (CanBeginEnemyMove(Player))
    {
        if (Distance<(Queen?240.f:180.f)) { BeginEnemyMove(Queen?EEnemyMove::QueenMelee:EEnemyMove::GoblinSlash,Player); return; }
        if (Queen && (GoblinPhase>0 || !bBossEncounter) && Distance<1800.f) { BeginEnemyMove(EEnemyMove::QueenClaw,Player); return; }
    }
    if (Distance>150.f && IsEnemyGroundAheadSafe(Delta.GetSafeNormal2D())) AddMovementInput(Delta.GetSafeNormal2D(),1.f);
}
void AArenaFighter::FireShadowClaw()
{
    auto* Player=UGameplayStatics::GetPlayerPawn(this,0); if (!Player) return;
    FActorSpawnParameters Params; Params.Owner=this;
    auto* Claw=GetWorld()->SpawnActor<AShadowClaw>(GetActorLocation()+GetActorForwardVector()*110.f,GetActorRotation(),Params);
    if (Claw) Claw->Velocity=(Player->GetActorLocation()-Claw->GetActorLocation()).GetSafeNormal()*(GoblinPhase>=2 ? 750.f : 500.f);
}
