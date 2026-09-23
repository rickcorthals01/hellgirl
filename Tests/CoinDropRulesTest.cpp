#include "../Source/AshenArena/CoinDropRules.h"
int main()
{
    int Counts[7] = {};
    for (int Tier = 0; Tier < 100; ++Tier)
        for (int Amount = 0; Amount < 3; ++Amount)
        {
            const int Coins = HellgirlCoins::DropAmount(Tier, Amount);
            if (Coins < 1 || Coins > 6) return 1;
            if ((Coins >= 4) != (Tier < 5)) return 2;
            ++Counts[Coins];
        }
    // Of all 300 equally likely input pairs, exactly 15 (5%) yield rare drops.
    for (int Coins = 1; Coins <= 3; ++Coins) if (Counts[Coins] != 95) return 3;
    for (int Coins = 4; Coins <= 6; ++Coins) if (Counts[Coins] != 5) return 4;
    return 0;
}
