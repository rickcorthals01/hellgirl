#include "ArenaGameMode.h"
#include "ArenaFighter.h"
#include "EnemySpawnPoint.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/StaticMeshActor.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "EngineUtils.h"
#include "Engine/World.h"

void AArenaGameMode::TickGoblinPrelude(float Dt)
{
    auto* Hero=Cast<AArenaFighter>(UGameplayStatics::GetPlayerPawn(this,0));
    if (!Hero || !Hero->IsAlive() || SpawnSites.Num()!=5) return;
    auto* PC=Cast<APlayerController>(Hero->GetController());
    ActivatedSites=ClearedSites=EnemiesRemaining=0;
    for (auto Site:SpawnSites) { ActivatedSites+=Site->bActivated; ClearedSites+=Site->bCleared; EnemiesRemaining+=Site->LivingEnemies(); }
    int32 Next=0; while (Next<5 && SpawnSites[Next]->bCleared) ++Next;
    if (Next!=ArenaWaveIndex) { ArenaWaveIndex=Next; WaveCountdown=Next==0?2.f:4.f; }
    for (int32 I=0;I<2;++I)
    {
        const bool Open=CampaignLevel==1 && Next>=(I==0?1:3);
        SectionGates[I]->SetActorHiddenInGame(Open); SectionGates[I]->SetActorEnableCollision(!Open); SectionBarriers[I]->SetActorEnableCollision(!Open);
    }
    if (Next<5)
    {
        const float X=Hero->GetActorLocation().X;
        const bool Entered=CampaignLevel==2 || Next==0 || (Next<3?X>-3200.f:X>3500.f);
        if (Entered && !SpawnSites[Next]->bActivated)
        {
            WaveCountdown-=Dt;
            if (WaveCountdown<=0.f) { SpawnSites[Next]->bEnabled=true; SpawnSites[Next]->bActivated=true; }
        }
        Objective=Entered ? FString::Printf(TEXT("WAVE %d / 5 — %s"),Next+1,SpawnSites[Next]->bActivated?TEXT("Defeat the goblins"):TEXT("Incoming")) : TEXT("Gate opened! Move to the next section");
    }
    else
    {
        Objective=TEXT("LEVEL COMPLETE / Enter the purple portal to return to camp");
        if (GetUnlockedLevel()<CampaignLevel+1 && !FParse::Param(FCommandLine::Get(),TEXT("HellgirlGoblinStageCheck")))
        { GConfig->SetInt(TEXT("HellgirlCampaign"),TEXT("UnlockedLevel"),CampaignLevel+1,GGameUserSettingsIni); GConfig->Flush(false,GGameUserSettingsIni); }
    }
    ExitPortal->SetActorHiddenInGame(Next<5);
    if (BossOrb) BossOrb->SetActorHiddenInGame(true);
    const bool NearExit=Next==5 && FVector::Dist2D(Hero->GetActorLocation(),ExitPosition)<350.f;
    Prompt.Empty(); PromptAction=0;
    if (!NearExit) bDeclined=false;
    if (NearExit && !bDeclined)
    {
        PromptAction=4; Prompt=TEXT("Return to camp? E / D-pad Up: YES   N / D-pad Down: NO");
        if (PC && (PC->WasInputKeyJustPressed(EKeys::N)||PC->WasInputKeyJustPressed(EKeys::Gamepad_DPad_Down))) AnswerPrompt(false);
        else if (PC && (PC->WasInputKeyJustPressed(EKeys::E)||PC->WasInputKeyJustPressed(EKeys::Gamepad_DPad_Up))) AnswerPrompt(true);
    }
}

void AArenaGameMode::RunGoblinStageCheck()
{
#if WITH_DEV_AUTOMATION_TESTS
    if (!FParse::Param(FCommandLine::Get(),TEXT("HellgirlGoblinStageCheck"))) return;
    auto* Hero=Cast<AArenaFighter>(UGameplayStatics::GetPlayerPawn(this,0));
    if (!Hero) return;
    static int32 CheckedLevel=0;
    if (CheckedLevel==CampaignLevel) return;
    CheckedLevel=CampaignLevel;
    bool Passed=SpawnSites.Num()==(CampaignLevel<=2?5:CampaignLevel==3?4:6);
    if (CampaignLevel==2) Passed &= FMath::Abs(Hero->GetActorLocation().X)<100.f;
    for (int32 I=0;I<SpawnSites.Num();++I)
    {
        auto* Site=SpawnSites[I].Get();
        const bool Boss=CampaignLevel>=3 && I==SpawnSites.Num()-1;
        Passed &= Site->bBoss==Boss;
        Passed &= Site->GroundType==(CampaignLevel<=3?(Boss?EHellgirlEnemyType::GoblinQueen:EHellgirlEnemyType::Goblins):(Boss?EHellgirlEnemyType::ImpCommander:EHellgirlEnemyType::Imps));
        if (CampaignLevel==2) Passed &= Site->EnemyCount==4+I*2;
    }
    if (CampaignLevel<=2)
    {
        Hero->SetActorLocation(FVector(CampaignLevel==1?4500.f:0.f,0,115));
        for (int32 I=0;I<5;++I)
        {
            TickGoblinPrelude(10.f);
            Passed &= SpawnSites[I]->bActivated;
            for (int32 J=I+1;J<5;++J) Passed &= !SpawnSites[J]->bActivated;
            SpawnSites[I]->bCleared=true;
        }
        TickGoblinPrelude(0.f);
        Passed &= !ExitPortal->IsHidden() && ClearedSites==5;
    }
    if (!Passed) { UE_LOG(LogTemp,Error,TEXT("GOBLIN STAGE CHECK FAILED at %d"),CampaignLevel); FPlatformMisc::RequestExitWithStatus(false,1); return; }
    if (CampaignLevel<4) { TravelToCampaign(CampaignLevel+1); return; }
    UE_LOG(LogTemp,Display,TEXT("GOBLIN STAGE CHECK PASSED: all four destinations, factions, boss placement, survival counts and center spawn"));
    FPlatformMisc::RequestExitWithStatus(false,0);
#endif
}
