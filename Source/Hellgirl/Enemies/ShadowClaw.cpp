#include "Enemies/ShadowClaw.h"
#include "Fighter/ArenaFighter.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
AShadowClaw::AShadowClaw()
{
    PrimaryActorTick.bCanEverTick=true;
    Visual=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ShadowClaw")); RootComponent=Visual;
    Visual->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere")));
    Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision); Visual->SetRelativeScale3D(FVector(.65f,.25f,.6f));
    InitialLifeSpan=8.f;
}
void AShadowClaw::Tick(float Dt)
{
    Super::Tick(Dt);
    if (auto* Caster=Cast<AArenaFighter>(GetOwner()); !Caster || !Caster->IsAlive() || Caster->bStorySurrendered) { Destroy(); return; }
    if (!Cast<UMaterialInstanceDynamic>(Visual->GetMaterial(0)))
      if (auto* Mat=Visual->CreateAndSetMaterialInstanceDynamic(0)) Mat->SetVectorParameterValue(TEXT("Color"),FLinearColor(.45f,.02f,.75f));
    FCollisionQueryParams Q; Q.AddIgnoredActor(this);
    for (TActorIterator<AArenaFighter> It(GetWorld());It;++It) if (It->bEnemy) Q.AddIgnoredActor(*It);
    const FVector Next=GetActorLocation()+Velocity*Dt;
    FHitResult Hit;
    if (GetWorld()->SweepSingleByObjectType(Hit,GetActorLocation(),Next,FQuat::Identity,FCollisionObjectQueryParams(FCollisionObjectQueryParams::AllObjects),FCollisionShape::MakeSphere(30.f),Q))
    {
        if (auto* Player=Cast<AArenaFighter>(Hit.GetActor()); Player && !Player->bEnemy) Player->ReceiveHit(20.f,Velocity.GetSafeNormal(),100.f);
        Destroy(); return;
    }
    SetActorLocation(Next);
}
