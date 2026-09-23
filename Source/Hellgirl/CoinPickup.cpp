#include "CoinPickup.h"
#include "CoinPickupRules.h"
#include "ArenaFighter.h"
#include "HellgirlWallet.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

ACoinPickup::ACoinPickup()
{
    PrimaryActorTick.bCanEverTick = true;
    Coin = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Coin"));
    RootComponent = Coin;
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    Coin->SetStaticMesh(Mesh.Object);
    Coin->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Coin->SetRelativeScale3D(FVector(.32f, .32f, .07f));
    Coin->SetRelativeRotation(FRotator(90.f, 0.f, 0.f));
    Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Value"));
    Label->SetupAttachment(RootComponent);
    Label->SetAbsolute(false, true, true);
    Label->SetHorizontalAlignment(EHorizTextAligment::EHTA_Center);
    Label->SetTextRenderColor(FColor(255, 205, 45));
    Label->SetWorldSize(28.f);
}

void ACoinPickup::SetAmount(int32 Value)
{
    Amount = FMath::Clamp(Value, 1, 6);
    Label->SetText(FText::FromString(FString::Printf(TEXT("+%d"), Amount)));
    if (UMaterialInstanceDynamic* Material = Coin->CreateAndSetMaterialInstanceDynamic(0))
        Material->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.f, .65f, .04f));
}

void ACoinPickup::Tick(float Dt)
{
    Super::Tick(Dt);
    if (bCollected) return;
    Age += Dt;
    AddActorWorldRotation(FRotator(0.f, Dt * 100.f, 0.f));
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
    if (auto* Material=Coin->CreateAndSetMaterialInstanceDynamic(0)) Material->SetVectorParameterValue(TEXT("Color"),FLinearColor(1.f,.03f,.15f));
}
