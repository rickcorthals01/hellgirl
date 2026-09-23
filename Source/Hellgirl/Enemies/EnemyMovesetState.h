#pragma once

// Enemy commitments reuse the regular attack clock, hit validation and counter
// window. This separate identity controls movement, telegraphs and boss hooks.
enum class EEnemyMove : unsigned char
{
    None, ImpClaw, ImpPounce, FlyingDive,
    CommanderCleave, CommanderRush, CommanderSlam, CommanderJumpSlam, GoblinSlash, QueenMelee, QueenClaw
};
