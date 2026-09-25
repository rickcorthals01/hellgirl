#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WavePortal.generated.h"
class UStaticMeshComponent;
class UPointLightComponent;
class UMaterialInstanceDynamic;

// A rune gateway with a glowing membrane. Between waves it is a pale-blue soul portal (continue, stock souls,
// upgrades); at the end of a level it is the purple exit back to camp.
UCLASS()
class HELLGIRL_API AWavePortal : public AActor
{
    GENERATED_BODY()
public:
    AWavePortal();
    virtual void Tick(float DeltaSeconds) override;
    // Stands the gateway on the ground at Where, its opening turned toward Facing.
    void Open(const FVector& Where, const FVector& Facing, bool bExit);
    void Close();
    bool IsOpen() const { return bOpen; }
    bool IsExit() const { return bExit; }
    bool IsNear(const FVector& Point, float Radius = 260.f) const;
private:
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Gateway;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Membrane;
    UPROPERTY() TObjectPtr<UPointLightComponent> Light;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> Glow;
    UPROPERTY() TObjectPtr<class UParticleSystem> Smoke;
    bool bOpen = false, bExit = false;
    float Age = 0.f;
};
