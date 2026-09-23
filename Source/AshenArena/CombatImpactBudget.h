#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

// Shared by all bodies launched by one player area attack, including secondary
// collisions. Victims are weak references; the context expires with the launches.
struct FCombatImpactBudget
{
    static constexpr float DamagePerVictim = 6.f;
    TMap<TWeakObjectPtr<AActor>,float> DamageSpent;

    float Spend(AActor* Victim,float Requested)
    {
        if (!Victim || Requested <= 0.f) return 0.f;
        float& Spent = DamageSpent.FindOrAdd(Victim);
        const float Damage = FMath::Min(Requested,FMath::Max(0.f,DamagePerVictim-Spent));
        Spent += Damage;
        return Damage;
    }
};
