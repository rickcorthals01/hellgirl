#pragma once
#include "CoreMinimal.h"

// Global tuning for ordinary (non-boss) enemies: fewer hit points each, more of them per wave.
// Bosses keep their own health; enemies they summon count as ordinary.
namespace EnemyTuning
{
constexpr float HealthScale = .65f;
constexpr float CountScale = 1.5f;

// Pacing: the breather between waves is a third of what the maps were authored with, and enemies in a
// trickling wave arrive this many seconds apart.
constexpr float WaveBreakScale = .35f;
constexpr float SpawnInterval = .3f;
inline float WaveBreak(float Authored) { return Authored * WaveBreakScale; }

inline float OrdinaryHealth(int32 Difficulty) { return (55.f + Difficulty * 12.f) * HealthScale; }
inline int32 WaveSize(int32 Authored) { return Authored <= 0 ? 0 : FMath::Max(1, FMath::RoundToInt(Authored * CountScale)); }

// Goblins hit harder and attack more often (2026-09-25). Their dagger slash deals the enemy's AttackDamage
// (9 + 2 per difficulty) times GoblinDamageScale: 13 at difficulty 1 (was a flat 10), more in later endless waves.
// A slash takes GoblinSlashSeconds plus GoblinRecovery before the next (1.9 s, was 2.55 s). Up to GoblinAttackSlots
// goblins may be mid-attack at once, and attacks start at least GoblinAttackSpacing apart (other enemies: 2 and 0.45 s).
constexpr float GoblinDamageScale = 1.2f;
constexpr float GoblinSlashSeconds = 1.3f;
constexpr float GoblinRecovery = .6f;
constexpr int32 GoblinAttackSlots = 6;
constexpr float GoblinAttackSpacing = .3f;
// The quick slash: a fast swing (0.5 s, the hit at 60%) for about 45% of the slash damage (5 at difficulty 1).
// A perfect dodge cannot counter it and it shows no counter flash. Each goblin can use it once every 6 s; a goblin
// arriving at Hellgirl opens with it.
constexpr float GoblinQuickSlashSeconds = .5f;
constexpr float GoblinQuickSlashRecovery = .35f;
constexpr float GoblinQuickSlashDamageScale = .45f;
constexpr float GoblinQuickSlashCooldown = 6.f;
constexpr float GoblinQuickSlashReach = 220.f;
}
