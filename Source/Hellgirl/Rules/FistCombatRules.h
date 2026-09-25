#pragma once

// Engine-independent move selection so branching can be tested without the editor.
namespace FistCombat
{
enum class Move
{
    RightPunch, LeftPunch, TurningKick, FollowKick,
    HeavyPunch, Headbutt, Elbow, Tackle, ShoulderThrow,
    DodgeUppercut, LegSweep, AirPunch, AirLeftPunch, AirKick, AirCrashKick,
    AirSlam, ChargedStrike, EnemyClaw,
    DoubleJab, RightHeavyKick, LeftHeavyKick, DodgeSlam,
    SwordSlash, SwordBackslash, SwordThrust, SwordSpin
};

struct AttackSpec
{
    Move Type;
    float Duration;
    float ContactFraction;
    float Damage;
    float Range;
    float Knockback;
    float Knockdown;
    int NextCombo;
};

inline AttackSpec Select(bool Heavy, int Combo, bool Airborne, bool AfterDodge, bool Sword = false)
{
    if (Airborne)
    {
        if (Heavy) return {Move::AirSlam, .65f, .5f, 16.f, 280.f, 300.f, .8f, 0};
        switch (Combo)
        {
        case 1: return {Move::AirLeftPunch, .3f, .4f, 18.f, 200.f, 30.f, 0.f, 2};
        case 2: return {Move::AirKick, .38f, .45f, 25.f, 235.f, 50.f, 0.f, 3};
        case 3: return {Move::AirCrashKick, .65f, .5f, 14.f, 360.f, 420.f, 1.1f, 4};
        default: return {Move::AirPunch, .3f, .4f, 18.f, 200.f, 30.f, 0.f, 1};
        }
    }
    if (AfterDodge)
        // Dodge + heavy: a slam that hits everyone around her (area move, softer toward the edge) and launches them
        // away. It costs a full energy tube (Rules/CombatEnergyRules.h).
        return Heavy ? AttackSpec{Move::DodgeSlam, .55f, .45f, 26.f, 400.f, 450.f, 0.f, 0}
                     : AttackSpec{Move::Headbutt, .4f, .4f, 28.f, 180.f, 100.f, 0.f, 0};
    if (Heavy)
    {
        switch (Combo)
        {
        case 1: return {Move::LeftHeavyKick, .58f, .45f, 34.f, 210.f, 100.f, 0.f, 2};
        case 2: return {Move::RightHeavyKick, .58f, .45f, 36.f, 210.f, 100.f, 0.f, 3};
        case 3: return {Move::LegSweep, .65f, .45f, 24.f, 300.f, 300.f, .9f, 0};
        default: return {Move::RightHeavyKick, .58f, .45f, 34.f, 210.f, 100.f, 0.f, 1};
        }
    }
    if (Sword)
    {
        switch (Combo)
        {
        case 1: return {Move::SwordBackslash, .36f, .4f, 18.f, 250.f, 60.f, 0.f, 2};
        case 2: return {Move::SwordThrust, .5f, .65f, 25.f, 260.f, 700.f, .7f, 3};
        case 3: return {Move::SwordSpin, .55f, .45f, 14.f, 300.f, 100.f, 0.f, 0};
        default: return {Move::SwordSlash, .36f, .4f, 18.f, 250.f, 60.f, 0.f, 1};
        }
    }
    switch (Combo)
    {
    case 1: return {Move::LeftPunch, .32f, .4f, 18.f, 175.f, 60.f, 0.f, 2};
    case 2: return {Move::RightPunch, .32f, .4f, 20.f, 175.f, 60.f, 0.f, 3};
    case 3: return {Move::DoubleJab, .58f, .25f, 12.f, 190.f, 20.f, 0.f, 0};
    default: return {Move::RightPunch, .32f, .4f, 18.f, 175.f, 60.f, 0.f, 1};
    }
}

inline AttackSpec Charged(float Fraction)
{
    const float Power = Fraction < 0.f ? 0.f : (Fraction > 1.f ? 1.f : Fraction);
    return {Move::ChargedStrike, .65f, .45f, 12.f + 16.f * Power,
        190.f + 270.f * Power, 200.f + 350.f * Power, Power >= .75f ? .9f : 0.f, 0};
}

inline bool IsGroundImpact(Move Type)
{
    return Type == Move::AirCrashKick || Type == Move::AirSlam;
}

inline bool IsPlayerAreaMove(Move Type)
{
    return Type == Move::ChargedStrike || IsGroundImpact(Type) || Type == Move::LegSweep
        || Type == Move::DodgeSlam || Type == Move::SwordSpin;
}

inline float AreaDamageScale(float Distance, float Range)
{
    // Keep the centre useful, then soften the damage toward the outside of
    // the area. Invalid radii never divide by zero or amplify a hit.
    if (Distance <= 0.f) return 1.f;
    if (Range <= 0.f || Distance >= Range) return .55f;
    const float InnerRadius = Range * .25f;
    if (Distance <= InnerRadius) return 1.f;
    const float T = (Distance - InnerRadius) / (Range - InnerRadius);
    const float Smooth = T * T * (3.f - 2.f * T);
    return 1.f - .45f * Smooth;
}

inline bool IsAirStrike(Move Type)
{
    return Type == Move::AirPunch || Type == Move::AirLeftPunch || Type == Move::AirKick || IsGroundImpact(Type);
}
}

