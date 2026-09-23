#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Enemies/EnemyMovesetState.h"
#include "Enemies/EnemyTypes.h"
#include "BossBehavior.generated.h"

class AArenaFighter;

// Boss-specific rules for an enemy fighter: phases, summons, damage gates and
// tactics. AArenaFighter::SetEnemyType attaches the matching subclass, and the
// fighter calls these hooks from its own tick so ordering stays deterministic.
// To add a boss, subclass this and add it to UBossBehavior::ClassFor.
UCLASS(Abstract)
class HELLGIRL_API UBossBehavior : public UActorComponent
{
    GENERATED_BODY()
public:
    UBossBehavior();
    static TSubclassOf<UBossBehavior> ClassFor(EHellgirlEnemyType Type);

    // Runs every tick while the boss is alive, before combat physics.
    // Return true to skip the rest of the fighter's tick (for example while hidden).
    virtual bool TickPhases(float Dt) { return false; }
    // Replaces the generic enemy tactics while the player is in reach.
    virtual void TickTactics(float Dt, AArenaFighter* Player) {}
    // Called at an attack's contact point. Return true if the move is fully
    // handled here and the regular melee sweep should be skipped.
    virtual bool ResolveMove(EEnemyMove Move) { return false; }
    virtual float FilterDamage(float Damage) const { return Damage; }
    virtual bool IsHidden() const { return false; }
    virtual FString GetDisplayName() const { return TEXT("BOSS"); }
    // Second line of the boss health panel.
    virtual FString GetHudStatus() const { return FString(); }

protected:
    AArenaFighter* Fighter() const;
};
