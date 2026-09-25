#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ArenaGameMode.generated.h"
class AEnemySpawnPoint;
class AStaticMeshActor;

UCLASS()
class HELLGIRL_API AArenaGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    AArenaGameMode();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    int32 ActivatedSites = 0, ClearedSites = 0, TotalSites = 0, EnemiesRemaining = 0, Kills = 0;
    int32 MapNumber = 1; // Layout identity; legacy layouts 2/3 are retained.
    int32 CampaignLevel = 1;
    bool bLegacyMap = false;
    bool bForestHub = false;
    // World III map preview (URL option SuccubusCourt=1); separate from the campaign level numbers.
    bool bSuccubusCourt = false;
    bool bStoryEnabled = false;
    bool IsImpArena() const { return !bLegacyMap && !bForestHub && CampaignLevel == 4; }
    void QueueStory(FName Moment);
    void StoryFinished(FName Moment);
    bool HasPlayedStory(FName Moment) const;
    // Answering YES at a level's exit portal (Stage 1 plays a last line first).
    void UseExitPortal();
    void QueenSurrendered(class AArenaFighter* Queen);
    bool TickStory(float Dt);
    void TravelToHub();
    void BuildForestHub();
    void BuildForestHubDetails();
    void TickForestHub(float Dt);
    int32 GetHubInteraction() const;
    bool HasMerchant() const { return HubMerchant != nullptr; }
    void RunForestHubCheck(float Dt);
    void TravelToCampaign(int32 Level);
    int32 GetUnlockedLevel() const;
    void RunCampaignCheck(float Dt);
    void TickGoblinPrelude(float Dt);
    void RunGoblinStageCheck();
    void BuildImpArena();
    void BuildImpArenaDetails();
    void BuildImpArenaInferno();
    void TickImpArena(float Dt);
    void RunImpArenaCheck();
    void BuildSuccubusCourt();
    void TickSuccubusCourt(float Dt);
    void RunSuccubusCourtCheck();
    void TravelToSuccubusCourt();
    // Forest run (URL option ForestRun=1?Seed=N?Room=R): randomised forest rooms, see Levels/ForestRun.cpp.
    bool bForestRun = false;
    int32 ForestSeed = 1, ForestRoomNumber = 1;
    void StartForestRun();
    void TravelToForestRoom(int32 Seed, int32 Room);
    void BuildForestRun();
    void BuildForestRunScenery();
    void TickForestRun(float Dt);
    void ShowForestExit(bool Open);
    void RunMapShot(float Dt);
    void RunForestRunCheck();
    // World I Stages 2 and 3 and the endless mode follow the scripts in Levels/GoblinWaves.cpp:
    // waves, conversations and soul portals in order, then the exit portal.
    bool bEndless = false;
    int32 EndlessWave = 0;
    void BuildGoblinWaves();
    void TickGoblinWaves(float Dt);
    void TickEndless(float Dt);
    bool RunEndlessCheck();
    void RunNaturalWavesCheck(float Dt);
    void RescueStragglers(class AArenaFighter* Hero, float Dt);
    void StartEndless();
    // Soul portal menu choices: continue to the next wave, stock carried souls, or (endless / exit) leave for camp.
    enum class EPortalChoice : uint8 { Continue, Stock, Leave, Stay };
    void ChoosePortal(EPortalChoice Choice);
    // Soul portal upgrades (Rules/PortalUpgrades.h): five offers per portal, each can be bought once with carried souls;
    // they last for the rest of the level.
    const TArray<int32>& GetPortalOffers() const { return PortalOffers; }
    bool IsOfferSold(int32 Offer) const { return OfferSold.IsValidIndex(Offer) && OfferSold[Offer]; }
    int32 GetUpgradeLevel(int32 Upgrade) const { return UpgradeLevels.IsValidIndex(Upgrade) ? UpgradeLevels[Upgrade] : 0; }
    int32 GetUpgradeCost(int32 Upgrade) const;
    bool BuyUpgrade(int32 Offer);
    const TArray<int32>& GetUpgradeLevels() const { return UpgradeLevels; }
    void RestoreUpgrades(const TArray<int32>& Levels);
    bool IsSoulPortalOpen() const;
    bool IsPortalIntroPending() const;
    FVector GetSoulPortalLocation() const;
    bool IsExitOpen() const;
    void ShowExitPortal(bool Open);
    // The level is won: carried souls are banked and the next stage unlocks.
    void CompleteLevel();
    // A short on-screen tip (e.g. how to use the ultimate), drawn by the HUD until TipUntil (real time).
    FString Tip;
    double TipUntil = 0.0;
    bool bWon = false;
    FString MapTitle, Objective, Prompt;
    TArray<FVector4> MapPlatforms;
    FVector ExitPosition;
    void EnemyDefeated(const FVector& Location);
    void RestartMap();
    void AnswerPrompt(bool Yes);
    const TArray<TObjectPtr<AEnemySpawnPoint>>& GetSpawnSites() const { return SpawnSites; }
