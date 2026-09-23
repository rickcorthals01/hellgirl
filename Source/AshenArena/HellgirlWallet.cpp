#include "HellgirlWallet.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/PlatformTime.h"
#include "HellgirlPlayerController.h"
#include "Misc/ConfigCacheIni.h"

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
    if (bLoadFailed || Amount <= 0 || Coins > MAX_int64 - Amount) return false;
    Coins += Amount;
    LastPickup = Amount;
    PickupTime = FPlatformTime::Seconds();
    SaveWallet();
    return true;
}

bool UHellgirlWallet::BuyGoblinQueen()
{
    if (bLoadFailed || bGoblinQueenOwned || Coins < 200) return false;
    Coins -= 200; bGoblinQueenOwned = true;
    if (SaveWallet()) return true;
    Coins += 200; bGoblinQueenOwned = false;
    return false;
}

bool UHellgirlWallet::StartNewGame()
{
    // Write the fresh wallet first. A failed write leaves current progress intact.
    auto* Fresh=Cast<UHellgirlWalletSave>(UGameplayStatics::CreateSaveGameObject(UHellgirlWalletSave::StaticClass()));
    if (!Fresh || !UGameplayStatics::SaveGameToSlot(Fresh,WalletSlot,0)) return false;
    Coins=0; bGoblinQueenOwned=false; bSaveFailed=bLoadFailed=false;
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
