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
    bool bCleared = false;
    int32 LivingEnemies() const;
    bool RegisterReinforcement(AArenaFighter* Enemy);
private:
    void SpawnOne();
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Portal;
    UPROPERTY() TObjectPtr<UTextRenderComponent> Label;
    UPROPERTY() TObjectPtr<UPointLightComponent> Glow;
    TArray<TWeakObjectPtr<AArenaFighter>> Enemies;
    int32 Spawned = 0;
    float SpawnDelay = 0.f;
};
