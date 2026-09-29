#pragma once
#include "CoreMinimal.h"
#include "Rules/FistCombatRules.h"

// The goblin's shop at camp (2026-09-26 design): Soul Coins buy the energy moves and permanent stat upgrades. Unlike
// the soul portal upgrades (Rules/PortalUpgrades.h) these are kept for good, in the wallet save.
// - Energy moves start locked (the ultimates do not). A locked move falls back to a basic hit: a held heavy does not
//   charge, air + heavy does nothing, the air combo's last hit is a plain air kick, dodge + heavy and the ground combos' last
//   hits are a plain heavy.
// - Stat upgrades are bought one level at a time, each at the same price, up to a cap.
// - The Infernal Sword (2026-09-30) replaces her Bat Sword with the burning one; sword hits deal 25% more damage.
namespace HellgirlShop
{
enum class EItem : uint8 { Charge, SkySlam, DodgeSlam, LegSweep, CrashKick, Whirlwind, Health, Souls, Damage, Speed, InfernalSword, Count };
constexpr int32 Count = static_cast<int32>(EItem::Count);
constexpr int32 FirstStat = static_cast<int32>(EItem::Health);

struct FInfo { const TCHAR* Name; const TCHAR* Detail; int64 Price; int32 MaxLevel; };
inline const FInfo& Info(int32 Item)
{
    static const FInfo Table[Count] = {
        {TEXT("CHARGE"),        TEXT("Hold heavy to charge a strike"),          500,   1},
        {TEXT("SKY SLAM"),      TEXT("Heavy in the air: slam down"),            500,   1},
        {TEXT("DODGE + SLAM"),  TEXT("Heavy right after a dodge"),              700,   1},
        {TEXT("LEG SWEEP"),     TEXT("The heavy combo's 4th hit"),              300,   1},
        {TEXT("CRASH KICK"),    TEXT("The air combo's 4th hit"),                300,   1},
        {TEXT("WHIRLWIND"),     TEXT("The sword combo's 4th hit"),              1500,  1},
        {TEXT("VITALITY"),      TEXT("+10 max health"),                         3000,  100},
        {TEXT("SOUL HARVEST"),  TEXT("More Souls from every enemy"),            2500,  27},
        {TEXT("STRENGTH"),      TEXT("+1% damage"),                             4000,  50},
        {TEXT("AGILITY"),       TEXT("+2% attack and movement speed"),          3000,  20},
        {TEXT("INFERNAL SWORD"),TEXT("A burning blade: sword hits +25% damage"), 5000, 1}};
    return Table[FMath::Clamp(Item, 0, Count - 1)];
}
inline bool IsMove(int32 Item) { return Item >= 0 && Item < FirstStat; }
// Bought once and then OWNED (the moves and the Infernal Sword), rather than levelled up until MAXED.
inline bool IsOneOff(int32 Item) { return IsMove(Item) || Item == static_cast<int32>(EItem::InfernalSword); }

// The move a shop item unlocks, or -1 for a move that is always free.
inline int32 ItemFor(FistCombat::Move Type)
{
    using FistCombat::Move;
    switch (Type)
    {
    case Move::ChargedStrike: return static_cast<int32>(EItem::Charge);
    case Move::AirSlam: return static_cast<int32>(EItem::SkySlam);
    case Move::DodgeSlam: return static_cast<int32>(EItem::DodgeSlam);
    case Move::LegSweep: return static_cast<int32>(EItem::LegSweep);
    case Move::AirCrashKick: return static_cast<int32>(EItem::CrashKick);
    case Move::SwordSpin: return static_cast<int32>(EItem::Whirlwind);
    default: return -1;
    }
}

// Soul Harvest: +2.5% for each of the first two levels, then +1% each, up to +30% (27 levels).
inline float SoulPercent(int32 Level)
{
    Level = FMath::Clamp(Level, 0, Info(static_cast<int32>(EItem::Souls)).MaxLevel);
    return Level <= 2 ? 2.5f * Level : FMath::Min(30.f, 5.f + (Level - 2));
}

struct FStats
{
    float BonusHealth = 0.f, SoulBonus = 0.f, Damage = 1.f, Speed = 1.f; // SoulBonus: .05 = 5% more Souls
    bool bInfernalSword = false; // the Bat Sword becomes the Infernal Sword, whose hits deal SwordDamage
    float SwordDamage = 1.f;
};
inline FStats Stats(const TArray<int32>& Levels)
{
    auto L = [&](EItem Item)
    {
        const int32 I = static_cast<int32>(Item);
        return Levels.IsValidIndex(I) ? FMath::Clamp(Levels[I], 0, Info(I).MaxLevel) : 0;
    };
    FStats S;
    S.BonusHealth = 10.f * L(EItem::Health);
    S.SoulBonus = SoulPercent(L(EItem::Souls)) / 100.f;
    S.Damage = 1.f + .01f * L(EItem::Damage);
    S.Speed = 1.f + .02f * L(EItem::Speed);
    S.bInfernalSword = L(EItem::InfernalSword) > 0;
    S.SwordDamage = S.bInfernalSword ? 1.25f : 1.f;
    return S;
}

// A drop of Amount Souls with the harvest bonus: the fraction is a chance of one more (3 Souls at +10% is 3.3 on average).
inline int32 HarvestDrop(int32 Amount, float SoulBonus, float Roll01)
{
    const float Boosted = Amount * (1.f + SoulBonus);
    const int32 Whole = FMath::FloorToInt(Boosted);
    return Whole + (Roll01 < Boosted - Whole ? 1 : 0);
}
}
