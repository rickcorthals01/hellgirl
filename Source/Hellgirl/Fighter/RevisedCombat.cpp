#include "Fighter/ArenaFighter.h"
#include "Levels/ArenaGameMode.h"
#include "Progress/HellgirlWallet.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"

void AArenaFighter::SelectWeapon(int32 Weapon)
{
    if (bEnemy || !IsAlive()) return;
    const auto* GM = Cast<AArenaGameMode>(UGameplayStatics::GetGameMode(this));
    if (GM && !GM->Prompt.IsEmpty()) return; // D-pad belongs to the portal prompt.
    WeaponMenuClock = 3.f;
    if (Weapon > 1) { MoveLabel = TEXT("WEAPON NOT AVAILABLE YET"); MoveLabelClock = 2.f; return; }
    if (AttackClock > 0.f || DodgeClock > 0.f || bGroundImpactPending || bHeavyHeld) return;
    SelectedWeapon = Weapon;
    Combo = AirCombo = 0; ComboClock = BufferClock = 0.f;
    Sword->SetVisibility(Weapon == 1);
    MoveLabel = Weapon == 1 ? TEXT("SWORD READY") : TEXT("FISTS READY");
    MoveLabelClock = 2.f;
}

void AArenaFighter::Special()
{
    if (auto* GM=Cast<AArenaGameMode>(UGameplayStatics::GetGameMode(this)); GM && GM->bForestHub) return;
    if (bEnemy || !IsAlive() || UltimateClock > 0.f || HitClock > 0.f || KnockdownClock > 0.f
        || AttackClock > 0.f || DodgeClock > 0.f || bGroundImpactPending) return;
    if (!HellgirlProgress::UltimatesUnlocked()) { MoveLabel = TEXT("YOUR POWER IS STILL SEALED"); MoveLabelClock = 2.f; return; }
    if (!HasUltimate()) { MoveLabel = TEXT("THIS OUTFIT'S ULTIMATE IS NOT AVAILABLE YET"); MoveLabelClock = 2.f; return; }
    if (Energy < MaxEnergy) { MoveLabel = TEXT("ULTIMATE NEEDS ALL FOUR TUBES FULL"); MoveLabelClock = 2.f; return; }
    CancelCharge(); BufferClock = 0.f;
    NoteEnergySpent(Energy);
    Energy = 0.f; ActiveUltimate = SelectedOutfit;
    UltimateClock = ActiveUltimate == 0 ? 8.f : ActiveUltimate == 1 ? 6.f : 6.f;
    UltimatePulseClock = 0.f;
    MoveLabel = ActiveUltimate == 0 ? TEXT("CAVEMAN STRENGTH") : ActiveUltimate == 1 ? TEXT("PUNISHMENT") : TEXT("QUEEN'S CLAWS");
    MoveLabelClock = 3.f;
    if (ActiveUltimate == 1)
    {
        TArray<AActor*> Fighters;
        UGameplayStatics::GetAllActorsOfClass(this, StaticClass(), Fighters);
        for (AActor* Actor : Fighters)
        {
            auto* Enemy = Cast<AArenaFighter>(Actor);
            if (!Enemy || !Enemy->bEnemy || !Enemy->IsAlive()) continue;
            if (Enemy->bBossEncounter || FVector::Dist(GetActorLocation(), Enemy->GetActorLocation()) > 650.f) continue;
            Enemy->ParalysisClock = 3.f;
            Enemy->CancelEnemyMove();
            Enemy->AttackClock = Enemy->HitClock = Enemy->KnockdownClock = 0.f;
            Enemy->bHitResolved = Enemy->bSecondHitResolved = true;
            Enemy->bCombatLaunched = false;
            Enemy->QueuedCombatImpacts.Reset();
            Enemy->AttackFlash->SetVisibility(false);
            Enemy->GetCharacterMovement()->StopMovementImmediately();
            Enemy->GetCharacterMovement()->ClearAccumulatedForces();
            Enemy->GetCharacterMovement()->DisableMovement();
        }
        UltimatePulse(45.f, 650.f);
    }
}

void AArenaFighter::UltimatePulse(float Damage, float Range)
{
    TArray<AActor*> Fighters;
    UGameplayStatics::GetAllActorsOfClass(this, StaticClass(), Fighters);
    for (AActor* Actor : Fighters)
    {
        auto* Enemy = Cast<AArenaFighter>(Actor);
        if (!Enemy || !Enemy->bEnemy || !Enemy->IsAlive()) continue;
        if (Range > 0.f && FVector::Dist(GetActorLocation(), Enemy->GetActorLocation()) > Range) continue;
        // Ultimates hit reliably without cancelling boss actions or releasing paralysis.
        Enemy->Health = FMath::Max(0.f, Enemy->Health - Enemy->FilterEnemyDamage(Damage));
        DrawDebugSphere(GetWorld(), Enemy->GetActorLocation(), 95.f, 12, FColor(200,70,255), false, .3f, 0, 4.f);
        if (!Enemy->IsAlive()) Enemy->HandleDeath(FVector(0,0,120));
    }
}

void AArenaFighter::UpdateUltimate(float Dt)
{
    if (bEnemy) return;
    WeaponMenuClock = FMath::Max(0.f, WeaponMenuClock - Dt);
    if (!IsAlive()) { UltimateClock = 0.f; return; }
    if (UltimateClock <= 0.f) return;
    if (ActiveUltimate == 4)
    {
        const float ActiveDt = FMath::Min(Dt, UltimateClock);
        UltimatePulseClock -= ActiveDt;
        // Six pulses at seconds 0,1,2,3,4,5; hitches do not drop pulses.
        while (UltimatePulseClock < 0.f) { UltimatePulse(12.f, 650.f); UltimatePulseClock += 1.f; }
    }
    UltimateClock = FMath::Max(0.f, UltimateClock - Dt);
}

void AArenaFighter::NoteEnergySpent(float Amount)
{
    if (bEnemy || Amount <= 0.f) return;
    if (auto* Wallet = Cast<UHellgirlWallet>(GetGameInstance()); Wallet && Wallet->bInLevel) Wallet->LevelSouls.EnergySpent += Amount;
}
