#pragma once
#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "GameFramework/SaveGame.h"
#include "HellgirlWallet.generated.h"

UCLASS()
class HELLGIRL_API UHellgirlWalletSave : public USaveGame
{
    GENERATED_BODY()
public:
    UPROPERTY(SaveGame) int64 Coins = 0;
    UPROPERTY(SaveGame) bool bGoblinQueenOwned = false;
};

UCLASS()
class HELLGIRL_API UHellgirlGameSave : public UHellgirlWalletSave
{
    GENERATED_BODY()
public:
    UPROPERTY(SaveGame) int32 Version=2;
    UPROPERTY(SaveGame) int32 ImpArenaLayoutVersion=0;
    UPROPERTY(SaveGame) int32 ForestHubLayoutVersion=0;
    UPROPERTY(SaveGame) int32 Level=1;
    UPROPERTY(SaveGame) int32 Unlocked=1;
    UPROPERTY(SaveGame) int32 Outfit=0;
    UPROPERTY(SaveGame) bool bHub=false;
    UPROPERTY(SaveGame) FVector Location=FVector::ZeroVector;
    UPROPERTY(SaveGame) FRotator Facing=FRotator::ZeroRotator;
    UPROPERTY(SaveGame) FRotator Camera=FRotator::ZeroRotator;
    UPROPERTY(SaveGame) float Health=100.f;
    UPROPERTY(SaveGame) float Energy=0.f;
    UPROPERTY(SaveGame) float Stamina=100.f;
    UPROPERTY(SaveGame) TArray<bool> Cleared;
    UPROPERTY(SaveGame) TArray<FName> Story;
    UPROPERTY(SaveGame) FString Date;
    UPROPERTY(SaveGame) TArray<FVector> PickupLocations;
    UPROPERTY(SaveGame) TArray<int32> PickupAmounts;
    // Souls picked up in the level but not yet banked, and the story unlocks (Progress/CampaignProgress.h).
    UPROPERTY(SaveGame) int64 Carried=0;
    UPROPERTY(SaveGame) TArray<FString> Flags;
    UPROPERTY(SaveGame) TArray<int32> Upgrades; // soul portal upgrade levels bought in this level
};

UCLASS()
class HELLGIRL_API UHellgirlWallet : public UGameInstance
{
    GENERATED_BODY()
public:
    virtual void Init() override;
    virtual void Shutdown() override;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Wallet") int64 Coins = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Wallet") bool bSaveFailed = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Wallet") bool bLoadFailed = false;
    int32 LastPickup = 0;
    double PickupTime = -10.0;
    bool Collect(int32 Amount);
    // Souls picked up in a level are carried: banked when the level is won or stocked at a portal, all lost on death.
    // Outside a level (camp) pickups go straight to the bank.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Wallet") int64 Carried = 0;
    bool bCarrying = false;
    int64 LastLost = 0, LastBanked = 0;
    double LostTime = -10.0, BankedTime = -10.0;
    bool BankCarried();
    void ForfeitCarried();
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Wallet") bool bGoblinQueenOwned = false;
    bool BuyGoblinQueen();
    bool StartNewGame();
    FString SaveSlot(int32 Slot);
    bool LoadSlot(int32 Slot);
    FString DescribeSlot(int32 Slot) const;
    void RestorePending();
    bool RunSaveCheck();
    UPROPERTY() TObjectPtr<UHellgirlGameSave> PendingLoad;
private:
    bool SaveWallet();
};
