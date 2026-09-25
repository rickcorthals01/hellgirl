#include "Progress/HellgirlWallet.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/PlatformTime.h"
#include "UI/HellgirlPlayerController.h"
#include "Misc/ConfigCacheIni.h"
#include "Progress/CampaignProgress.h"
#include "Progress/Achievements.h"

namespace { const FString WalletSlot = TEXT("HellgirlWallet_v1"); }

void UHellgirlWallet::Init()
{
    Super::Init();
    if (UGameplayStatics::DoesSaveGameExist(WalletSlot, 0))
    {
        const UHellgirlWalletSave* Save = Cast<UHellgirlWalletSave>(UGameplayStatics::LoadGameFromSlot(WalletSlot, 0));
        bLoadFailed = !Save || Save->Coins < 0;
        if (!bLoadFailed) { Coins = Save->Coins; bGoblinQueenOwned = Save->bGoblinQueenOwned; }
    }
}

bool UHellgirlWallet::Collect(int32 Amount)
{
    // Do not overwrite an unreadable save or consume a pickup that cannot be stored.
    if (bLoadFailed || Amount <= 0 || LevelSouls.Earned > MAX_int64 / 2 - Amount) return false;
    LastPickup = Amount;
    PickupTime = FPlatformTime::Seconds();
    if (bInLevel) { LevelSouls.Souls += Amount; LevelSouls.Earned += Amount; return true; }
    Coins += Amount;
    SaveWallet();
    return true;
}

void UHellgirlWallet::ResetLevel()
{
    LevelSouls = FHellgirlLevelSouls();
}

HellgirlSouls::FReward UHellgirlWallet::FinishLevel(bool bDeposit)
{
    LastReward = HellgirlSouls::Compute(LevelSouls.Earned, LevelSouls.Stocked, LevelSouls.Time, LevelSouls.Kills, LevelSouls.ComboTime, LevelSouls.CombatTime, LevelSouls.EnergySpent);
    RewardTime = FPlatformTime::Seconds();
    if (bDeposit && !bLoadFailed && LastReward.Total > 0)
    {
        Coins += LastReward.Total;
        if (!SaveWallet()) Coins -= LastReward.Total;
    }
    ResetLevel();
    return LastReward;
}

void UHellgirlWallet::LoseLevel()
{
    if (LevelSouls.Earned > 0) { LastLost = LevelSouls.Earned; LostTime = FPlatformTime::Seconds(); }
    ResetLevel();
}

bool UHellgirlWallet::CanBuyGoblinQueen() const
{
    return !bLoadFailed && !bGoblinQueenOwned && Coins >= GoblinQueenPrice && HellgirlAchievements::Has(HellgirlAchievements::EndlessGoblins50);
}

bool UHellgirlWallet::BuyGoblinQueen()
{
    if (!CanBuyGoblinQueen()) return false;
    Coins -= GoblinQueenPrice; bGoblinQueenOwned = true;
    // Automated checks buy in memory only.
    if (HellgirlProgress::IsAutomated() || SaveWallet()) { HellgirlAchievements::Unlock(HellgirlAchievements::GoblinQueenOutfit); return true; }
    Coins += GoblinQueenPrice; bGoblinQueenOwned = false;
    return false;
}

bool UHellgirlWallet::StartNewGame()
{
    // Write the fresh wallet first. A failed write leaves current progress intact.
    auto* Fresh=Cast<UHellgirlWalletSave>(UGameplayStatics::CreateSaveGameObject(UHellgirlWalletSave::StaticClass()));
    if (!Fresh || !UGameplayStatics::SaveGameToSlot(Fresh,WalletSlot,0)) return false;
    Coins=0; ResetLevel(); bGoblinQueenOwned=false; bSaveFailed=bLoadFailed=false;
    for (const FString& Flag : HellgirlProgress::AllFlags()) GConfig->SetBool(HellgirlProgress::Section,*Flag,false,GGameUserSettingsIni);
    GConfig->SetInt(HellgirlProgress::Section,TEXT("EndlessGoblinsBest"),0,GGameUserSettingsIni);
    LastPickup=0; PickupTime=-10.0; PendingLoad=nullptr;
    GConfig->SetInt(TEXT("HellgirlCampaign"),TEXT("UnlockedLevel"),1,GGameUserSettingsIni);
    GConfig->SetInt(TEXT("HellgirlCampaign"),TEXT("ProgressVersion"),2,GGameUserSettingsIni);
    GConfig->SetInt(TEXT("HellgirlAppearance"),TEXT("Outfit"),0,GGameUserSettingsIni);
    GConfig->Flush(false,GGameUserSettingsIni);
    if (auto* PC=Cast<AHellgirlPlayerController>(UGameplayStatics::GetPlayerController(this,0))) PC->ResumeGame();
    UGameplayStatics::OpenLevel(this,TEXT("/Engine/Maps/Entry"),true,TEXT("StageMap=1?CampaignLevel=1?ForestHub=0"));
    return true;
}

bool UHellgirlWallet::SaveWallet()
{
    if (bLoadFailed) return false;
    UHellgirlWalletSave* Save = Cast<UHellgirlWalletSave>(UGameplayStatics::CreateSaveGameObject(UHellgirlWalletSave::StaticClass()));
    if (!Save) { bSaveFailed = true; return false; }
    Save->Coins = Coins;
    Save->bGoblinQueenOwned = bGoblinQueenOwned;
    bSaveFailed = !UGameplayStatics::SaveGameToSlot(Save, WalletSlot, 0);
    return !bSaveFailed;
}

void UHellgirlWallet::Shutdown()
{
    if (bSaveFailed) SaveWallet();
    Super::Shutdown();
}

bool UHellgirlWallet::StockSouls(bool bWrite)
{
    const int64 Amount = LevelSouls.Souls;
    if (bLoadFailed || Amount <= 0) return false;
    Coins += Amount;
    if (bWrite && !SaveWallet()) { Coins -= Amount; return false; }
    LevelSouls.Souls = 0;
    LevelSouls.Stocked += Amount;
    LastStocked = Amount; StockTime = FPlatformTime::Seconds();
    return true;
}
