#pragma once

// Enemy commitments reuse the regular attack clock, hit validation and counter
// window. This separate identity controls movement, telegraphs and boss hooks.
enum class EEnemyMove : unsigned char
{
    None, ImpClaw, ImpPounce, FlyingDive,
    CommanderCleave, CommanderRush, CommanderSlam, CommanderJumpSlam, GoblinSlash, QueenMelee, QueenClaw,
    // A fast, weak goblin slash that a perfect dodge cannot counter (Rules/EnemyTuning.h).
    GoblinQuickSlash,
    // World II. The rat: a quick bite that cannot be countered, a fast punch (sometimes followed by a second), a
    // dodge roll. The frog: a quick punch, a punch in the air, and a slam down from the air.
    RatBite, RatPunch, RatPunch2, RatRoll, FrogPunch, FrogAirPunch, FrogSlam,
    // The Rat Queen: right slash, left slash, a charged heavy slash that dashes her far forward, a jump slam, and the
    // pose she strikes calling her rats (no hit).
    RatQueenSlash, RatQueenSlash2, RatQueenHeavy, RatQueenJumpSlam, RatQueenSummon,
    // The Deprived: the execute (once per life, charged, lunging), then slow slices and claws.
    DeprivedExecute, DeprivedSlice, DeprivedClaw
};
