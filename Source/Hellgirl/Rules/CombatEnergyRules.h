#pragma once
#include "FistCombatRules.h"

namespace HellgirlEnergy
{
constexpr float Capacity = 100.f;
constexpr float NormalHitGain = 12.f;
constexpr float HeavyHitGain = 18.f;
constexpr float TubeCapacity = Capacity / 4.f;
constexpr float ChargeCost = TubeCapacity * .5f;
constexpr float SlamCost = TubeCapacity * .5f;
// The dodge slam (dodge + heavy) is an area launch, so it costs a whole tube.
constexpr float DodgeSlamCost = TubeCapacity;

inline float Cost(FistCombat::Move Type)
{
    if (Type == FistCombat::Move::ChargedStrike || Type == FistCombat::Move::LegSweep) return ChargeCost;
    if (Type == FistCombat::Move::DodgeSlam) return DodgeSlamCost;
    return FistCombat::IsGroundImpact(Type) || Type == FistCombat::Move::SwordSpin ? SlamCost : 0.f;
}

// Award once per successful swing, independent of the number of targets hit.
inline float Gain(FistCombat::Move Type)
{
    using FistCombat::Move;
    switch (Type)
    {
    case Move::RightPunch: case Move::LeftPunch:
    case Move::TurningKick: case Move::FollowKick:
    case Move::DodgeUppercut: case Move::DoubleJab: case Move::Headbutt:
    case Move::SwordSlash: case Move::SwordBackslash: case Move::SwordThrust:
    case Move::AirPunch: case Move::AirLeftPunch: case Move::AirKick:
        return NormalHitGain;
    case Move::HeavyPunch: case Move::Elbow: case Move::RightHeavyKick: case Move::LeftHeavyKick:
    case Move::Tackle: case Move::ShoulderThrow:
        return HeavyHitGain;
    default:
        return 0.f;
    }
}
}
