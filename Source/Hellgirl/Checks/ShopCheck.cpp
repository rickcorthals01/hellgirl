#include "Levels/ArenaGameMode.h"
#include "Fighter/ArenaFighter.h"
#include "Progress/HellgirlWallet.h"
#include "Rules/ShopUpgrades.h"
#include "Rules/CombatEnergyRules.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

// -HellgirlShopCheck (in the swamp map, where attacks are on): the goblin's shop (Rules/ShopUpgrades.h).
// - Prices, caps and Soul Harvest's steps (2.5%, 2.5%, then 1% up to 30%); a harvested drop rounds by chance.
// - Buying takes the price, stops at the cap and needs the coins (in memory only: checks never write the wallet).
// - With nothing bought each energy move falls back to a basic hit (air + heavy does nothing) and a held heavy does
//   not charge; once bought,
//   each comes out as itself.
// - Stat levels give Hellgirl more health, damage and speed when she spawns; enemies never get them.
struct FShopCheck
{
    static void Reset(AArenaFighter* Hero)
    {
        Hero->AttackClock = Hero->DodgeClock = Hero->BufferClock = Hero->ComboClock = Hero->PostDodgeClock = Hero->PendingCharge = Hero->ChargeClock = 0.f;
        Hero->Combo = Hero->AirCombo = 0;
        Hero->bAirFinisherUsed = Hero->bHeavyHeld = false;
        Hero->SelectedWeapon = 0;
        Hero->Energy = Hero->MaxEnergy;
        Hero->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    }
    // Sets up one way into an energy move, swings, and says which move came out.
    static FistCombat::Move Swing(AArenaFighter* Hero, FistCombat::Move Want)
    {
        using FistCombat::Move;
        Reset(Hero);
        bool Heavy = true;
        switch (Want)
        {
        case Move::ChargedStrike: Hero->PendingCharge = 1.f; break;
        case Move::DodgeSlam: Hero->PostDodgeClock = .3f; break;
        case Move::LegSweep: Hero->ComboClock = 1.f; Hero->Combo = 3; Hero->bLastComboHeavy = true; break;
        case Move::SwordSpin: Hero->SelectedWeapon = 1; Hero->ComboClock = 1.f; Hero->Combo = 3; Hero->bLastComboHeavy = false; Heavy = false; break;
        case Move::AirSlam: Hero->GetCharacterMovement()->SetMovementMode(MOVE_Falling); break;
        case Move::AirCrashKick: Hero->GetCharacterMovement()->SetMovementMode(MOVE_Falling); Hero->AirCombo = 3; Heavy = false; break;
        default: break;
        }
        // A swing that never started leaves the (enemy-only) claw behind.
        Hero->CurrentAttack.Type = Move::EnemyClaw;
        Hero->StartAttack(Heavy);
        const Move Got = Hero->CurrentAttack.Type;
        Reset(Hero);
        return Got;
    }
    // A held heavy for half a second: does it charge?
    static bool Charges(AArenaFighter* Hero)
    {
        Reset(Hero);
        Hero->HeavyAttack();
        Hero->Tick(.5f);
        const bool Charged = Hero->ChargeClock > 0.f;
        Hero->CancelCharge();
        Reset(Hero);
        return Charged;
    }
    static FString Run(AArenaGameMode* GM, AArenaFighter* Hero)
    {
        using namespace HellgirlShop;
        auto* Wallet = Cast<UHellgirlWallet>(Hero->GetGameInstance());
        if (!Wallet) return TEXT("no wallet");
        // The price list and the steps.
        const int64 Prices[Count] = {500, 500, 700, 300, 300, 1500, 3000, 2500, 4000, 3000, 5000};
        const int32 Caps[Count] = {1, 1, 1, 1, 1, 1, 100, 27, 50, 20, 1};
        for (int32 I = 0; I < Count; ++I)
            if (Info(I).Price != Prices[I] || Info(I).MaxLevel != Caps[I]) return FString::Printf(TEXT("%s has the wrong price or cap"), Info(I).Name);
        const float Steps[] = {0.f, 2.5f, 5.f, 6.f, 7.f};
        for (int32 L = 0; L < UE_ARRAY_COUNT(Steps); ++L) if (!FMath::IsNearlyEqual(SoulPercent(L), Steps[L])) return FString::Printf(TEXT("Soul Harvest level %d is %g%%"), L, SoulPercent(L));
        if (!FMath::IsNearlyEqual(SoulPercent(27), 30.f) || !FMath::IsNearlyEqual(SoulPercent(40), 30.f)) return TEXT("Soul Harvest does not stop at 30%");
        if (HarvestDrop(3, .1f, .1f) != 4 || HarvestDrop(3, .1f, .5f) != 3 || HarvestDrop(2, 0.f, 0.f) != 2) return TEXT("a harvested drop rounds wrongly");
        TArray<int32> Levels; Levels.SetNumZeroed(Count);
        Levels[static_cast<int32>(EItem::Health)] = 100; Levels[static_cast<int32>(EItem::Damage)] = 50; Levels[static_cast<int32>(EItem::Speed)] = 20;
        const FStats Top = Stats(Levels);
        if (!FMath::IsNearlyEqual(Top.BonusHealth, 1000.f) || !FMath::IsNearlyEqual(Top.Damage, 1.5f) || !FMath::IsNearlyEqual(Top.Speed, 1.4f)) return TEXT("the capped stats are wrong");
        if (Top.bInfernalSword || !FMath::IsNearlyEqual(Top.SwordDamage, 1.f)) return TEXT("the Infernal Sword without buying it");
        Levels[static_cast<int32>(EItem::InfernalSword)] = 1;
        if (!Stats(Levels).bInfernalSword || !FMath::IsNearlyEqual(Stats(Levels).SwordDamage, 1.25f)) return TEXT("the Infernal Sword does not add 25% sword damage");

        const int64 CoinsKept = Wallet->Coins;
        const TArray<int32> LevelsKept = Wallet->ShopLevels;
        const bool bInChecksKept = Wallet->bShopInChecks;
        ON_SCOPE_EXIT { Wallet->Coins = CoinsKept; Wallet->ShopLevels = LevelsKept; Wallet->bShopInChecks = bInChecksKept; };
        Wallet->bShopInChecks = true;
        Wallet->ShopLevels.Reset();
        // Buying.
        const int32 Charge = static_cast<int32>(EItem::Charge), Speed = static_cast<int32>(EItem::Speed), Health = static_cast<int32>(EItem::Health);
        Wallet->Coins = 10000;
        if (!Wallet->BuyShopItem(Charge) || Wallet->Coins != 9500 || Wallet->ShopLevel(Charge) != 1) return TEXT("buying Charge");
        if (Wallet->BuyShopItem(Charge) || Wallet->Coins != 9500) return TEXT("Charge could be bought twice");
        Wallet->ShopLevels[Speed] = 19;
        if (!Wallet->BuyShopItem(Speed) || Wallet->ShopLevel(Speed) != 20 || Wallet->CanBuyShopItem(Speed)) return TEXT("Agility does not stop at 20 levels");
        Wallet->Coins = 100;
        if (Wallet->CanBuyShopItem(Health)) return TEXT("an upgrade was for sale without the coins");

        // Locked moves.
        using FistCombat::Move;
        const Move EnergyMoves[] = {Move::ChargedStrike, Move::DodgeSlam, Move::LegSweep, Move::SwordSpin, Move::AirSlam, Move::AirCrashKick};
        Wallet->ShopLevels.Reset();
        if (!Wallet->OwnsMove(Move::RightPunch)) return TEXT("a free move counts as locked");
        for (const Move M : EnergyMoves)
            // A locked Sky Slam does nothing at all; the others fall back to a basic hit.
            if (const Move Got = Swing(Hero, M); M == Move::AirSlam ? Got != Move::EnemyClaw : (Got == M || Got == Move::EnemyClaw)) return FString::Printf(TEXT("locked move %d did not fall back to a basic hit"), static_cast<int32>(M));
        if (Charges(Hero)) return TEXT("a held heavy charged without Charge");
        // Bought moves.
        Wallet->ShopLevels.SetNumZeroed(Count);
        for (int32 I = 0; I < FirstStat; ++I) Wallet->ShopLevels[I] = 1;
        for (const Move M : EnergyMoves)
            if (Swing(Hero, M) != M) return FString::Printf(TEXT("bought move %d did not come out"), static_cast<int32>(M));
        if (!Charges(Hero)) return TEXT("a held heavy did not charge with Charge bought");

        // Stats on a freshly spawned Hellgirl, never on an enemy.
        Wallet->ShopLevels[Health] = 5; Wallet->ShopLevels[static_cast<int32>(EItem::Damage)] = 10; Wallet->ShopLevels[Speed] = 5;
        Wallet->ShopLevels[static_cast<int32>(EItem::Souls)] = 3;
        auto* Fresh = GM->GetWorld()->SpawnActor<AArenaFighter>(FVector(-5000.f, 800.f, 200.f), FRotator::ZeroRotator);
        if (!Fresh) return TEXT("spawning a fresh Hellgirl");
        const bool StatsOk = FMath::IsNearlyEqual(Fresh->MaxHealth, 150.f) && FMath::IsNearlyEqual(Fresh->Health, 150.f)
            && FMath::IsNearlyEqual(Fresh->Shop.Damage, 1.1f) && FMath::IsNearlyEqual(Fresh->Shop.Speed, 1.1f) && FMath::IsNearlyEqual(Fresh->Shop.SoulBonus, .06f);
        Fresh->MakeEnemy(1, false);
        const bool EnemyClear = FMath::IsNearlyEqual(Fresh->Shop.Damage, 1.f) && FMath::IsNearlyEqual(Fresh->Shop.Speed, 1.f) && Fresh->Shop.SoulBonus == 0.f;
        Fresh->Destroy();
        if (!StatsOk) return TEXT("a fresh Hellgirl did not get her stat upgrades");
        if (!EnemyClear) return TEXT("an enemy kept the shop's upgrades");
        return FString();
    }
};

void AArenaGameMode::RunShopCheck(float Dt)
{
#if WITH_DEV_AUTOMATION_TESTS
    if (!FParse::Param(FCommandLine::Get(), TEXT("HellgirlShopCheck"))) return;
    static float Clock = 0.f;
    static bool Done = false;
    Clock += Dt;
    auto* Hero = Cast<AArenaFighter>(UGameplayStatics::GetPlayerPawn(this, 0));
    if (Done || Clock < 1.f || !Hero) return;
    Done = true;
    const FString Why = FShopCheck::Run(this, Hero);
    if (Why.IsEmpty()) { UE_LOG(LogTemp, Display, TEXT("SHOP CHECK PASSED: prices, caps, buying, locked and bought energy moves, stat upgrades on Hellgirl only")); }
    else { UE_LOG(LogTemp, Error, TEXT("SHOP CHECK FAILED: %s"), *Why); }
    FPlatformMisc::RequestExitWithStatus(false, Why.IsEmpty() ? 0 : 1);
#endif
}
