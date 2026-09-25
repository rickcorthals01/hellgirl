#pragma once
#include "Rules/FistCombatRules.h"

// Style combo meter: landing hits with varied moves builds a damage bonus from 1.1x up to 2.0x.
// Engine-independent so it can be checked without the editor.
namespace HellgirlCombo
{
constexpr int MaxTier = 10;             // 1.0x + 10 x 0.1 = 2.0x
constexpr float PointsPerTier = 100.f;
constexpr float FreshGain = 40.f;      // a hit with a move family you have not used in your last few hits
constexpr float RepeatFactor = .55f;    // each further hit in a row with the same family earns this much less
constexpr float GraceSeconds = 3.f;     // no drain while you keep landing hits
constexpr float DrainPerSecond = 70.f;
constexpr float HurtLoss = 2.f * PointsPerTier;

// Moves are grouped by how you perform them; varying the family is what counts, not the exact animation.
enum class Family { Light, Heavy, AirLight, AirHeavy, AfterDodge, Charged, Sword, Other };

inline Family FamilyOf(FistCombat::Move Type)
{
    using M = FistCombat::Move;
    switch (Type)
    {
    case M::RightPunch: case M::LeftPunch: case M::DoubleJab: case M::HeavyPunch: case M::Elbow: return Family::Light;
    case M::RightHeavyKick: case M::LeftHeavyKick: case M::LegSweep: case M::TurningKick: case M::FollowKick: return Family::Heavy;
    case M::AirPunch: case M::AirLeftPunch: case M::AirKick: return Family::AirLight;
    case M::AirSlam: case M::AirCrashKick: return Family::AirHeavy;
    case M::Headbutt: case M::DodgeSlam: case M::DodgeUppercut: return Family::AfterDodge;
    case M::ChargedStrike: return Family::Charged;
    case M::SwordSlash: case M::SwordBackslash: case M::SwordThrust: case M::SwordSpin: return Family::Sword;
    default: return Family::Other;
    }
}

struct FMeter
{
    float Points = 0.f;
    float SinceHit = 100.f;
    Family Recent[3] = {Family::Other, Family::Other, Family::Other}; // newest first
    int RecentCount = 0;
    int Streak = 0;           // hits in a row with the same family
    float LastGain = 0.f;

    int Tier() const { const int T = static_cast<int>(Points / PointsPerTier); return T < 0 ? 0 : (T > MaxTier ? MaxTier : T); }
    float Multiplier() const { return 1.f + .1f * Tier(); }
    // Progress toward the next tier, 0..1 (1 at the top tier).
    float Fill() const { return Tier() >= MaxTier ? 1.f : (Points - Tier() * PointsPerTier) / PointsPerTier; }
    bool IsSpamming() const { return Streak >= 3; }

    // A move landed (once per attack, however many enemies it hit).
    float Landed(FistCombat::Move Type)
    {
        const Family F = FamilyOf(Type);
        bool Seen = false;
        for (int I = 0; I < RecentCount; ++I) Seen |= Recent[I] == F;
        Streak = RecentCount > 0 && Recent[0] == F ? Streak + 1 : 0;
        float Gain = FreshGain;
        if (Streak > 0) { for (int I = 0; I < Streak; ++I) Gain *= RepeatFactor; }  // same family again and again
        else if (Seen) Gain *= .75f;                                               // back to a family used a moment ago
        for (int I = 2; I > 0; --I) Recent[I] = Recent[I - 1];
        Recent[0] = F;
        RecentCount = RecentCount < 3 ? RecentCount + 1 : 3;
        const float Cap = (MaxTier + 1) * PointsPerTier - 1.f;
        Points = Points + Gain > Cap ? Cap : Points + Gain;
        SinceHit = 0.f;
        LastGain = Gain;
        return Gain;
    }

    void Hurt() { Points = Points > HurtLoss ? Points - HurtLoss : 0.f; Streak = 0; }

    void Tick(float Dt)
    {
        SinceHit += Dt;
        if (SinceHit > GraceSeconds) Points = Points > DrainPerSecond * Dt ? Points - DrainPerSecond * Dt : 0.f;
        if (Points <= 0.f) { RecentCount = 0; Streak = 0; }
    }
};
}