private:
    bool bShowStartupMenu = false;
    friend class UHellgirlWallet;
    void BuildArena();
    void BuildEnvironmentDetails();
    void RockPlatform(FVector Center, FVector Size, FLinearColor Color);
    void RunMapVisualCheck(float Dt);
    void RunTerrainCheck(float Dt);
    void RunMapCheck(float Dt);
    AStaticMeshActor* Prop(FVector Position, FVector Scale, FLinearColor Color, bool Sphere = false);
    void Platform(FVector Center, FVector Size, FLinearColor Color);
    void Bridge(FVector From, FVector To, bool Gaps);
    AEnemySpawnPoint* Site(FVector Position, FString Name, int32 Count, int32 Flyers, bool Enabled, bool Boss = false);
    void Travel(int32 Map);
    UPROPERTY() TArray<TObjectPtr<AEnemySpawnPoint>> SpawnSites;
    UPROPERTY() TObjectPtr<AStaticMeshActor> ExitPortal;
    UPROPERTY() TObjectPtr<AStaticMeshActor> BossOrb;
    UPROPERTY() TArray<TObjectPtr<AStaticMeshActor>> SectionGates;
    UPROPERTY() TArray<TObjectPtr<AStaticMeshActor>> SectionBarriers;
    int32 ArenaWaveIndex = -1;
    float WaveCountdown = 0.f;
    FVector LastSafePosition;
    float HazardClock = 0.f;
    bool bDeclined = false;
    int32 PromptAction = 0;
    UPROPERTY() TObjectPtr<class APointLight> CampfireLight;
    UPROPERTY() TObjectPtr<class ASkeletalMeshActor> HubMerchant;
    TArray<FVector> HubInteractionPoints;
    float HubTime = 0.f;
    bool bHubLevelMenuTriggered = false;
    TArray<FName> PendingStory;
    TSet<FName> PlayedStory;
    TWeakObjectPtr<class AArenaFighter> SurrenderedQueen;
    float QueenFleeClock=0.f;
    FVector WokeAt = FVector::ZeroVector;
    UPROPERTY() TObjectPtr<class AWavePortal> SoulPortal;
    UPROPERTY() TObjectPtr<class AWavePortal> ExitGate;
    FName OpenPortalId;          // the soul portal step now standing (its id is added to PlayedStory on Continue)
    FName OpenPortalIntro;       // its help text, which plays before the portal can be used
    bool bPortalMenuDeclined = false;
    bool bLevelCompleted = false;
    bool bPlayerDeathHandled = false;
    bool bKeepCarriedSouls = false; // set when moving on to the next forest room
    int32 ScriptStep = -1;
    int32 EndlessWaveStart = 0; // first spawn site of the current endless wave
    TArray<int32> UpgradeLevels, PortalOffers;
    TArray<bool> OfferSold;
    void RollPortalOffers();
    void ApplyUpgrades();
    int32 LastLivingEnemies = -1;
    float StragglerClock = 0.f;
    void TickPortalMenus(class AArenaFighter* Hero);
    TArray<FVector4> ForestThorns; // X, Y, radius
    UPROPERTY() TObjectPtr<AStaticMeshActor> ForestExitMarker;
    UPROPERTY() TObjectPtr<class APointLight> ForestExitLight;
};
