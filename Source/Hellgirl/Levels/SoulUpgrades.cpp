#include "Levels/ArenaGameMode.h"
#include "Rules/PortalUpgrades.h"
#include "Fighter/ArenaFighter.h"
#include "Progress/HellgirlWallet.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/PlatformTime.h"

// Soul portal upgrades: each blue portal rolls five offers; Hellgirl can buy any of them (each once) with carried
// souls. Prices are about one portal's worth of souls, so saving up buys several at once.
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
    if (Wallet->Carried < Price) return false;
    Wallet->Carried -= Price;
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
