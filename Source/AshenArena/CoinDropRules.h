#pragma once
namespace HellgirlCoins
{
// Independent uniform rolls: tier 0..99 and amount 0..2.
inline int DropAmount(int TierRoll, int AmountRoll)
{
    return (TierRoll < 5 ? 4 : 1) + AmountRoll;
}
}
