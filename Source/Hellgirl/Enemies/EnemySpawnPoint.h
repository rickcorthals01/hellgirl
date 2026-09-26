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
    // A second kind of ground enemy mixed into the wave (the swamp's rats and frogs): MixCount of EnemyCount are MixType.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spawn Site") EHellgirlEnemyType MixType = EHellgirlEnemyType::Imps;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spawn Site") int32 MixCount = 0;
    bool bEnabled = true;
    bool bBoss = false;
    bool bInstantGroup = false;
    // Instant groups guard their spawn spot unless this is off (an ambush rushes the player instead).
    bool bGroupGuardsHome = true;
    // Size of non-boss enemies from this site (elite waves are drawn larger).
    float EnemyScale = 1.f;
    bool bCleared = false;
    int32 LivingEnemies() const;
    int32 GetSpawned() const { return Spawned; }
    // Enemies this site actually spawns: EnemyCount/FlyingCount scaled by Rules/EnemyTuning.h (bosses unscaled).
    int32 WaveTotal() const;
    int32 WaveFlyers() const;
    int32 WaveMixed() const;
    bool RegisterReinforcement(AArenaFighter* Enemy);
    // Goblins come out of a dirt burrow (forest run).
    void UseBurrow();
    // The burning hell rift marker is switched off (too much clutter); the mesh and fire are kept for later use.
    bool bShowRift = false;
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
    int32 SpawnRetries = 0;
    float SpawnDelay = 0.f;
};
