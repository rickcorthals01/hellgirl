#pragma once
#include "CoreMinimal.h"
#include "Math/RandomStream.h"

// Soul portal upgrades: each portal offers five at random, each buyable once with the level's Souls. Prices were cut 15%
// on 2026-09-25 and another 35% on 2026-09-27, so one portal's worth of Souls (a Stage 2 portal: ~40, later ones 60-120)
// buys one or two, and saving up buys several at once. They last for the rest of the level (or endless run) and
// stack; every level owned makes the next one 40% dearer.
namespace HellgirlUpgrades
{
// Named after the seven deadly sins (2026-09-27; they were Fury, Iron Skin, Vitality, Swiftness, Soul Hunger, Second
// Wind, Bloodthirst and Greed).
enum class EUpgrade : uint8 { Wrath, Pride, Lust, Sloth, Gluttony, Resurrection, Envy, Greed, Count };
constexpr int32 Count = static_cast<int32>(EUpgrade::Count);
constexpr int32 OffersPerPortal = 5;

struct FInfo { const TCHAR* Name; const TCHAR* Detail; int32 BaseCost; };
inline const FInfo& Info(int32 Upgrade)
{
    static const FInfo Table[Count] = {
        {TEXT("WRATH"),        TEXT("+15% damage"),                         28},
        {TEXT("PRIDE"),        TEXT("-15% damage taken"),                   25},
        {TEXT("LUST"),         TEXT("+25 max health, heals half your health"), 20},
        {TEXT("SLOTH"),        TEXT("+25% move speed"),                     17},
        {TEXT("GLUTTONY"),     TEXT("+25% energy from hits"),               20},
        {TEXT("RESURRECTION"), TEXT("Heals you fully"),                     14},
        {TEXT("ENVY"),         TEXT("Heals 5% of your health per kill"),    25},
        {TEXT("GREED"),        TEXT("+1 soul from every drop"),             22}};
    return Table[FMath::Clamp(Upgrade, 0, Count - 1)];
}
inline int32 Cost(int32 Upgrade, int32 Owned) { return FMath::RoundToInt(Info(Upgrade).BaseCost * (1.f + .4f * FMath::Max(0, Owned))); }
// Resurrection is spent at once; the rest are kept and shown as levels.
inline bool IsInstant(int32 Upgrade) { return Upgrade == static_cast<int32>(EUpgrade::Resurrection); }

// What the owned levels add up to (index by EUpgrade).
struct FStats
{
    float Damage = 1.f, DamageTaken = 1.f, Speed = 1.f, Energy = 1.f, BonusMaxHealth = 0.f;
    float HealPerKill = 0.f; // a share of max health (Envy: .05 per level)
    int32 BonusSouls = 0;
};
inline FStats Stats(const TArray<int32>& Owned)
{
    auto N = [&](EUpgrade U) { return Owned.IsValidIndex(static_cast<int32>(U)) ? Owned[static_cast<int32>(U)] : 0; };
    FStats S;
    S.Damage = 1.f + .15f * N(EUpgrade::Wrath);
    S.DamageTaken = FMath::Max(.45f, FMath::Pow(.85f, static_cast<float>(N(EUpgrade::Pride))));
    S.Speed = FMath::Min(1.5f, 1.f + .25f * N(EUpgrade::Sloth)); // two levels at most
    S.Energy = 1.f + .25f * N(EUpgrade::Gluttony);
    S.HealPerKill = .05f * N(EUpgrade::Envy);
    S.BonusMaxHealth = 25.f * N(EUpgrade::Lust);
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
