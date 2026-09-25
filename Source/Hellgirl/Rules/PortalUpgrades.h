#pragma once
#include "CoreMinimal.h"
#include "Math/RandomStream.h"

// Soul portal upgrades: each portal offers five at random, each buyable once with carried souls. A price is about
// what one portal's worth of kills drops (a Stage 2 portal: ~40 souls, later ones 60-120), so saving up buys several
// at once. They last for the rest of the level (or endless run) and stack; every level owned makes the next one
// 40% dearer.
namespace HellgirlUpgrades
{
enum class EUpgrade : uint8 { Fury, IronSkin, Vitality, Swiftness, SoulHunger, SecondWind, Bloodthirst, Greed, Count };
constexpr int32 Count = static_cast<int32>(EUpgrade::Count);
constexpr int32 OffersPerPortal = 5;

struct FInfo { const TCHAR* Name; const TCHAR* Detail; int32 BaseCost; };
inline const FInfo& Info(int32 Upgrade)
{
    static const FInfo Table[Count] = {
        {TEXT("FURY"),        TEXT("+15% damage"),              50},
        {TEXT("IRON SKIN"),   TEXT("-15% damage taken"),        45},
        {TEXT("VITALITY"),    TEXT("+25 max health, healed"),   35},
        {TEXT("SWIFTNESS"),   TEXT("+10% move speed"),          30},
        {TEXT("SOUL HUNGER"), TEXT("+25% energy from hits"),    35},
        {TEXT("SECOND WIND"), TEXT("Heal half your health"),    25},
        {TEXT("BLOODTHIRST"), TEXT("Heal 3 per kill"),          45},
        {TEXT("GREED"),       TEXT("+1 soul from every drop"),  40}};
    return Table[FMath::Clamp(Upgrade, 0, Count - 1)];
}
inline int32 Cost(int32 Upgrade, int32 Owned) { return FMath::RoundToInt(Info(Upgrade).BaseCost * (1.f + .4f * FMath::Max(0, Owned))); }
// Second Wind is spent at once; the rest are kept and shown as levels.
inline bool IsInstant(int32 Upgrade) { return Upgrade == static_cast<int32>(EUpgrade::SecondWind); }

// What the owned levels add up to (index by EUpgrade).
struct FStats
{
    float Damage = 1.f, DamageTaken = 1.f, Speed = 1.f, Energy = 1.f, HealPerKill = 0.f, BonusMaxHealth = 0.f;
    int32 BonusSouls = 0;
};
inline FStats Stats(const TArray<int32>& Owned)
{
    auto N = [&](EUpgrade U) { return Owned.IsValidIndex(static_cast<int32>(U)) ? Owned[static_cast<int32>(U)] : 0; };
    FStats S;
    S.Damage = 1.f + .15f * N(EUpgrade::Fury);
    S.DamageTaken = FMath::Max(.45f, FMath::Pow(.85f, static_cast<float>(N(EUpgrade::IronSkin))));
    S.Speed = FMath::Min(1.4f, 1.f + .1f * N(EUpgrade::Swiftness));
    S.Energy = 1.f + .25f * N(EUpgrade::SoulHunger);
    S.HealPerKill = 3.f * N(EUpgrade::Bloodthirst);
    S.BonusMaxHealth = 25.f * N(EUpgrade::Vitality);
    S.BonusSouls = N(EUpgrade::Greed);
    return S;
}
// Five different upgrades.
inline TArray<int32> Roll(FRandomStream& Random)
{
    TArray<int32> Pool;
    for (int32 I = 0; I < Count; ++I) Pool.Add(I);
    TArray<int32> Offers;
    while (Offers.Num() < OffersPerPortal && Pool.Num() > 0)
    {
        const int32 Pick = Random.RandHelper(Pool.Num());
        Offers.Add(Pool[Pick]);
        Pool.RemoveAtSwap(Pick);
    }
    return Offers;
}
}
