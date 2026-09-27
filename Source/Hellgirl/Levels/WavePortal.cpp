#include "Levels/WavePortal.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Particles/ParticleSystem.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
const FLinearColor PortalSoulBlue(.08f, .35f, 1.f), PortalExitPurple(.45f, .04f, 1.f);
// The painting is 1086 x 1448: the card is 2.5 m wide and 3.3 m tall, standing just above the ground.
constexpr float CardHeight = 330.f, CardWidth = 247.f, CardLift = 10.f;
}

AWavePortal::AWavePortal()
{
    PrimaryActorTick.bCanEverTick = true;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    // A flat card (the engine's 1 m plane, stood upright) carrying the painted portal; it turns to face the camera.
    Card = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Card"));
    Card->SetupAttachment(RootComponent);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Plane(TEXT("/Engine/BasicShapes/Plane.Plane"));
    Card->SetStaticMesh(Plane.Object);
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> PortalMaterial(TEXT("/Game/Environment/Portals/M_Portal.M_Portal"));
    if (PortalMaterial.Succeeded()) Card->SetMaterial(0, PortalMaterial.Object);
    static ConstructorHelpers::FObjectFinder<UTexture> Blue(TEXT("/Game/Environment/Portals/T_PortalBlue.T_PortalBlue"));
    static ConstructorHelpers::FObjectFinder<UTexture> Purple(TEXT("/Game/Environment/Portals/T_PortalPurple.T_PortalPurple"));
    BlueArt = Blue.Object; PurpleArt = Purple.Object;
    Card->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Card->SetCastShadow(false);
    Card->SetRelativeLocation(FVector(0.f, 0.f, CardLift + CardHeight * .5f));
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
    if (!Glow) Glow = Card->CreateAndSetMaterialInstanceDynamic(0);
    if (Glow)
    {
        if (UTexture* Art = bExit ? PurpleArt : BlueArt) Glow->SetTextureParameterValue(TEXT("Portal"), Art);
        Glow->SetScalarParameterValue(TEXT("Fade"), 0.f);
    }
    Light->SetLightColor(bExit ? PortalExitPurple : PortalSoulBlue);
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
    const float Pulse = 1.f + .025f * FMath::Sin(Age * 3.f);
    // The plane lies flat (1 m square); pitched up it stands, its length along the portal's height.
    Card->SetRelativeScale3D(FVector(CardHeight / 100.f, CardWidth / 100.f, 1.f) * FMath::Max(Grow * Pulse, .01f));
    Card->SetRelativeLocation(FVector(0.f, 0.f, CardLift + CardHeight * .5f * Grow * Pulse));
    // Always square to the camera, upright.
    if (const APlayerCameraManager* View = UGameplayStatics::GetPlayerCameraManager(this, 0))
    {
        const FVector ToCamera = (View->GetCameraLocation() - Card->GetComponentLocation()).GetSafeNormal2D();
        if (!ToCamera.IsNearlyZero()) Card->SetWorldRotation(FRotator(90.f, ToCamera.Rotation().Yaw, 0.f));
    }
    if (Glow)
    {
        Glow->SetScalarParameterValue(TEXT("Fade"), Grow);
        Glow->SetScalarParameterValue(TEXT("Strength"), 2.2f * (1.f + .12f * FMath::Sin(Age * 5.f)));
    }
    Light->SetIntensity(9000.f * Grow * (1.f + .12f * FMath::Sin(Age * 7.f)));
}
