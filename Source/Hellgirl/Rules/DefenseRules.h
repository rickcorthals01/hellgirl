#pragma once
namespace HellgirlDefense
{
inline float ClampMeter(float Meter) { return Meter < 0.f ? 0.f : (Meter > 100.f ? 100.f : Meter); }
struct Hit { float Damage; float Meter; bool Blocked; };
inline Hit Receive(float Damage, float FacingDot, bool Guarding, float Meter)
{
    if (Damage <= 0.f) return {0.f, ClampMeter(Meter), false};
    if (!Guarding || FacingDot < .5f) return {Damage, ClampMeter(Meter), false};
    return {Damage * .25f, ClampMeter(Meter + Damage * .75f), true};
}
inline float BonusMultiplier(float Meter) { return 1.f + ClampMeter(Meter) / 100.f; }
inline float AfterAttack(float Meter, bool Landed) { return Landed ? 0.f : ClampMeter(Meter); }
}
