#include "Bosses/GoblinQueenBehavior.h"
#include "Fighter/ArenaFighter.h"
#include "Levels/ArenaGameMode.h"
#include "Enemies/EnemySpawnPoint.h"
#include "Enemies/ShadowClaw.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

float UGoblinQueenBehavior::FilterDamage(float Damage) const
{
    if (bHidden) return 0.f;
    const AArenaFighter* Queen = Fighter();
    if (Queen->bBossEncounter && !bFinalReturned)
    {
        const float Threshold=Phase==0 ? .7f : Phase==1 ? .3f : .05f;
        // Prevent burst attacks and ultimates skipping mandatory phases.
        Damage=FMath::Min(Damage,FMath::Max(0.f,Queen->Health-Queen->MaxHealth*Threshold));
    }
    return Damage;
}

void UGoblinQueenBehavior::SpawnGoblinPack(int32 Count,bool bFirstPack)
{
    AArenaFighter* Queen=Fighter();
    auto* Site=Queen->EncounterSite.Get(); if (!Site || Site->bCleared) return;
    for (int32 I=0,Attempts=0;I<Count && Attempts<32;++Attempts)
    {
        const float Angle=Attempts*2.39996f+GetWorld()->GetTimeSeconds();
        const FVector P=Queen->HomePosition+FVector(FMath::Cos(Angle)*550.f,FMath::Sin(Angle)*650.f,0);
        FHitResult Floor; FCollisionQueryParams Q; Q.AddIgnoredActor(Queen);
        if (!GetWorld()->LineTraceSingleByChannel(Floor,P+FVector(0,0,800),P-FVector(0,0,1000),ECC_Visibility,Q) || Floor.ImpactNormal.Z<.7f) continue;
        FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;
        auto* Goblin=GetWorld()->SpawnActor<AArenaFighter>(Floor.ImpactPoint+FVector(0,0,110),FRotator::ZeroRotator,Params);
        if (!Goblin) continue;
        Goblin->MakeEnemy(1,false); Goblin->SetEnemyType(EHellgirlEnemyType::Goblins); Goblin->HomePosition=Queen->HomePosition;
        if (!Site->RegisterReinforcement(Goblin)) { Goblin->Destroy(); continue; }
        Goblins.Add(Goblin); if (bFirstPack) FirstPack.Add(Goblin);
        else if (Phase==1 && bHidden) SecondPack.Add(Goblin);
        ++I;
    }
}

void UGoblinQueenBehavior::Hide()
{
    AArenaFighter* Queen=Fighter();
    Queen->CancelEnemyMove(); Queen->AttackClock=0.f; bHidden=true; Queen->SetActorHiddenInGame(true); Queen->SetActorEnableCollision(false);
    Queen->GetCharacterMovement()->StopMovementImmediately(); Queen->GetCharacterMovement()->ClearAccumulatedForces(); Queen->GetCharacterMovement()->DisableMovement(); Queen->AttackFlash->SetVisibility(false);
}

