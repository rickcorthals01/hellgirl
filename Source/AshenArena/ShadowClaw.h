#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ShadowClaw.generated.h"
UCLASS()
class ASHENARENA_API AShadowClaw : public AActor
{
    GENERATED_BODY()
public:
    AShadowClaw();
    virtual void Tick(float Dt) override;
    FVector Velocity=FVector::ZeroVector;
private:
    UPROPERTY() TObjectPtr<class UStaticMeshComponent> Visual;
};
