#include "Fighter/ArenaFighter.h"
#include "Bosses/BossBehavior.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Rules/EnemyTuning.h"

bool AArenaFighter::IsBossHidden() const
{
    return BossBehavior && BossBehavior->IsHidden();
}

float AArenaFighter::FilterEnemyDamage(float Damage)
{
    if (!bEnemy) return Damage;
    if (bStorySurrendered) return 0.f;
    return BossBehavior ? BossBehavior->FilterDamage(Damage) : Damage;
}

void AArenaFighter::UpdateGoblinTactics(float Dt,AArenaFighter* Player)
{
    if (!Player || AttackClock>0.f || EnemyMoveCooldown>0.f) return;
    const FVector Delta=Player->GetActorLocation()-GetActorLocation(); const float Distance=Delta.Size2D();
    SetActorRotation(Delta.GetSafeNormal2D().Rotation());
    // The quick slash first (once every few seconds), otherwise the dagger slash.
    if (CanBeginEnemyMove(Player) && GoblinQuickSlashClock<=0.f && Distance<EnemyTuning::GoblinQuickSlashReach)
    {
        BeginEnemyMove(EEnemyMove::GoblinQuickSlash,Player);
        if (EnemyMove==EEnemyMove::GoblinQuickSlash) { GoblinQuickSlashClock=EnemyTuning::GoblinQuickSlashCooldown; return; }
    }
    if (CanBeginEnemyMove(Player) && Distance<180.f) { BeginEnemyMove(EEnemyMove::GoblinSlash,Player); return; }
    if (Distance>150.f && IsEnemyGroundAheadSafe(Delta.GetSafeNormal2D())) AddMovementInput(Delta.GetSafeNormal2D(),1.f);
}
