#include "ArenaFighter.h"
#include "EnemySpawnPoint.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "ShadowClaw.h"
void AArenaFighter::RunBossDesignCheck()
{
#if WITH_DEV_AUTOMATION_TESTS
    if (bEnemy || !FParse::Param(FCommandLine::Get(),TEXT("HellgirlBossDesignCheck")) || GetWorld()->GetTimeSeconds()<1.f) return;
    static bool Done=false; if (Done) return; Done=true;
    bool Good=true;
    auto Check=[&](bool Pass,const TCHAR* Why) { if (!Pass) { Good=false; UE_LOG(LogTemp,Error,TEXT("BOSS DESIGN CHECK FAILED: %s"),Why); } };
    auto* Site=GetWorld()->SpawnActor<AEnemySpawnPoint>(FVector(4100,0,10),FRotator::ZeroRotator); Site->bBoss=Site->bActivated=Site->bEnabled=true;
    auto* Queen=GetWorld()->SpawnActor<AArenaFighter>(FVector(4100,0,160),FRotator::ZeroRotator);
    Queen->MakeEnemy(1,false); Queen->SetEnemyType(EHellgirlEnemyType::GoblinQueen); Queen->bBossEncounter=true; Queen->EncounterSite=Site; Queen->HomePosition=Site->GetActorLocation(); Queen->Health=Queen->MaxHealth=450.f;
    Check(Queen->GetMesh()->GetSkeletalMeshAsset()!=nullptr,TEXT("Queen native mesh"));
    Queen->Health-=Queen->FilterEnemyDamage(9999); Queen->UpdateQueenPhases(.1f);
    Check(Queen->bQueenHidden && Queen->GoblinPhase==1 && Queen->QueenFirstPack.Num()==3 && Queen->QueenGoblins.Num()==6,TEXT("70 percent: hidden and two packs"));
    Check(Queen->FilterEnemyDamage(100)==0,TEXT("Hidden queen invulnerable"));
    for (auto G:Queen->QueenSecondPack) if (G.IsValid()) G->Health=0;
    Queen->UpdateQueenPhases(3.f); Check(!Queen->bQueenHidden,TEXT("One pack defeated: returns"));
    Queen->Health-=Queen->FilterEnemyDamage(9999); Queen->UpdateQueenPhases(.1f);
    Check(Queen->GoblinPhase==2 && Queen->QueenGoblins.Num()==6,TEXT("30 percent: extra pack"));
    Queen->Health-=Queen->FilterEnemyDamage(9999); Queen->UpdateQueenPhases(.1f);
    Check(Queen->GoblinPhase==3 && Queen->bQueenHidden,TEXT("5 percent: final vanish"));
    for (int I=0;I<5 && Queen->bQueenHidden;++I) { for (auto G:Queen->QueenGoblins) if (G.IsValid()) G->Health=0; Queen->UpdateQueenPhases(4.1f); }
    Check(Queen->bQueenFinalReturned && Queen->Health==225.f,TEXT("Ten kills: return with half health"));
    Check(Queen->FilterEnemyDamage(9999)>Queen->Health,TEXT("Final return can be killed"));
    Queen->bBossEncounter=false; Queen->bQueenHidden=false; Check(Queen->FilterEnemyDamage(100)==100,TEXT("Future elite has no boss damage gate"));
    for (TActorIterator<AArenaFighter> It(GetWorld());It;++It) if (It->EnemyType==EHellgirlEnemyType::Goblins && It->bEnemy) It->Destroy();
    Queen->Destroy();
    auto* Boss=GetWorld()->SpawnActor<AArenaFighter>(FVector(4100,0,350),FRotator::ZeroRotator);
    if (!Boss) { Check(false,TEXT("Test boss spawn blocked")); FPlatformMisc::RequestExitWithStatus(false,1); return; }
    Boss->MakeEnemy(2,false); Boss->SetEnemyType(EHellgirlEnemyType::ImpCommander); Boss->bBossEncounter=true; Boss->EncounterSite=Site; Boss->HomePosition=Boss->GetActorLocation();
    Boss->SpawnCommanderImps(); Check(Boss->GetSummonedImpCount()==3,TEXT("Three protective imps"));
    Check(Boss->FilterEnemyDamage(100)==10,TEXT("90 percent shield"));
    for (auto Imp:Boss->CommanderImps) if (Imp.IsValid()) Imp->Health=0;
    Check(Boss->FilterEnemyDamage(100)==100,TEXT("Shield removed when imps dead"));
    Boss->SpawnCommanderImps(); Check(Boss->GetSummonedImpCount()==3,TEXT("Jump slam replenishes imps"));
    Boss->bBossEncounter=false; Check(Boss->FilterEnemyDamage(100)==100,TEXT("Future commander elite unshielded"));
    SetActorLocation(FVector(4100,0,5000)); Health=100.f; HitClock=DodgeClock=0.f;
    FActorSpawnParameters ProjectileParams; ProjectileParams.Owner=Boss;
    auto* Claw=GetWorld()->SpawnActor<AShadowClaw>(GetActorLocation()-FVector(300,0,0),FRotator::ZeroRotator,ProjectileParams);
    Claw->Velocity=FVector(500,0,0); Claw->Tick(1.f);
    Check(Health==80.f,TEXT("Shadow projectile swept hit on player capsule"));
    HitClock=0.f; DodgeClock=.25f;
    Claw=GetWorld()->SpawnActor<AShadowClaw>(GetActorLocation()-FVector(300,0,0),FRotator::ZeroRotator,ProjectileParams);
    Claw->Velocity=FVector(750,0,0); Claw->Tick(1.f);
    Check(Health==80.f,TEXT("Dodge avoids shadow claw"));
    if (Good) UE_LOG(LogTemp,Display,TEXT("BOSS DESIGN CHECK PASSED"));
    FPlatformMisc::RequestExitWithStatus(false,Good?0:1);
#endif
}
