#include "../Source/AshenArena/StageOneLayout.h"
#include "../Source/AshenArena/CoinPickupRules.h"
#include "../Source/AshenArena/FistCombatRules.h"
#define CHECK(C) if (!(C)) return __LINE__
int main()
{
    using namespace StageOne;
    CHECK(!ShouldActivate(false, true, 2001.f * 2001.f, 2000.f));
    CHECK(ShouldActivate(false, true, 2000.f * 2000.f, 2000.f));
    CHECK(!ShouldActivate(true, true, 0.f, 2000.f));
    CHECK(!ShouldActivate(false, false, 0.f, 2000.f));
    CHECK(Height(3000.f, 4500.f) > Height(0.f, 0.f) + 700.f);
    for (Point Site : Sites)
    {
        CHECK(std::fabs(Site.X) < HalfExtent && std::fabs(Site.Y) < HalfExtent);
        const float DX = Site.X - PlayerStart.X, DY = Site.Y - PlayerStart.Y;
        CHECK(!ShouldActivate(false, true, DX * DX + DY * DY, 2000.f));
    }
    for (Point P : Road)
    {
        CHECK(IsRoad(P.X, P.Y));
        const float DX = (Height(P.X + 10.f, P.Y) - Height(P.X - 10.f, P.Y)) / 20.f;
        const float DY = (Height(P.X, P.Y + 10.f) - Height(P.X, P.Y - 10.f)) / 20.f;
        CHECK(DX * DX + DY * DY < 1.f); // Walkable, below a 45-degree grade.
    }
    using HellgirlCoins::TouchesCapsule;
    CHECK(!TouchesCapsule(150.f, 0.f, 0.f, 38.f, 88.f)); // Nearby is not collected.
    CHECK(TouchesCapsule(54.f, 0.f, 0.f, 38.f, 88.f)); // Coin edge touches side.
    CHECK(!TouchesCapsule(55.f, 0.f, 0.f, 38.f, 88.f));
    CHECK(TouchesCapsule(0.f, 0.f, 104.f, 38.f, 88.f)); // Top contact.
    CHECK(!TouchesCapsule(0.f, 0.f, 105.f, 38.f, 88.f));
    CHECK(TouchesCapsule(0.f, 0.f, -104.f, 38.f, 88.f)); // Bottom contact.
    const auto Punch = FistCombat::Select(false, 0, true, false);
    const auto Kick = FistCombat::Select(false, 2, true, false);
    const auto Finish = FistCombat::Select(false, 3, true, false);
    CHECK(Punch.Damage == 12.f && Kick.Damage == 18.f && Finish.Damage == 28.f);
    CHECK(Finish.Range == 360.f);
    return 0;
}
