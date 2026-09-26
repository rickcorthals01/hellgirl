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

// World II, the rat: fast and nimble. Its bite is a quick lunge (0.45 s) for low damage that a perfect dodge cannot
// counter (a normal dodge still avoids it), once every few seconds. Its punch is a very fast charge and release
// (0.55 s) for medium damage, sometimes followed straight away by a second punch. When Hellgirl winds up an attack
// close by, it may dodge-roll aside (untouchable while rolling).
constexpr float RatSpeed = 470.f;
constexpr float RatBiteSeconds = .45f, RatBiteDamageScale = .5f, RatBiteCooldown = 4.f, RatBiteReach = 210.f;
constexpr float RatPunchSeconds = .55f, RatPunchDamageScale = 1.15f, RatPunchRecovery = .55f, RatPunchReach = 190.f;
constexpr float RatSecondPunchChance = .55f;
constexpr float RatRollSeconds = .55f, RatRollDistance = 330.f, RatRollChance = .4f, RatRollCooldown = 3.f;
constexpr int32 RatAttackSlots = 4;

// World II, the frog: always hopping, fast and high. On the ground a quick punch (0.45 s, low damage); in the air a
// punch when level with Hellgirl, or a slam straight down onto her from high up: medium damage in a circle, a small
// blast that pushes her back, and a smaller hit and push for other enemies caught in it.
constexpr float FrogHopMin = .3f, FrogHopMax = .75f;       // pause on the ground between hops
constexpr float FrogHopUp = 720.f, FrogHopForward = 520.f;  // launch speeds (cm/s)
constexpr float FrogPunchSeconds = .45f, FrogPunchDamageScale = .6f, FrogPunchReach = 175.f;
constexpr float FrogAirPunchSeconds = .4f, FrogAirPunchDamageScale = .6f;
constexpr float FrogSlamSeconds = 1.1f, FrogSlamDamageScale = 1.4f, FrogSlamRadius = 320.f, FrogSlamBlast = 520.f;
constexpr float FrogSlamCooldown = 6.f, FrogSlamEnemyDamage = 6.f, FrogSlamEnemyPush = 420.f;
constexpr int32 FrogAttackSlots = 3;
// The Frog King (the swamp run's mini-boss): a bigger frog that hops higher and slams more often and wider.
constexpr float FrogKingHealth = 750.f, FrogKingSlamCooldown = 2.8f, FrogKingSlamRadius = 460.f;
}
