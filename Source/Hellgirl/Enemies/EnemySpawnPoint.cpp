#include "Enemies/EnemySpawnPoint.h"
#include "Fighter/ArenaFighter.h"
#include "Rules/StageOneLayout.h"
#include "Rules/EnemyTuning.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Particles/ParticleSystem.h"
#include "UObject/ConstructorHelpers.h"

AEnemySpawnPoint::AEnemySpawnPoint()
{
    PrimaryActorTick.bCanEverTick = true;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    // A ring of obsidian shards around a glowing crack; it burns while its enemies are coming through.
    Portal = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Rift"));
    Portal->SetupAttachment(RootComponent);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Rift(TEXT("/Game/Environment/ForestKit/SM_HellRift.SM_HellRift"));
    Portal->SetStaticMesh(Rift.Object);
    Portal->SetRelativeLocation(FVector(0.f, 0.f, 2.f));
    Portal->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Fire = CreateDefaultSubobject<UNiagaraComponent>(TEXT("RiftFire"));
    Fire->SetupAttachment(RootComponent);
    static ConstructorHelpers::FObjectFinder<UNiagaraSystem> Flames(TEXT("/Game/Stylish_Fire_VFX/Niagara/NS_Stylish_Fire_2.NS_Stylish_Fire_2"));
    Fire->SetAsset(Flames.Object);
    Fire->SetRelativeLocation(FVector(0.f, 0.f, 10.f));
    Fire->SetRelativeScale3D(FVector(1.3f));
    Fire->bAutoActivate = false;
    static ConstructorHelpers::FObjectFinder<UParticleSystem> Smoke(TEXT("/Game/Realistic_Starter_VFX_Pack_Vol2/Particles/Smoke/P_Smoke_A.P_Smoke_A"));
    ArrivalSmoke = Smoke.Object;
    Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
    Glow->SetupAttachment(RootComponent);
    Glow->SetRelativeLocation(FVector(0.f, 0.f, 90.f));
    Glow->SetLightColor(FLinearColor(1.f, .22f, .04f));
    Glow->SetIntensity(3000.f);
    Glow->SetAttenuationRadius(700.f);
    Glow->SetCastShadows(false);
    // The site name is shown by the HUD objective; the old floating label stays hidden.
    Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("SiteLabel"));
    Label->SetupAttachment(RootComponent);
    Label->SetHiddenInGame(true);
}

