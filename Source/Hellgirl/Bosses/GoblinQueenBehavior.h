#pragma once
#include "CoreMinimal.h"
#include "Bosses/BossBehavior.h"
#include "GoblinQueenBehavior.generated.h"

// Goblin Queen (World I boss). Melee at first; at 70% she vanishes and summons
// two packs, returning with shadow claws once either pack falls. At 30% she
// summons another pack and her claws speed up. At 5% she vanishes until ten
// summoned goblins die, then returns at half health. Without bBossEncounter she
// is an ordinary elite: claws from the start, no phases or summons.
UCLASS()
class HELLGIRL_API UGoblinQueenBehavior : public UBossBehavior
{
    GENERATED_BODY()
public:
    virtual bool TickPhases(float Dt) override;
    virtual void TickTactics(float Dt, AArenaFighter* Player) override;
    virtual bool ResolveMove(EEnemyMove Move) override;
    virtual float FilterDamage(float Damage) const override;
    virtual bool IsHidden() const override { return bHidden; }
    virtual FString GetDisplayName() const override { return TEXT("GOBLIN QUEEN"); }
    virtual FString GetHudStatus() const override;

    void SpawnGoblinPack(int32 Count, bool FirstPack = false);
    void FireShadowClaw();

    // 0 = full health, 1 = after 70%, 2 = after 30%, 3 = after 5%.
    int32 Phase = 0;
    bool bHidden = false;
    bool bFinalReturned = false;
    float SpawnClock = 0.f;
    int32 FinalKills = 0;
    TArray<TWeakObjectPtr<AArenaFighter>> Goblins, FirstPack, SecondPack;

private:
    void Hide();
};
