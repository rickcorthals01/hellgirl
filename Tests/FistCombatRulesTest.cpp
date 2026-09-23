#include "../Source/Hellgirl/FistCombatRules.h"
#include "../Source/Hellgirl/CombatEnergyRules.h"
#define CHECK(C) if (!(C)) return __LINE__
int main()
{
    using namespace FistCombat;
    const Move Light[] = {Move::RightPunch,Move::LeftPunch,Move::RightPunch,Move::DoubleJab};
    const Move Heavy[] = {Move::RightHeavyKick,Move::LeftHeavyKick,Move::RightHeavyKick,Move::LegSweep};
    const Move Sword[] = {Move::SwordSlash,Move::SwordBackslash,Move::SwordThrust,Move::SwordSpin};
    for (int i=0;i<4;++i) {
        CHECK(Select(false,i,false,false).Type==Light[i]);
        CHECK(Select(true,i,false,false).Type==Heavy[i]);
        CHECK(Select(false,i,false,false,true).Type==Sword[i]);
        CHECK(Select(false,i,false,false).NextCombo==(i+1)%4);
        CHECK(Select(true,i,false,false).NextCombo==(i+1)%4);
    }
    CHECK(Select(false,0,false,true).Type==Move::Headbutt);
    CHECK(Select(true,0,false,true).Knockback==0.f);
    CHECK(HellgirlEnergy::ChargeCost==12.5f && HellgirlEnergy::SlamCost==12.5f);
    CHECK(HellgirlEnergy::Cost(Move::LegSweep)==12.5f);
    CHECK(HellgirlEnergy::Gain(Move::DoubleJab)==12.f);
    CHECK(HellgirlEnergy::Gain(Move::RightHeavyKick)==18.f);
    CHECK(HellgirlEnergy::Gain(Move::AirCrashKick)==0.f);
    CHECK(Charged(1).Damage>Charged(0).Damage && Charged(1).Range>Charged(0).Range);
    CHECK(AreaDamageScale(1000,300)==.55f);
    CHECK(Select(false,3,true,false).Type==Move::AirCrashKick);
    return 0;
}
