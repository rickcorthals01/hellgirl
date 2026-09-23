#include "../Source/AshenArena/DefenseRules.h"
#define CHECK(C) if (!(C)) return __LINE__
int main()
{
    using namespace HellgirlDefense;
    const auto Front = Receive(20.f, 1.f, true, 0.f);
    CHECK(Front.Blocked && Front.Damage == 5.f && Front.Meter == 15.f);
    CHECK(Receive(20.f, .5f, true, 0.f).Blocked);
    CHECK(!Receive(20.f, .49f, true, 0.f).Blocked);
    CHECK(Receive(20.f, -1.f, true, 30.f).Damage == 20.f);
    CHECK(Receive(20.f, -1.f, true, 30.f).Meter == 30.f);
    CHECK(!Receive(20.f, 1.f, false, 0.f).Blocked);
    CHECK(Receive(20.f, 1.f, true, 95.f).Meter == 100.f);
    CHECK(Receive(0.f, 1.f, true, 10.f).Meter == 10.f);
    CHECK(BonusMultiplier(0.f) == 1.f && BonusMultiplier(100.f) == 2.f);
    CHECK(AfterAttack(75.f, false) == 75.f);
    CHECK(AfterAttack(75.f, true) == 0.f);
    CHECK(BonusMultiplier(AfterAttack(100.f, true)) == 1.f);
    return 0;
}
