#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Enemies/EnemyTypes.h"
#include "EnemySpawnPoint.generated.h"
class AArenaFighter;
class UStaticMeshComponent;
class UTextRenderComponent;
class UPointLightComponent;

UCLASS(Blueprintable)
class HELLGIRL_API AEnemySpawnPoint : public AActor
{
    GENERATED_BODY()
public:
    AEnemySpawnPoint();
    virtual void Tick(float DeltaSeconds) override;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spawn Site") float ActivationRadius = 2000.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spawn Site") int32 EnemyCount = 4;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spawn Site") int32 Difficulty = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spawn Site") int32 FlyingCount = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spawn Site") FString SiteName = TEXT("RIFT");
    bool bActivated = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spawn Site") EHellgirlEnemyType GroundType = EHellgirlEnemyType::Imps;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spawn Site") EHellgirlEnemyType FlyingType = EHellgirlEnemyType::FlyingImps;
    bool bEnabled = true;
    bool bBoss = false;
    bool bInstantGroup = false;
    // Instant groups guard their spawn spot unless this is off (an ambush rushes the player instead).
    bool bGroupGuardsHome = true;
    // Size of non-boss enemies from this site (elite waves are drawn larger).
    float EnemyScale = 1.f;
    bool bCleared = false;
    int32 LivingEnemies() const;
    // Enemies this site actually spawns: EnemyCount/FlyingCount scaled by Rules/EnemyTuning.h (bosses unscaled).
    int32 WaveTotal() const;
    int32 WaveFlyers() const;
    bool RegisterReinforcement(AArenaFighter* Enemy);
    // Goblins come out of a dirt burrow instead of a burning hell rift.
    void UseBurrow();
private:
    void SpawnOne();
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Portal;
    UPROPERTY() TObjectPtr<UTextRenderComponent> Label;
    UPROPERTY() TObjectPtr<UPointLightComponent> Glow;
    UPROPERTY() TObjectPtr<class UNiagaraComponent> Fire;
    UPROPERTY() TObjectPtr<class UParticleSystem> ArrivalSmoke;
    bool bBurrow = false;
    TArray<TWeakObjectPtr<AArenaFighter>> Enemies;
    int32 Spawned = 0;
    float SpawnDelay = 0.f;
};
