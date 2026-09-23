#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CoinPickup.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;

UCLASS()
class HELLGIRL_API ACoinPickup : public AActor
{
    GENERATED_BODY()
public:
    ACoinPickup();
    virtual void Tick(float DeltaSeconds) override;
    void SetAmount(int32 Value);
    void SetHeart();
private:
    friend class UHellgirlWallet;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Coin;
    UPROPERTY() TObjectPtr<UTextRenderComponent> Label;
    int32 Amount = 1;
    float Age = 0.f;
    bool bCollected = false;
    bool bHeart = false;
};
