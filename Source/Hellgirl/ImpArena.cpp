#include "ArenaGameMode.h"
#include "ArenaFighter.h"
#include "EnemySpawnPoint.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Parse.h"

void AArenaGameMode::BuildImpArena()
{
    MapTitle=TEXT("STAGE 2 / IMP TORTURE ARENA");
    // One continuous, collision-bearing rock. The sea and cavern are scenery.
    RockPlatform(FVector::ZeroVector,FVector(10500.f,10500.f,1700.f),FLinearColor(.17f,.155f,.145f));
    MapPlatforms.Add(FVector4(0.f,0.f,10500.f,10500.f));
    for (int32 I=0;I<5;++I)
    {
        const float A=2.f*PI*I/5.f;
        const FVector P(2800.f*FMath::Cos(A),2800.f*FMath::Sin(A),10.f);
        Site(P,FString::Printf(TEXT("IMP WAVE %d / 5"),I+1),4+I, I%2 ? 1 : 0,false);
    }
    Site(FVector(0,2600,10),TEXT("IMP COMMANDER"),1,0,false,true);
    ExitPosition=FVector::ZeroVector; // Victory offers return to camp; no exit is built into the arena.
}

void AArenaGameMode::TickImpArena(float Dt)
{
    auto* Hero=Cast<AArenaFighter>(UGameplayStatics::GetPlayerPawn(this,0));
    if (!Hero || !Hero->IsAlive() || SpawnSites.Num()!=6) return;
    auto* PC=Cast<APlayerController>(Hero->GetController());
    ActivatedSites=ClearedSites=EnemiesRemaining=0;
    for (auto Site:SpawnSites)
    {
        ActivatedSites+=Site->bActivated;
        ClearedSites+=Site->bCleared;
        EnemiesRemaining+=Site->LivingEnemies();
    }
    int32 Next=0;
    while (Next<SpawnSites.Num() && SpawnSites[Next]->bCleared) ++Next;
    if (Next!=ArenaWaveIndex)
    {
        ArenaWaveIndex=Next;
        WaveCountdown=Next==0 ? 2.f : 4.f;
    }
    Prompt.Empty(); PromptAction=0;
    if (Next<SpawnSites.Num())
    {
        auto* Current=SpawnSites[Next].Get();
        if (!Current->bActivated)
        {
            WaveCountdown-=Dt;
            if (WaveCountdown<=0.f) { Current->bEnabled=true; Current->bActivated=true; }
        }
        Objective=Next<5
            ? FString::Printf(TEXT("IMP WAVE %d / 5 / %s"),Next+1,Current->bActivated?TEXT("Survive the attack"):TEXT("Incoming"))
            : Current->bActivated ? TEXT("Defeat the Imp Commander and his reinforcements") : TEXT("The Imp Commander approaches");
    }
    else
    {
        Objective=TEXT("IMP COMMANDER DEFEATED / Return to camp");
        if (GetUnlockedLevel()<5 && !FParse::Param(FCommandLine::Get(),TEXT("HellgirlImpArenaCheck")))
        {
            GConfig->SetInt(TEXT("HellgirlCampaign"),TEXT("UnlockedLevel"),5,GGameUserSettingsIni);
            GConfig->Flush(false,GGameUserSettingsIni);
        }
        PromptAction=4;
        Prompt=TEXT("Return to camp? E / D-pad Up: YES   N / D-pad Down: NO");
        if (PC && (PC->WasInputKeyJustPressed(EKeys::E)||PC->WasInputKeyJustPressed(EKeys::Gamepad_DPad_Up))) AnswerPrompt(true);
    }
    // Falling into the sea hurts, then returns the player to the same arena.
    HazardClock=FMath::Max(0.f,HazardClock-Dt);
    if (Hero->GetActorLocation().Z < -820.f && HazardClock<=0.f)
    {
        Hero->ApplyPhysicsDamage(20.f,Hero->GetVelocity());
        if (Hero->IsAlive())
        {
            Hero->SetActorLocation(FVector(0,0,115),false,nullptr,ETeleportType::TeleportPhysics);
            Hero->ResetAfterRecovery();
            Hero->MoveLabel=TEXT("LAVA / PULLED BACK INTO THE ARENA");
        }
        HazardClock=1.f;
    }
    for (TActorIterator<AArenaFighter> It(GetWorld()); It; ++It)
        if (It->bEnemy && It->IsAlive() && It->GetActorLocation().Z < -820.f)
        {
            It->SetActorLocation(It->HomePosition+FVector(0,0,115),false,nullptr,ETeleportType::TeleportPhysics);
            It->ResetAfterRecovery();
        }
    if (ExitPortal) ExitPortal->SetActorHiddenInGame(true);
}