bool UGoblinQueenBehavior::TickPhases(float Dt)
{
    AArenaFighter* Queen=Fighter();
    if (!Queen->bBossEncounter) return bHidden;
    const float Health=Queen->Health, MaxHealth=Queen->MaxHealth;
    for (int32 I=Goblins.Num()-1;I>=0;--I)
        if (!Goblins[I].IsValid() || !Goblins[I]->IsAlive()) { if (Phase==3) ++FinalKills; Goblins.RemoveAt(I); }
    if (Phase==0 && Health<=MaxHealth*.7f+.01f)
    {
        Phase=1; Hide(); SpawnGoblinPack(3,true); SpawnGoblinPack(3); SpawnClock=2.f; return bHidden;
    }
    if (Phase==1 && !bHidden && Health<=MaxHealth*.3f+.01f) { Phase=2; SpawnGoblinPack(3); if (auto* GM=Cast<AArenaGameMode>(UGameplayStatics::GetGameMode(Queen))) GM->QueueStory(TEXT("QueenLowHealth")); }
    if (Phase==2 && Health<=MaxHealth*.05f+.01f) { Phase=3; FinalKills=0; Hide(); SpawnClock=0.f; }
    if (Phase==3)
    {
        SpawnClock-=Dt;
        if (SpawnClock<=0.f && Goblins.Num()<18) { SpawnGoblinPack(3); SpawnClock=4.f; }
    }
    if (bHidden)
    {
        bool FirstPackDead=true;
        for (const auto& G:FirstPack) if (G.IsValid() && G->IsAlive()) FirstPackDead=false;
        bool SecondPackDead=true;
        for (const auto& G:SecondPack) if (G.IsValid() && G->IsAlive()) SecondPackDead=false;
        if (Phase==1) SpawnClock-=Dt;
        if ((Phase==1 && (FirstPackDead || SecondPackDead) && SpawnClock<=0.f) || (Phase==3 && FinalKills>=10))
        {
            if (Phase==3) { Queen->Health=MaxHealth*.5f; bFinalReturned=true; }
            const FVector P=Queen->HomePosition+FVector(450,600,700); FHitResult Floor; FCollisionQueryParams Q; Q.AddIgnoredActor(Queen);
            if (GetWorld()->LineTraceSingleByChannel(Floor,P,P-FVector(0,0,1600),ECC_Visibility,Q))
                Queen->SetActorLocation(Floor.ImpactPoint+FVector(0,0,Queen->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+10));
            bHidden=false; Queen->SetActorHiddenInGame(false); Queen->SetActorEnableCollision(true); Queen->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
            Queen->EnemyMoveCooldown=1.f;

        }
    }
    return bHidden;
}

void UGoblinQueenBehavior::TickTactics(float Dt,AArenaFighter* Player)
{
    AArenaFighter* Queen=Fighter();
    if (!Player || Queen->AttackClock>0.f || Queen->EnemyMoveCooldown>0.f || bHidden) return;
    const FVector Delta=Player->GetActorLocation()-Queen->GetActorLocation(); const float Distance=Delta.Size2D();
    Queen->SetActorRotation(Delta.GetSafeNormal2D().Rotation());
    if (Queen->CanBeginEnemyMove(Player))
    {
        if (Distance<240.f) { Queen->BeginEnemyMove(EEnemyMove::QueenMelee,Player); return; }
        if ((Phase>0 || !Queen->bBossEncounter) && Distance<1800.f) { Queen->BeginEnemyMove(EEnemyMove::QueenClaw,Player); return; }
    }
    if (Distance>150.f && Queen->IsEnemyGroundAheadSafe(Delta.GetSafeNormal2D())) Queen->AddMovementInput(Delta.GetSafeNormal2D(),1.f);
}

bool UGoblinQueenBehavior::ResolveMove(EEnemyMove Move)
{
    if (Move!=EEnemyMove::QueenClaw) return false;
    FireShadowClaw();
    return true;
}

void UGoblinQueenBehavior::FireShadowClaw()
{
    AArenaFighter* Queen=Fighter();
    auto* Player=UGameplayStatics::GetPlayerPawn(Queen,0); if (!Player) return;
    FActorSpawnParameters Params; Params.Owner=Queen;
    auto* Claw=GetWorld()->SpawnActor<AShadowClaw>(Queen->GetActorLocation()+Queen->GetActorForwardVector()*110.f,Queen->GetActorRotation(),Params);
    if (Claw) Claw->Velocity=(Player->GetActorLocation()-Claw->GetActorLocation()).GetSafeNormal()*(Phase>=2 ? 750.f : 500.f);
}

FString UGoblinQueenBehavior::GetHudStatus() const
{
    return bHidden ? TEXT("DEFEAT THE GOBLINS TO BRING HER BACK") : FString::Printf(TEXT("SHADOW PHASE %d"),Phase+1);
}
