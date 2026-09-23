#include "Enemies/EnemySpawnPoint.h"
#include "Fighter/ArenaFighter.h"
#include "Rules/StageOneLayout.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "UObject/ConstructorHelpers.h"

AEnemySpawnPoint::AEnemySpawnPoint()
{
    PrimaryActorTick.bCanEverTick = true;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    Portal = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Rift"));
    Portal->SetupAttachment(RootComponent);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    Portal->SetStaticMesh(Mesh.Object);
    Portal->SetRelativeLocation(FVector(0.f, 0.f, 180.f));
    Portal->SetRelativeScale3D(FVector(.5f, 2.3f, 4.f));
    Portal->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
    Glow->SetupAttachment(RootComponent);
    Glow->SetRelativeLocation(FVector(0.f, 0.f, 200.f));
    Glow->SetLightColor(FLinearColor(1.f, .12f, .025f));
    Glow->SetIntensity(15000.f);
    Glow->SetAttenuationRadius(1100.f);
    Glow->SetCastShadows(false);
    Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("SiteLabel"));
    Label->SetupAttachment(RootComponent);
    Label->SetRelativeLocation(FVector(0.f, 0.f, 480.f));
    Label->SetHorizontalAlignment(EHorizTextAligment::EHTA_Center);
    Label->SetWorldSize(60.f);
    Label->SetTextRenderColor(FColor(255, 130, 50));
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
    const bool Flying = Spawned >= EnemyCount - FlyingCount;
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
    Enemy->bGuardHome = bInstantGroup;
    if (bBoss)
    {
        Enemy->MaxHealth = Enemy->Health = 1350.f;
        Enemy->AttackDamage = 28.f;
        Enemy->AttackRange = 240.f;
        Enemy->SetActorScale3D(FVector(1.5f));
    }
    Enemies.Add(Enemy);
    ++Spawned;
    SpawnDelay = .55f;
    DrawDebugSphere(GetWorld(), Enemy->GetActorLocation(), 130.f, 12, FColor::Orange, false, .4f, 0, 4.f);
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
        if (Spawned < EnemyCount && SpawnDelay <= 0.f)
        {
            SpawnOne();
            if (bInstantGroup)
                while (Spawned < EnemyCount) { const int32 Before = Spawned; SpawnOne(); if (Before == Spawned) break; }
        }
        if (Spawned >= EnemyCount && LivingEnemies() == 0) bCleared = true;
    }
    const FColor Color = bCleared ? FColor(80, 160, 110) : (bActivated ? FColor::Red : FColor::Orange);
    Label->SetTextRenderColor(Color);
    Label->SetText(FText::FromString(SiteName + (bCleared ? TEXT(" / CLEARED") : (bActivated ? TEXT(" / ACTIVE") : TEXT(" / DORMANT")))));
    Glow->SetIntensity(bCleared ? 1000.f : (bActivated ? 30000.f : 15000.f));
    Portal->SetVisibility(!bCleared);
    DrawDebugCircle(GetWorld(), GetActorLocation() + FVector(0.f, 0.f, 15.f), 230.f, 32, Color, false, -1.f, 0, 5.f,
        FVector::ForwardVector, FVector::RightVector, false);
    if (APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(this, 0))
        Label->SetWorldRotation((Camera->GetCameraLocation() - Label->GetComponentLocation()).Rotation());
}
