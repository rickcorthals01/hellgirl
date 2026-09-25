#pragma once
#include "CoreMinimal.h"

// Souls and Soul Coins. Souls are picked up inside a level (each level starts with none). At a soul portal they can
// buy upgrades, or be stocked: sent to camp at once as Soul Coins, safe but no longer spendable. Winning the level
// deposits every Soul earned there (spending does not lower it) minus what was already stocked, plus three bonuses.
// Falling deposits nothing more.
namespace HellgirlSouls
{
// Par time grows with the fighting: ParBase seconds plus ParPerKill for every enemy defeated.
constexpr float ParBase = 45.f;
constexpr float ParPerKill = 2.5f;
// Speed: up to SpeedShare of the Souls earned when finished within par, falling to nothing at twice par.
constexpr float SpeedShare = .3f;
// Combo: up to ComboShare of the Souls earned, times the share of fighting time with a combo multiplier going.
constexpr float ComboShare = .3f;
// Energy: one Soul Coin for every EnergyPerCoin energy spent on charges, slams and ultimates.
constexpr float EnergyPerCoin = 10.f;

struct FReward
{
    int64 Earned = 0, Stocked = 0, Speed = 0, Combo = 0, Energy = 0, Total = 0; // Total: Soul Coins deposited at the end
    float Time = 0.f, Par = 0.f, Uptime = 0.f, EnergySpent = 0.f;
};

inline FReward Compute(int64 Earned, int64 Stocked, float Time, int32 Kills, float ComboTime, float CombatTime, float EnergySpent)
{
    FReward R;
    R.Earned = FMath::Max<int64>(0, Earned);
    R.Stocked = FMath::Clamp<int64>(Stocked, 0, R.Earned);
    R.Time = FMath::Max(0.f, Time);
    R.Par = ParBase + ParPerKill * FMath::Max(0, Kills);
    R.Uptime = CombatTime > 0.f ? FMath::Clamp(ComboTime / CombatTime, 0.f, 1.f) : 0.f;
    R.EnergySpent = FMath::Max(0.f, EnergySpent);
    const float Pace = FMath::Clamp((2.f * R.Par - R.Time) / R.Par, 0.f, 1.f);
    R.Speed = FMath::RoundToInt(R.Earned * SpeedShare * Pace);
    R.Combo = FMath::RoundToInt(R.Earned * ComboShare * R.Uptime);
    R.Energy = FMath::FloorToInt(R.EnergySpent / EnergyPerCoin);
    R.Total = R.Earned - R.Stocked + R.Speed + R.Combo + R.Energy;
    return R;
}

inline FString Clock(float Seconds)
{
    const int32 S = FMath::Max(0, FMath::RoundToInt(Seconds));
    return FString::Printf(TEXT("%d:%02d"), S / 60, S % 60);
}
}
