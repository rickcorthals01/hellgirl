#pragma once
#include "CoreMinimal.h"
#include "Bosses/BossBehavior.h"
#include "ImpCommanderBehavior.generated.h"

// Imp Commander (World II boss). Starts with three protective Imps; while any
// live he takes 90% less damage. Uses wind-up rush, telegraphed area slam and
// a jump-slam (30 s cooldown) that replenishes the Imps. Without bBossEncounter
// he is an ordinary elite: no Imps, no shield and a longer jump warning.
UCLASS()
class HELLGIRL_API UImpCommanderBehavior : public UBossBehavior
{
    GENERATED_BODY()
public:
    virtual void TickTactics(float Dt, AArenaFighter* Player) override;
    virtual bool ResolveMove(EEnemyMove Move) override;
    virtual float FilterDamage(float Damage) const override;
    virtual FString GetDisplayName() const override { return TEXT("IMP COMMANDER"); }
    virtual FString GetHudStatus() const override;

    void SpawnImps();
    int32 GetSummonedImpCount() const;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Commander", meta=(ClampMin="1", ClampMax="6")) int32 ImpLimit = 3;
    TArray<TWeakObjectPtr<AArenaFighter>> Imps;
    float JumpClock = 0.f;
    int32 AttackCycle = 0;
    bool bStarted = false;
};
