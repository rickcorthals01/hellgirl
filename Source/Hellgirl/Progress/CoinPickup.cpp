#include "Progress/CoinPickup.h"
#include "Rules/CoinPickupRules.h"
#include "Fighter/ArenaFighter.h"
#include "Progress/HellgirlWallet.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"
#include "Components/PointLightComponent.h"

// The currency is souls: each pickup is a small glowing wisp with its own soft light.
// (The class and wallet keep their original "coin" names so existing saves still load.)
ACoinPickup::ACoinPickup()
{
    PrimaryActorTick.bCanEverTick = true;
    Coin = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Coin"));
    RootComponent = Coin;
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> GlowMaterial(TEXT("/Game/Environment/Materials/M_EnvironmentGlow.M_EnvironmentGlow"));
    Coin->SetStaticMesh(Mesh.Object);
    if (GlowMaterial.Succeeded()) Coin->SetMaterial(0, GlowMaterial.Object);
    Coin->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Coin->SetCastShadow(false);
    Coin->SetRelativeScale3D(FVector(.2f));
    Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("SoulLight"));
    Glow->SetupAttachment(RootComponent);
    Glow->SetAbsolute(false, false, true);
    Glow->SetIntensity(900.f);
    Glow->SetAttenuationRadius(220.f);
    Glow->SetCastShadows(false);
    Glow->SetLightColor(FLinearColor(.45f, .8f, 1.f));
    Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Value"));
    Label->SetupAttachment(RootComponent);
    Label->SetAbsolute(false, true, true);
    Label->SetHorizontalAlignment(EHorizTextAligment::EHTA_Center);
    Label->SetTextRenderColor(FColor(150, 215, 255));
    Label->SetWorldSize(26.f);
}

void ACoinPickup::SetAmount(int32 Value)
{
    Amount = FMath::Clamp(Value, 1, 6);
    Label->SetText(FText::FromString(FString::Printf(TEXT("+%d"), Amount)));
    if (UMaterialInstanceDynamic* Material = Coin->CreateAndSetMaterialInstanceDynamic(0))
    {
        Material->SetVectorParameterValue(TEXT("Color"), FLinearColor(.3f, .65f, 1.f));
        Material->SetScalarParameterValue(TEXT("EmissiveStrength"), 2.2f);
    }
}

void ACoinPickup::Tick(float Dt)
{
    Super::Tick(Dt);
    if (bCollected) return;
    Age += Dt;
    // A soul pulses gently instead of spinning.
    const float Pulse = 1.f + .12f * FMath::Sin(Age * 5.f);
    Coin->SetRelativeScale3D(FVector(.2f * Pulse));
    Glow->SetIntensity(900.f * Pulse);
    Label->SetWorldLocation(GetActorLocation() + FVector(0.f, 0.f, 45.f + FMath::Sin(Age * 4.f) * 6.f));
    AArenaFighter* Player = Cast<AArenaFighter>(UGameplayStatics::GetPlayerPawn(this, 0));
    if (!Player) return;
    if (APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(this, 0))
        Label->SetWorldRotation((Camera->GetCameraLocation() - Label->GetComponentLocation()).Rotation());
    if (!Player->IsAlive() || Age < .35f) return;
    const FVector PlayerPosition = Player->GetActorLocation();
    FVector Delta = PlayerPosition - GetActorLocation();
    FCollisionQueryParams Query;
    Query.AddIgnoredActor(this); Query.AddIgnoredActor(Player);
    FHitResult Obstacle;
    if (Delta.SizeSquared() <= FMath::Square(HellgirlCoins::AttractionRadius))
    {
        const FVector Next = FMath::VInterpConstantTo(GetActorLocation(), PlayerPosition, Dt, HellgirlCoins::AttractionSpeed);
        if (!GetWorld()->LineTraceSingleByChannel(Obstacle, GetActorLocation(), Next, ECC_Visibility, Query))
            SetActorLocation(Next);
    }
    // Collection requires contact with the player's capsule, not entry into the attraction radius.
    const UCapsuleComponent* Capsule = Player->GetCapsuleComponent();
    Delta = GetActorLocation() - PlayerPosition;
    if (!HellgirlCoins::TouchesCapsule(Delta.X, Delta.Y, Delta.Z, Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight())) return;
    if (GetWorld()->LineTraceSingleByChannel(Obstacle, GetActorLocation(), PlayerPosition, ECC_Visibility, Query)) return;
    if (bHeart)
    {
        Player->Health=FMath::Min(Player->MaxHealth,Player->Health+Player->MaxHealth*.5f);
        bCollected=true; Destroy(); return;
    }
    if (UHellgirlWallet* Wallet = Cast<UHellgirlWallet>(GetGameInstance()))
    {
        if (Wallet->Collect(Amount))
        {
            bCollected = true;
            Destroy();
        }
    }
}

void ACoinPickup::SetHeart()
{
    bHeart=true;
    Label->SetText(FText::FromString(TEXT("HEART +50% HP")));
    Label->SetTextRenderColor(FColor::Red);
    Glow->SetLightColor(FLinearColor(1.f,.1f,.15f));
    if (auto* Material=Coin->CreateAndSetMaterialInstanceDynamic(0))
    {
        Material->SetVectorParameterValue(TEXT("Color"),FLinearColor(1.f,.03f,.15f));
        Material->SetScalarParameterValue(TEXT("EmissiveStrength"),4.f);
    }
}
