#pragma once
#include "CoreMinimal.h"

// Global tuning for ordinary (non-boss) enemies: fewer hit points each, more of them per wave.
// Bosses keep their own health; enemies they summon count as ordinary.
namespace EnemyTuning
{
constexpr float HealthScale = .65f;
constexpr float CountScale = 1.5f;

inline float OrdinaryHealth(int32 Difficulty) { return (55.f + Difficulty * 12.f) * HealthScale; }
inline int32 WaveSize(int32 Authored) { return Authored <= 0 ? 0 : FMath::Max(1, FMath::RoundToInt(Authored * CountScale)); }
}
