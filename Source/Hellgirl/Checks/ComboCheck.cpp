// -HellgirlComboCheck: the style combo meter's rules, then a real punch at 1.5x.
#include "Fighter/ArenaFighter.h"
#include "Rules/ComboRules.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

void AArenaFighter::RunComboCheck()
{
#if WITH_DEV_AUTOMATION_TESTS
    if (!FParse::Param(FCommandLine::Get(), TEXT("HellgirlComboCheck"))) return;
    static bool Done = false;
    if (Done || GetWorld()->GetTimeSeconds() < 1.f) return;
    Done = true;
    using namespace HellgirlCombo;
    using M = FistCombat::Move;
    FString Failure;
    auto Check = [&](bool Ok, const TCHAR* What) { if (!Ok && Failure.IsEmpty()) Failure = What; return Ok; };

    // 1. Varied moves build the meter; the same move family over and over barely does.
    FMeter Varied;
    const M Cycle[] = {M::RightPunch, M::RightHeavyKick, M::AirPunch, M::AirSlam, M::Headbutt, M::ChargedStrike};
    for (int32 I = 0; I < 3; ++I) Varied.Landed(Cycle[I]);
    Check(Varied.Tier() == 1 && FMath::IsNearlyEqual(Varied.Multiplier(), 1.1f), TEXT("three different moves reach 1.1x"));
    FMeter Spam;
    for (int32 I = 0; I < 12; ++I) Spam.Landed(M::RightPunch);
    Check(Spam.Tier() == 0 && Spam.IsSpamming(), TEXT("twelve light punches in a row stay below 1.1x"));
    FMeter Full;
    for (int32 I = 0; I < 60; ++I) Full.Landed(Cycle[I % 6]);
    Check(Full.Tier() == MaxTier && FMath::IsNearlyEqual(Full.Multiplier(), 2.f) && Full.Fill() == 1.f, TEXT("sustained variety caps at 2.0x"));
    int32 HitsToMax = 0;
    for (FMeter Count; Count.Tier() < MaxTier; ++HitsToMax) Count.Landed(Cycle[HitsToMax % 6]);
    Check(HitsToMax >= 20 && HitsToMax <= 35, TEXT("2.0x takes one good fight (20-35 varied hits)"));
    // 2. Getting hit costs two tiers; idling drains after the grace period.
    FMeter Hurt = Full;
    Hurt.Hurt();
    Check(Hurt.Tier() == MaxTier - 2, TEXT("taking a hit drops two tiers"));
    FMeter Idle = Varied;
    Idle.Tick(GraceSeconds - .1f);
    Check(Idle.Tier() == 1, TEXT("no drain inside the grace period"));
    for (int32 I = 0; I < 60; ++I) Idle.Tick(.1f);
    Check(Idle.Tier() == 0 && Idle.Points == 0.f, TEXT("idling drains the meter"));

    // 3. In the world: at 1.5x a right punch deals 1.5 times its damage and feeds the meter.
    const FVector Front = GetActorLocation() + GetActorForwardVector() * 120.f;
    auto* Target = GetWorld()->SpawnActor<AArenaFighter>(Front, (GetActorLocation() - Front).Rotation());
    if (Check(Target != nullptr, TEXT("target spawned")))
    {
        Target->MakeEnemy(1, false);
        Target->Health = Target->MaxHealth = 1000.f;
        ComboMeter = FMeter();
        ComboMeter.Points = 5.f * PointsPerTier + 10.f;
        Riposte = 0.f;
        CurrentAttack = FistCombat::Select(false, 0, false, false);
        bComboCredited = false;
        const float Expected = 1000.f - CurrentAttack.Damage * 1.5f;
        ResolveAttack();
        Check(FMath::IsNearlyEqual(Target->Health, Expected, .01f), TEXT("a punch at 1.5x deals 1.5 times its damage"));
        Check(ComboMeter.Points > 5.f * PointsPerTier + 10.f && bComboCredited, TEXT("the landed punch feeds the meter once"));
        Target->Destroy();
    }

    if (Failure.IsEmpty()) { UE_LOG(LogTemp, Display, TEXT("COMBO CHECK PASSED: variety to 1.1x, spam stays low, 2.0x cap after %d varied hits, hurt and drain, 1.5x punch damage"), HitsToMax); }
    else { UE_LOG(LogTemp, Error, TEXT("COMBO CHECK FAILED: %s"), *Failure); }
    FPlatformMisc::RequestExitWithStatus(false, Failure.IsEmpty() ? 0 : 1);
#endif
}
