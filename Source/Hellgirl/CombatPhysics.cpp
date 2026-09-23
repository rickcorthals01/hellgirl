#include "ArenaFighter.h"
#include "CombatImpactBudget.h"
#include "CoinPickup.h"
#include "ArenaGameMode.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/TextRenderComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"

void AArenaFighter::ApplyCombatLaunch(const FVector& InVelocity, int32 ChainDepth, AArenaFighter* IgnoreEnemy, TSharedPtr<FCombatImpactBudget> ImpactBudget)
{
    if (!bEnemy || !IsAlive() || IsBossAttackArmored() || ParalysisClock > 0.f || bQueenHidden || bStorySurrendered) return;
    CancelEnemyMove();
    CombatLaunchImpactBudget = ImpactBudget;
    auto* Movement = GetCharacterMovement();
    if (!bCombatLaunched) LaunchAirControl = Movement->AirControl;
    bCombatLaunched = true;
    CombatLaunchClock = 2.5f;
    LaunchChainDepth = FMath::Clamp(ChainDepth,0,2);
    LaunchHitActors.Reset();
    QueuedCombatImpacts.Reset();
    if (IgnoreEnemy) LaunchHitActors.Add(IgnoreEnemy);
    bWorldImpactQueued = false;
    CancelCharge(); FinishAirMove();
    GroundDashClock = AttackClock = BufferClock = ComboClock = 0.f;
    bGroundImpactPending = false; bHitResolved = true;
    KnockdownClock = FMath::Max(KnockdownClock,.9f);
    ConsumeMovementInputVector();
    Movement->ClearAccumulatedForces();
    Movement->AirControl = 0.f;
    Movement->SetMovementMode(MOVE_Falling);
    FVector Launch = InVelocity;
    const FVector Horizontal = FVector(Launch.X,Launch.Y,0).GetClampedToMaxSize(1800.f);
    Launch.X=Horizontal.X; Launch.Y=Horizontal.Y; Launch.Z=FMath::Clamp(Launch.Z,-1800.f,650.f);
    LaunchCharacter(Launch,true,true);
}

void AArenaFighter::MoveBlockedBy(const FHitResult& Impact)
{
    Super::MoveBlockedBy(Impact);
    if (!bEnemy || !IsAlive() || !bCombatLaunched || Impact.bStartPenetrating || Impact.ImpactNormal.Z > .65f) return;
    AActor* OtherActor=Impact.GetActor();
    if (!OtherActor || LaunchHitActors.Contains(OtherActor) || QueuedCombatImpacts.Num()>=3) return;
    auto* Other=Cast<AArenaFighter>(OtherActor);
    if (Other && (!Other->bEnemy || !Other->IsAlive())) return;
    if (!Other && bWorldImpactQueued) return;
    const FVector Velocity=GetCharacterMovement()->Velocity;
    const FVector Relative=Velocity-(Other ? Other->GetVelocity() : FVector::ZeroVector);
    const float Closing=FMath::Max(0.f,-FVector::DotProduct(Relative,Impact.ImpactNormal));
    if (Closing<450.f) return;
    LaunchHitActors.Add(OtherActor);
    if (!Other) bWorldImpactQueued=true;
    QueuedCombatImpacts.Add({Impact,Velocity,Closing,LaunchChainDepth,CombatLaunchImpactBudget});
}

void AArenaFighter::ApplyPhysicsDamage(float Damage,const FVector& ImpulseVelocity)
{
    if (!IsAlive() || Damage<=0.f) return;
    Damage=FilterEnemyDamage(Damage);
    if (Damage<=0.f) return;
    if (IsBossAttackArmored() || ParalysisClock > 0.f)
    {
        Health=FMath::Max(0.f,Health-Damage);
        if (!IsAlive()) HandleDeath(ImpulseVelocity);
        return;
    }
    if (bEnemy) CancelEnemyMove();
    Health=FMath::Max(0.f,Health-Damage);
    CancelCharge(); FinishAirMove(); ResetPlayerMomentum();
    GroundDashClock=AttackClock=BufferClock=ComboClock=0.f;
    bGroundImpactPending=false; bHitResolved=true;
    PlayerHitAnimationTime=EnemyHitAnimationTime=0.f;
    HitClock=.12f;
    if (bEnemy) KnockdownClock=FMath::Max(KnockdownClock,.45f);
    if (!IsAlive()) HandleDeath(ImpulseVelocity);
}