void AArenaGameMode::RunImpArenaCheck()
{
#if WITH_DEV_AUTOMATION_TESTS
    if (!FParse::Param(FCommandLine::Get(),TEXT("HellgirlImpArenaCheck"))) return;
    static bool Done=false;
    if (Done) return;
    Done=true;
    bool Passed=IsImpArena() && MapPlatforms.Num()==1 && SpawnSites.Num()==6 && SectionGates.IsEmpty();
    int32 Lava=0,Roof=0,Chains=0,Cavern=0;
    for (TActorIterator<AActor> It(GetWorld());It;++It)
    {
        Lava+=It->ActorHasTag(TEXT("ImpArenaLava"));
        Roof+=It->ActorHasTag(TEXT("ImpArenaRoof"));
        Chains+=It->ActorHasTag(TEXT("ImpArenaChains"));
        Cavern+=It->ActorHasTag(TEXT("ImpArenaCavern"));
    }
    Passed &= Lava==1 && Roof==1 && Chains==1 && Cavern==32;
    FHitResult Floor;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(ImpArenaFloor),false);
    Passed &= GetWorld()->LineTraceSingleByChannel(Floor,FVector(0,0,700),FVector(0,0,-1600),ECC_Visibility,Query)
        && FMath::Abs(Floor.ImpactPoint.Z)<30.f && Floor.ImpactNormal.Z>.8f;
    for (int32 I=0;I<SpawnSites.Num();++I)
    {
        auto* Current=SpawnSites[I].Get();
        Passed &= Current->bBoss==(I==5);
        Passed &= Current->GroundType==(I==5?EHellgirlEnemyType::ImpCommander:EHellgirlEnemyType::Imps);
        if (I<5) Passed &= Current->FlyingCount==(I%2?1:0);
        TickImpArena(10.f);
        Passed &= Current->bActivated;
        for (int32 J=I+1;J<SpawnSites.Num();++J) Passed &= !SpawnSites[J]->bActivated;
        if (I==0 || I==5)
        {
            Current->Tick(1.f);
            bool Found=false;
            for (TActorIterator<AArenaFighter> It(GetWorld());It;++It)
                if (It->bEnemy && It->EncounterSite.Get()==Current && It->IsAlive())
                {
                    Found=true;
                    Passed &= It->EnemyType==(I==5?EHellgirlEnemyType::ImpCommander:EHellgirlEnemyType::Imps)
                        && It->GetMesh()->GetSkeletalMeshAsset()!=nullptr;
                    It->Health=0.f;
                    It->SetActorEnableCollision(false);
                }
            Passed &= Found;
        }
        Current->bCleared=true;
    }
    TickImpArena(0.f);
    Passed &= PromptAction==4 && ExitPortal && ExitPortal->IsHidden();
    if (Passed) { UE_LOG(LogTemp,Display,TEXT("IMP ARENA CHECK PASSED: one walkable platform, lava, chains, cavern, sequential waves, commander and no physical exit")); }
    else { UE_LOG(LogTemp,Error,TEXT("IMP ARENA CHECK FAILED")); }
    FPlatformMisc::RequestExitWithStatus(false,Passed?0:1);
#endif
}
