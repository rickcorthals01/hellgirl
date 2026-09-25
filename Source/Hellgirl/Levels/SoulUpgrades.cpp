#include "Levels/ArenaGameMode.h"
#include "Rules/PortalUpgrades.h"
#include "Fighter/ArenaFighter.h"
#include "Progress/HellgirlWallet.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/PlatformTime.h"
#include "EngineUtils.h"
#include "Engine/World.h"

// Soul portal upgrades: each blue portal rolls five offers; Hellgirl can buy any of them (each once) with the
// level's Souls. Prices are a little under one portal's worth of Souls, so saving up buys several at once.
// What she buys lasts for the rest of the level (a new level starts with none).

void AArenaGameMode::RollPortalOffers()
{
    FRandomStream Random(static_cast<int32>(FPlatformTime::Cycles()));
    PortalOffers = HellgirlUpgrades::Roll(Random);
    OfferSold.Init(false, PortalOffers.Num());
}

int32 AArenaGameMode::GetUpgradeCost(int32 Upgrade) const
{
    return HellgirlUpgrades::Cost(Upgrade, GetUpgradeLevel(Upgrade));
}

void AArenaGameMode::ApplyUpgrades()
{
    if (auto* Hero = Cast<AArenaFighter>(UGameplayStatics::GetPlayerPawn(this, 0))) Hero->Upgrades = HellgirlUpgrades::Stats(UpgradeLevels);
}

bool AArenaGameMode::BuyUpgrade(int32 Offer)
{
    auto* Hero = Cast<AArenaFighter>(UGameplayStatics::GetPlayerPawn(this, 0));
    auto* Wallet = Cast<UHellgirlWallet>(GetGameInstance());
    // Each offer once per portal, paid from the souls she is carrying.
    if (!Hero || !Wallet || !IsSoulPortalOpen() || !PortalOffers.IsValidIndex(Offer) || IsOfferSold(Offer)) return false;
    const int32 Upgrade = PortalOffers[Offer];
    const int32 Price = GetUpgradeCost(Upgrade);
    if (Wallet->LevelSouls.Souls < Price) return false;
    Wallet->LevelSouls.Souls -= Price; // spending does not lower what the level deposits when won
    OfferSold[Offer] = true;
    if (UpgradeLevels.Num() != HellgirlUpgrades::Count) UpgradeLevels.Init(0, HellgirlUpgrades::Count);
    using HellgirlUpgrades::EUpgrade;
    if (Upgrade == static_cast<int32>(EUpgrade::SecondWind)) Hero->Health = FMath::Min(Hero->MaxHealth, Hero->Health + Hero->MaxHealth * .5f);
    else ++UpgradeLevels[Upgrade];
    if (Upgrade == static_cast<int32>(EUpgrade::Vitality)) { Hero->MaxHealth += 25.f; Hero->Health = FMath::Min(Hero->MaxHealth, Hero->Health + 25.f); }
    ApplyUpgrades();
    Hero->MoveLabel = FString(HellgirlUpgrades::Info(Upgrade).Name) + TEXT(" / ") + HellgirlUpgrades::Info(Upgrade).Detail;
    return true;
}

void AArenaGameMode::RestoreUpgrades(const TArray<int32>& Levels)
{
    UpgradeLevels.Init(0, HellgirlUpgrades::Count);
    for (int32 I = 0; I < Levels.Num() && I < HellgirlUpgrades::Count; ++I) UpgradeLevels[I] = FMath::Max(0, Levels[I]);
    ApplyUpgrades();
    if (auto* Hero = Cast<AArenaFighter>(UGameplayStatics::GetPlayerPawn(this, 0)))
        Hero->MaxHealth += HellgirlUpgrades::Stats(UpgradeLevels).BonusMaxHealth;
}

// Bonus measures for Soul Coins (Rules/SoulRewards.h): unpaused time until the level is won, time with enemies
// alive, and how much of that Hellgirl kept a combo multiplier going. (Energy spent is noted by the fighter.)
void AArenaGameMode::TrackLevelSouls(float Dt)
{
    auto* Hero = Cast<AArenaFighter>(UGameplayStatics::GetPlayerPawn(this, 0));
    auto* Wallet = Cast<UHellgirlWallet>(GetGameInstance());
    if (bForestHub || bLevelCompleted || !Hero || !Hero->IsAlive() || !Wallet || !Wallet->bInLevel) return;
    FHellgirlLevelSouls& Level = Wallet->LevelSouls;
    Level.Time += Dt;
    bool bFighting = false;
    for (TActorIterator<AArenaFighter> It(GetWorld()); It && !bFighting; ++It) bFighting = It->bEnemy && It->IsAlive();
    if (!bFighting) return;
    Level.CombatTime += Dt;
    if (Hero->GetComboMeter().Tier() > 0) Level.ComboTime += Dt;
}