void AArenaFighter::HandleDeath(const FVector& ImpulseVelocity)
{
    if (bDeathHandled || bStorySurrendered) return;
    if (bEnemy && bBossEncounter && EnemyType==EHellgirlEnemyType::GoblinQueen)
        if (auto* GM=Cast<AArenaGameMode>(UGameplayStatics::GetGameMode(this)); GM && GM->bStoryEnabled)
        {
            bStorySurrendered=true; Health=1.f; CancelEnemyMove(); CancelCharge(); ResetPlayerMomentum();
            bCombatLaunched=false; QueuedCombatImpacts.Reset(); AttackFlash->SetVisibility(false);
            GetCharacterMovement()->StopMovementImmediately(); GetCharacterMovement()->ClearAccumulatedForces(); GetCharacterMovement()->DisableMovement();
            SetActorTickEnabled(false); GM->QueenSurrendered(this); return;
        }
    if (bEnemy) CancelEnemyMove();
    bDeathHandled=true;
    ActiveAttackImpactBudget.Reset(); CombatLaunchImpactBudget.Reset();
    Health=0.f;
    CancelCharge(); FinishAirMove(); ResetPlayerMomentum();
    bBlockHeld=bSprintHeld=bWalkHeld=false;
    GroundDashClock=AttackClock=BufferClock=DodgeClock=ComboClock=0.f;
    Riposte=0.f; bGroundImpactPending=false; bHitResolved=true;
    Energy=0.f;
    bCombatLaunched=false; CombatLaunchClock=0.f; QueuedCombatImpacts.Reset();
    AttackFlash->SetVisibility(false);
    GetCharacterMovement()->ClearAccumulatedForces();
    if (bEnemy)
    {
        // Encounter credit and coins happen once, before the corpse is detached/simulated.
        if (auto* GM=Cast<AArenaGameMode>(UGameplayStatics::GetGameMode(this))) GM->EnemyDefeated(GetActorLocation());
        if (!bBossEncounter && FMath::RandRange(0,99)<5)
            if (auto* Heart=GetWorld()->SpawnActor<ACoinPickup>(GetActorLocation(),FRotator::ZeroRotator)) Heart->SetHeart();
        StartDeathRagdoll(ImpulseVelocity);
    }
    else
    {
        GetCharacterMovement()->StopMovementImmediately();
        Body->SetRelativeRotation(FRotator(0,0,90));
        Sword->SetVisibility(false);
    }
}

void AArenaFighter::UpdateCombatPhysics(float Dt)
{
    if (!IsAlive()) return;
    // Process after the movement sweep, so enabling ragdoll never changes a body mid-sweep.
    TArray<FQueuedCombatImpact> Impacts=MoveTemp(QueuedCombatImpacts);
    QueuedCombatImpacts.Reset();
    for (const auto& Impact : Impacts)
    {
        if (!IsAlive()) break;
        const float Damage=FMath::Clamp((Impact.ClosingSpeed-350.f)*.032f,8.f,35.f);
        auto* Other=Cast<AArenaFighter>(Impact.Hit.GetActor());
        if (Other)
        {
            if (!Other->IsAlive()) continue;
            const FVector Transfer=Impact.Velocity.GetSafeNormal2D()*FMath::Clamp(Impact.ClosingSpeed*.65f,400.f,1100.f)+FVector(0,0,280);
            const float OtherDamage = Impact.Budget.IsValid() ? Impact.Budget->Spend(Other,Damage) : Damage;
            Other->ApplyPhysicsDamage(OtherDamage,Transfer);
            if (Other->IsAlive() && Impact.ChainDepth<2)
                Other->ApplyCombatLaunch(Transfer,Impact.ChainDepth+1,this,Impact.Budget);
            const float SelfDamage = Impact.Budget.IsValid() ? Impact.Budget->Spend(this,Damage) : Damage;
            ApplyPhysicsDamage(SelfDamage,Impact.Velocity);
            if (IsAlive() && !IsBossAttackArmored())
                LaunchCharacter(Impact.Velocity*.35f+FVector(0,0,80),true,true);
        }
        else
        {
            const float SelfDamage = Impact.Budget.IsValid() ? Impact.Budget->Spend(this,Damage) : Damage;
            ApplyPhysicsDamage(SelfDamage,Impact.Velocity);
            if (IsAlive() && !IsBossAttackArmored())
            {
                bCombatLaunched=false; CombatLaunchClock=0;
                GetCharacterMovement()->AirControl=LaunchAirControl;
                LaunchCharacter(Impact.Hit.ImpactNormal*180.f+FVector(0,0,100),true,true);
                KnockdownClock=FMath::Max(KnockdownClock,.5f);
            }
        }
        // A shared area budget can be partly or fully spent; don't display
        // the uncapped raw damage as if it had been dealt again.
        DrawDebugString(GetWorld(),Impact.Hit.ImpactPoint+FVector(0,0,80),Impact.Budget.IsValid() ? TEXT("IMPACT")
            : FString::Printf(TEXT("IMPACT +%d"),FMath::RoundToInt(Damage)),nullptr,FColor(255,190,90),.65f,true);
    }
    if (bCombatLaunched)
    {
        CombatLaunchClock=FMath::Max(0.f,CombatLaunchClock-Dt);
        if (CombatLaunchClock<=0 || (GetCharacterMovement()->IsMovingOnGround() && CombatLaunchClock<2.3f))
        {
            bCombatLaunched=false;
            CombatLaunchImpactBudget.Reset();
            GetCharacterMovement()->AirControl=LaunchAirControl;
        }
    }
}

void AArenaFighter::ResetAfterRecovery()
{
    if (bEnemy) CancelEnemyMove();
    ActiveAttackImpactBudget.Reset(); CombatLaunchImpactBudget.Reset();
    bCombatLaunched=false; CombatLaunchClock=0;
    QueuedCombatImpacts.Reset(); LaunchHitActors.Reset(); bWorldImpactQueued=false;
    CancelCharge(); FinishAirMove(); ResetPlayerMomentum();
    GroundDashClock=AttackClock=DodgeClock=BufferClock=0.f;
    bGroundImpactPending=false; bHitResolved=true;
    GetCharacterMovement()->AirControl=bEnemy ? LaunchAirControl : GetClass()->GetDefaultObject<AArenaFighter>()->GetCharacterMovement()->AirControl;
    GetCharacterMovement()->ClearAccumulatedForces();
    GetCharacterMovement()->StopMovementImmediately();
}
