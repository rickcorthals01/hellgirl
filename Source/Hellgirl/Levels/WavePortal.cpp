#include "Levels/WavePortal.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Particles/ParticleSystem.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
const FLinearColor SoulBlue(.08f, .35f, 1.f), ExitPurple(.45f, .04f, 1.f);
}

AWavePortal::AWavePortal()
{
    PrimaryActorTick.bCanEverTick = true;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    // SM_Gateway (Tools/Environment/forest_kit.py): two rune stones 2.9 m apart under a lintel, opening facing +X.
    Gateway = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Gateway"));
    Gateway->SetupAttachment(RootComponent);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Stones(TEXT("/Game/Environment/ForestKit/SM_Gateway.SM_Gateway"));
    Gateway->SetStaticMesh(Stones.Object);
    // The stones never block fighters or the camera: a portal can open right where Hellgirl stands.
    Gateway->SetCollisionProfileName(TEXT("BlockAll"));
    Gateway->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
    Gateway->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
    Membrane = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Membrane"));
    Membrane->SetupAttachment(RootComponent);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    Membrane->SetStaticMesh(Sphere.Object);
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> GlowMaterial(TEXT("/Game/Environment/Materials/M_EnvironmentGlow.M_EnvironmentGlow"));
    if (GlowMaterial.Succeeded()) Membrane->SetMaterial(0, GlowMaterial.Object);
    Membrane->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Membrane->SetCastShadow(false);
    Membrane->SetRelativeLocation(FVector(0.f, 0.f, 185.f));
    Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
    Light->SetupAttachment(RootComponent);
    Light->SetRelativeLocation(FVector(60.f, 0.f, 190.f));
    Light->SetAttenuationRadius(900.f);
    Light->SetCastShadows(false);
    static ConstructorHelpers::FObjectFinder<UParticleSystem> Puff(TEXT("/Game/Realistic_Starter_VFX_Pack_Vol2/Particles/Smoke/P_Smoke_A.P_Smoke_A"));
    Smoke = Puff.Object;
    SetActorHiddenInGame(true);
    SetActorEnableCollision(false);
}

void AWavePortal::Open(const FVector& Where, const FVector& Facing, bool bExitPortal)
{
    bExit = bExitPortal;
    FVector Ground = Where;
    FCollisionObjectQueryParams Floor; Floor.AddObjectTypesToQuery(ECC_WorldStatic);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(WavePortalFloor), false, this);
    FHitResult Hit;
    if (GetWorld()->LineTraceSingleByObjectType(Hit, Where + FVector(0, 0, 2000), Where - FVector(0, 0, 2000), Floor, Query)) Ground = Hit.ImpactPoint;
    const FVector Toward = (Facing - Ground).GetSafeNormal2D();
    SetActorLocationAndRotation(Ground - FVector(0, 0, 5), Toward.IsNearlyZero() ? FRotator::ZeroRotator : Toward.Rotation());
    if (!Glow) Glow = Membrane->CreateAndSetMaterialInstanceDynamic(0);
    const FLinearColor Color = bExit ? ExitPurple : SoulBlue;
    if (Glow)
    {
        Glow->SetVectorParameterValue(TEXT("Color"), Color);
        Glow->SetScalarParameterValue(TEXT("PatternStrength"), .9f);
        Glow->SetScalarParameterValue(TEXT("TextureScale"), .004f);
    }
    Light->SetLightColor(Color);
    if (!bOpen && Smoke) UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), Smoke, Ground + FVector(0, 0, 40), FRotator::ZeroRotator, FVector(.8f));
    bOpen = true; Age = 0.f;
    SetActorHiddenInGame(false);
    SetActorEnableCollision(true);
}

void AWavePortal::Close()
{
    if (bOpen && Smoke) UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), Smoke, GetActorLocation() + FVector(0, 0, 40), FRotator::ZeroRotator, FVector(.8f));
    bOpen = false;
    SetActorHiddenInGame(true);
    SetActorEnableCollision(false);
}

bool AWavePortal::IsNear(const FVector& Point, float Radius) const
{
    return bOpen && FVector::DistSquared2D(Point, GetActorLocation()) < FMath::Square(Radius);
}

void AWavePortal::Tick(float Dt)
{
    Super::Tick(Dt);
    if (!bOpen) return;
    Age += Dt;
    // It swells open over half a second, then breathes.
    const float Grow = FMath::InterpEaseOut(0.f, 1.f, FMath::Clamp(Age / .5f, 0.f, 1.f), 2.f);
    const float Pulse = 1.f + .05f * FMath::Sin(Age * 3.f);
    Membrane->SetRelativeScale3D(FVector(.05f, 2.05f * Pulse, 3.2f * Pulse) * Grow);
    if (Glow) Glow->SetScalarParameterValue(TEXT("EmissiveStrength"), (bExit ? .9f : .8f) * (1.f + .2f * FMath::Sin(Age * 5.f)));
    Light->SetIntensity(9000.f * Grow * (1.f + .12f * FMath::Sin(Age * 7.f)));
}