void AEnemySpawnPoint::UseBurrow()
{
    bBurrow = true;
    if (auto* Burrow = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Environment/ForestKit/SM_Burrow.SM_Burrow"))) Portal->SetStaticMesh(Burrow);
    Fire->SetAsset(nullptr);
    Glow->SetLightColor(FLinearColor(1.f, .45f, .15f));
    Glow->SetRelativeLocation(FVector(0.f, 0.f, 40.f));
}

int32 AEnemySpawnPoint::WaveTotal() const
{
    // Ordinary waves are scaled up globally (Rules/EnemyTuning.h); a boss site spawns just its boss.
    return bBoss ? EnemyCount : EnemyTuning::WaveSize(EnemyCount);
}

int32 AEnemySpawnPoint::WaveFlyers() const
{
    return FMath::Min(WaveTotal(), bBoss ? FlyingCount : EnemyTuning::WaveSize(FlyingCount));
}

int32 AEnemySpawnPoint::LivingEnemies() const
{
    int32 Count = 0;
    for (const TWeakObjectPtr<AArenaFighter>& Enemy : Enemies)
        if (Enemy.IsValid() && Enemy->IsAlive()) ++Count;
    return Count;
}

bool AEnemySpawnPoint::RegisterReinforcement(AArenaFighter* Enemy)
{
    if (!bBoss || !bActivated || bCleared || !IsValid(Enemy) || !Enemy->IsAlive()) return false;
    Enemies.AddUnique(Enemy);
    Enemy->EncounterSite = this;
    return true;
}

void AEnemySpawnPoint::SpawnOne()
{
    const float Angle = Spawned * 2.39996f;
    const float Radius = 190.f + 45.f * Spawned;
    const float X = static_cast<float>(GetActorLocation().X) + FMath::Cos(Angle) * Radius;
    const float Y = static_cast<float>(GetActorLocation().Y) + FMath::Sin(Angle) * Radius;
    const bool Flying = Spawned >= WaveTotal() - WaveFlyers();
    // Each position on the spawn spiral can have a different terrain height.
    // Ignore pawns so a previous spawn cannot become the next enemy's floor.
    FCollisionObjectQueryParams FloorTypes;
    FloorTypes.AddObjectTypesToQuery(ECC_WorldStatic);
    FloorTypes.AddObjectTypesToQuery(ECC_WorldDynamic);
    FCollisionQueryParams FloorQuery(SCENE_QUERY_STAT(EnemySpawnFloor), false, this);
    FHitResult Floor;
    float GroundZ = GetActorLocation().Z;
    const float WalkableZ = GetDefault<AArenaFighter>()->GetCharacterMovement()->GetWalkableFloorZ();
    if (GetWorld()->LineTraceSingleByObjectType(Floor, FVector(X,Y,GroundZ + 1000.f),
        FVector(X,Y,GroundZ - 800.f), FloorTypes, FloorQuery) && Floor.ImpactNormal.Z >= WalkableZ)
    {
        GroundZ = Floor.ImpactPoint.Z;
    }
    else
    {
        // Preserve the original height fallback for a flyer over a platform gap.
        UE_LOG(LogTemp, Warning, TEXT("Spawn floor unavailable for %s enemy %d; using site height"), *SiteName, Spawned + 1);
    }
    const float SpawnOffset = Flying ? 310.f : (bBoss ? 142.f : 110.f);
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;
    AArenaFighter* Enemy = GetWorld()->SpawnActor<AArenaFighter>(FVector(X, Y, GroundZ + SpawnOffset), FRotator::ZeroRotator, Params);
    if (!Enemy) { SpawnDelay = .8f; return; }
    Enemy->bBossEncounter = bBoss;
    Enemy->MakeEnemy(Difficulty, Flying);
    Enemy->SetEnemyType(Flying ? FlyingType : GroundType);
    Enemy->HomePosition = GetActorLocation();
    Enemy->EncounterSite = this;
    Enemy->bGuardHome = bInstantGroup && bGroupGuardsHome;
    if (!bBoss && EnemyScale != 1.f) Enemy->SetActorScale3D(FVector(EnemyScale));
    if (bBoss)
    {
        Enemy->MaxHealth = Enemy->Health = 1350.f;
        Enemy->AttackDamage = 28.f;
        Enemy->AttackRange = 240.f;
        Enemy->SetActorScale3D(FVector(1.5f));
    }
    Enemies.Add(Enemy);
    ++Spawned;
    SpawnDelay = EnemyTuning::SpawnInterval;
    if (ArrivalSmoke) UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), ArrivalSmoke, FVector(X, Y, GroundZ + 20.f), FRotator::ZeroRotator, FVector(.6f));
}

void AEnemySpawnPoint::Tick(float Dt)
{
    Super::Tick(Dt);
    SetActorHiddenInGame(!bEnabled);
    if (!bEnabled) return;
    const AArenaFighter* Player = Cast<AArenaFighter>(UGameplayStatics::GetPlayerPawn(this, 0));
    if (Player && StageOne::ShouldActivate(bActivated, Player->IsAlive(),
        static_cast<float>(FVector::DistSquared2D(GetActorLocation(), Player->GetActorLocation())), ActivationRadius))
        bActivated = true;
    if (bActivated && !bCleared && Player && Player->IsAlive())
    {
        SpawnDelay -= Dt;
        if (Spawned < WaveTotal() && SpawnDelay <= 0.f)
        {
            SpawnOne();
            if (bInstantGroup)
                while (Spawned < WaveTotal()) { const int32 Before = Spawned; SpawnOne(); if (Before == Spawned) break; }
        }
        if (Spawned >= WaveTotal() && LivingEnemies() == 0) bCleared = true;
    }
    // Burning while enemies come through, smouldering before, cold once cleared.
    const bool Burning = bActivated && !bCleared;
    if (Burning != Fire->IsActive()) { if (Burning) Fire->Activate(true); else Fire->Deactivate(); }
    const float Flicker = 1.f + .15f * FMath::Sin(GetWorld()->GetTimeSeconds() * 13.f) + .08f * FMath::Sin(GetWorld()->GetTimeSeconds() * 29.f);
    Glow->SetIntensity(bCleared ? 0.f : (Burning ? (bBurrow ? 4000.f : 9000.f) * Flicker : 1800.f));
}
