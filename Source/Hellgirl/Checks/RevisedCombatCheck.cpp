#include "Fighter/ArenaFighter.h"
#include "Rules/CombatEnergyRules.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Engine/World.h"
#include "Components/InputComponent.h"

void AArenaFighter::RunRevisedCombatCheck()
{
#if WITH_DEV_AUTOMATION_TESTS
    if (bEnemy || !FParse::Param(FCommandLine::Get(),TEXT("HellgirlRevisedCombatCheck")) || GetWorld()->GetTimeSeconds()<1.f) return;
    static bool Done=false; if (Done) return; Done=true;
    bool Passed=true;
    auto Check=[&](bool Result,const TCHAR* Why) { if (!Result) { Passed=false; UE_LOG(LogTemp,Error,TEXT("REVISED COMBAT CHECK FAILED: %s"),Why); } };
    const int32 OutfitBefore=SelectedOutfit;
    SetActorLocation(FVector(0,0,5000)); SetActorRotation(FRotator::ZeroRotator);
    if (Controller) Controller->SetControlRotation(FRotator::ZeroRotator);
    auto* Target=GetWorld()->SpawnActor<AArenaFighter>(FVector(150,0,5000),FRotator::ZeroRotator);
    Target->MakeEnemy(1,false); Target->Health=Target->MaxHealth=10000.f;
    auto Reset=[&]() { AttackClock=HitClock=DodgeClock=KnockdownClock=GroundDashClock=BufferClock=PostDodgeClock=0.f; bGroundImpactPending=false; CancelCharge(); GetCharacterMovement()->SetMovementMode(MOVE_Walking); Target->HitClock=0.f; Target->GetCharacterMovement()->StopMovementImmediately(); };
    Reset(); ComboClock=0.f; HeavyAttack(); ReleaseHeavyAttack();
    Check(CurrentAttack.Type==FistCombat::Move::RightHeavyKick && AttackClock>0.f,TEXT("Heavy press/release starts kick"));
    if (Controller && Controller->InputComponent)
        for (int32 I=0;I<Controller->InputComponent->GetNumActionBindings();++I)
        {
            const auto& Binding=Controller->InputComponent->GetActionBinding(I);
            if (Binding.GetActionName()==TEXT("Interact")) Check(!Binding.bConsumeInput,TEXT("Hub interaction must not swallow heavy input"));
        }
    Reset(); ActiveUltimate=0; UltimateClock=12.f; bCounterDodge=false;
    GetCharacterMovement()->SetMovementMode(MOVE_Falling);
    GetCharacterMovement()->Velocity=FVector(0,0,500); DodgeDirection=FVector::ForwardVector;
    StartDodgeMomentum();
    Check(GetVelocity().Size2D()<=1800.f && GetVelocity().Z==500.f,TEXT("Rags airborne dodge capped and jump arc preserved"));
    GetCharacterMovement()->Velocity=FVector(10000,0,500); StartDodgeMomentum();
    Check(GetVelocity().Size2D()<=1800.f,TEXT("Repeated aerial dodge caps inherited ultimate momentum"));
    UltimateClock=0.f; ResetPlayerMomentum();
    Reset(); Energy=100.f; Combo=3; ComboClock=1.f; bLastComboHeavy=false;
    StartAttack(false); GroundDashClock=0.f;
    const float Before=Target->Health; UpdateAttackTiming(CurrentAttack.Duration);
    Check(FMath::IsNearlyEqual(Before-Target->Health,24.f),TEXT("Double jab must land exactly twice on a long frame"));
    Reset(); Energy=0.f; Combo=3; ComboClock=1.f; bLastComboHeavy=true; StartAttack(true);
    Check(CurrentAttack.Type==FistCombat::Move::RightHeavyKick && Energy==0.f,TEXT("Free heavy fallback when sweep unaffordable"));
    Reset(); Combo=2; ComboClock=1.f; bLastComboHeavy=false; StartAttack(true);
    Check(CurrentAttack.NextCombo==1,TEXT("Switching from light to heavy starts new chain"));
    Reset(); Energy=12.5f; PendingCharge=1.f; StartAttack(true);
    Check(Energy==0.f,TEXT("Charge costs half a tube"));
    Reset(); bBlockHeld=true; Check(!IsBlocking(),TEXT("Block removed"));
    SelectedOutfit=0; Energy=99.f; Special(); Check(UltimateClock==0.f && Energy==99.f,TEXT("Ultimate requires full meter"));
    Energy=100.f; Special(); Check(Energy==0.f && UltimateClock==8.f && GetDashMultiplier()==6.f && GetSpeedMultiplier()==2.f,TEXT("Rags cost and buffs"));
    UpdateUltimate(8.f); Check(GetSpeedMultiplier()==1.f && GetDashMultiplier()==1.f,TEXT("Rags buffs expire"));
    SelectedOutfit=4; Energy=100.f; Special(); const float ClawsBefore=Target->Health;
    UpdateUltimate(6.f); Check(FMath::IsNearlyEqual(ClawsBefore-Target->Health,72.f),TEXT("Six claws pulses including a long frame"));
    SelectedOutfit=1; Energy=100.f; Special();
    Check(Target->ParalysisClock==3.f && Target->GetCharacterMovement()->MovementMode==MOVE_None,TEXT("Punishment freezes enemy movement"));
    Target->ReceiveHit(10.f,FVector::ForwardVector,800.f,1.f);
    Check(Target->ParalysisClock==3.f && Target->GetVelocity().IsNearlyZero(),TEXT("Damage does not release paralysis"));
    Target->Tick(3.f); Check(Target->ParalysisClock==0.f && Target->GetCharacterMovement()->MovementMode!=MOVE_None,TEXT("Paralysis restores movement"));
    UltimateClock=0.f; SelectedOutfit=2; Energy=100.f; Special(); Check(Energy==100.f && UltimateClock==0.f,TEXT("Undefined ultimate does not consume energy"));
    for (int32 Outfit : {0,1,4})
    {
        Reset(); Energy=0.f; ActiveUltimate=Outfit; UltimateClock=5.f;
        Combo=0; ComboClock=0.f; StartAttack(false); GroundDashClock=0.f;
        UpdateAttackTiming(CurrentAttack.Duration);
        Check(Energy==0.f,TEXT("Active ultimates block basic attack energy gain"));
    }
    UltimateClock=0.f; Reset(); Combo=0; ComboClock=0.f; StartAttack(false); GroundDashClock=0.f;
    UpdateAttackTiming(CurrentAttack.Duration);
    Check(Energy>0.f,TEXT("Energy gain resumes after ultimate expires"));
    SelectedOutfit=OutfitBefore; Target->Destroy();
    if (Passed) UE_LOG(LogTemp,Display,TEXT("REVISED COMBAT CHECK PASSED"));
    FPlatformMisc::RequestExitWithStatus(false,Passed?0:1);
#endif
}
