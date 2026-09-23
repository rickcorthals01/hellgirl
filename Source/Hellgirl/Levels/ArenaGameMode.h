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
    int32 ActivatedSites = 0, ClearedSites = 0, TotalSites = 0, EnemiesRemaining = 0, Kills = 0;
    int32 MapNumber = 1; // Layout identity; legacy layouts 2/3 are retained.
    int32 CampaignLevel = 1;
    bool bLegacyMap = false;
    bool bForestHub = false;
    bool bStoryEnabled = false;
    bool IsImpArena() const { return !bLegacyMap && !bForestHub && CampaignLevel == 4; }
    void QueueStory(FName Moment);
    void StoryFinished(FName Moment);
    void QueenSurrendered(class AArenaFighter* Queen);
    bool TickStory(float Dt);
    void TravelToHub();
    void BuildForestHub();
    void BuildForestHubDetails();
    void TickForestHub(float Dt);
    int32 GetHubInteraction() const;
    void RunForestHubCheck(float Dt);
    void TravelToCampaign(int32 Level);
    int32 GetUnlockedLevel() const;
    void RunCampaignCheck(float Dt);
    void TickGoblinPrelude(float Dt);
    void RunGoblinStageCheck();
    void BuildImpArena();
    void BuildImpArenaDetails();
    void TickImpArena(float Dt);
    void RunImpArenaCheck();
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
};
