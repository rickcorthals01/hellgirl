#pragma once
#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "GameFramework/SaveGame.h"
#include "Rules/SoulRewards.h"
#include "HellgirlWallet.generated.h"

// One level's Souls (Rules/SoulRewards.h): what is left to spend, everything picked up, and the bonus measures.
USTRUCT()
struct FHellgirlLevelSouls
{
    GENERATED_BODY()
    UPROPERTY(SaveGame) int64 Souls = 0;        // spendable on soul portal upgrades
    UPROPERTY(SaveGame) int64 Earned = 0;       // every Soul picked up this level; spending does not lower it
    UPROPERTY(SaveGame) int64 Stocked = 0;      // ...of which already sent to camp as Soul Coins at a soul portal
    UPROPERTY(SaveGame) float Time = 0.f;       // unpaused time until the level was won
    UPROPERTY(SaveGame) float CombatTime = 0.f; // time with enemies alive
    UPROPERTY(SaveGame) float ComboTime = 0.f;  // ...of which with a combo multiplier going
    UPROPERTY(SaveGame) float EnergySpent = 0.f;
    UPROPERTY(SaveGame) int32 Kills = 0;
};

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
    // The level's Souls so far, and the story unlocks (Progress/CampaignProgress.h).
    UPROPERTY(SaveGame) FHellgirlLevelSouls LevelSouls;
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
    // Coins are the camp's Soul Coins. Inside a level, pickups are Souls (LevelSouls): spent on portal upgrades, and all
    // of them (spent or not) plus bonuses become Soul Coins when the level is won. Each level starts with none.
    UPROPERTY() FHellgirlLevelSouls LevelSouls;
    bool bInLevel = false;
    void ResetLevel();
    // Winning: Earned plus the speed, combo and energy bonuses (kept in LastReward); deposited unless bDeposit is off.
    HellgirlSouls::FReward FinishLevel(bool bDeposit);
    // At a soul portal: send the spendable Souls to camp now as Soul Coins (safe if she falls; no longer spendable).
    // bWrite off keeps it in memory only (automated checks).
    bool StockSouls(bool bWrite);
    int64 LastStocked = 0;
    double StockTime = -10.0;
    // Falling: nothing more is deposited (stocked Soul Coins stay).
    void LoseLevel();
    HellgirlSouls::FReward LastReward;
    double RewardTime = -100.0;
    int64 LastLost = 0;
    double LostTime = -10.0;
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
