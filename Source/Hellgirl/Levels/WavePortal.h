#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WavePortal.generated.h"
class UStaticMeshComponent;
class UPointLightComponent;
class UMaterialInstanceDynamic;

// A glowing painted portal (T_PortalBlue / T_PortalPurple on M_Portal, Tools/Environment/import_portals.py), a card
// that always turns to face the camera. Between waves it is the pale-blue soul portal (continue, stock souls,
// upgrades); at the end of a level it is the purple exit back to camp.
UCLASS()
class HELLGIRL_API AWavePortal : public AActor
{
    GENERATED_BODY()
public:
    AWavePortal();
    virtual void Tick(float DeltaSeconds) override;
    // Stands the portal on the ground at Where (Facing only matters for where its light sits).
    void Open(const FVector& Where, const FVector& Facing, bool bExit);
    void Close();
    bool IsOpen() const { return bOpen; }
    bool IsExit() const { return bExit; }
    bool IsNear(const FVector& Point, float Radius = 260.f) const;
private:
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Card;
    UPROPERTY() TObjectPtr<UPointLightComponent> Light;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> Glow;
    UPROPERTY() TObjectPtr<class UTexture> BlueArt;
    UPROPERTY() TObjectPtr<class UTexture> PurpleArt;
    UPROPERTY() TObjectPtr<class UParticleSystem> Smoke;
    bool bOpen = false, bExit = false;
    float Age = 0.f;
};
