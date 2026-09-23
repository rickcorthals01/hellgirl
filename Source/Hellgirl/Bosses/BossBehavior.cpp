#include "Bosses/BossBehavior.h"
#include "Bosses/GoblinQueenBehavior.h"
#include "Bosses/ImpCommanderBehavior.h"
#include "Enemies/EnemyTypes.h"
#include "Fighter/ArenaFighter.h"

UBossBehavior::UBossBehavior()
{
    // The owning fighter drives every hook from its own tick.
    PrimaryComponentTick.bCanEverTick = false;
}

TSubclassOf<UBossBehavior> UBossBehavior::ClassFor(EHellgirlEnemyType Type)
{
    switch (Type)
    {
    case EHellgirlEnemyType::GoblinQueen: return UGoblinQueenBehavior::StaticClass();
    case EHellgirlEnemyType::ImpCommander: return UImpCommanderBehavior::StaticClass();
    default: return nullptr;
    }
}

AArenaFighter* UBossBehavior::Fighter() const
{
    return CastChecked<AArenaFighter>(GetOwner());
}
