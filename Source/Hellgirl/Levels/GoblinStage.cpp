#include "Rules/EnemyTuning.h"
#include "Levels/ArenaGameMode.h"
#include "Fighter/ArenaFighter.h"
#include "Enemies/EnemySpawnPoint.h"
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
    ActivatedSites=ClearedSites=EnemiesRemaining=0;
    for (auto Site:SpawnSites) { ActivatedSites+=Site->bActivated; ClearedSites+=Site->bCleared; EnemiesRemaining+=Site->LivingEnemies(); }
    int32 Next=0; while (Next<5 && SpawnSites[Next]->bCleared) ++Next;
    if (Next!=ArenaWaveIndex) { ArenaWaveIndex=Next; WaveCountdown=Next==0?2.f:EnemyTuning::WaveBreak(4.f); }
    for (int32 I=0;I<2;++I)
    {
        const bool Open=CampaignLevel==1 && Next>=(I==0?1:3);
        SectionGates[I]->SetActorHiddenInGame(Open); SectionGates[I]->SetActorEnableCollision(!Open); SectionBarriers[I]->SetActorEnableCollision(!Open);
    }
    if (Next<5)
    {
        const float X=Hero->GetActorLocation().X;
        bool Entered=CampaignLevel==2 || Next==0 || (Next<3?X>-3200.f:X>3500.f);
        // Stage 1's story: no enemies until the voice has spoken.
        const bool Waiting=bStoryEnabled && CampaignLevel==1 && Next==0 && !HasPlayedStory(TEXT("L1_Voice"));
        if (Waiting) Entered=false;
        if (Entered && !SpawnSites[Next]->bActivated)
        {
            WaveCountdown-=Dt;
            if (WaveCountdown<=0.f) { SpawnSites[Next]->bEnabled=true; SpawnSites[Next]->bActivated=true; }
        }
        Objective=Waiting ? TEXT("Look around") : Entered ? FString::Printf(TEXT("WAVE %d / 5 — %s"),Next+1,SpawnSites[Next]->bActivated?TEXT("Defeat the goblins"):TEXT("Incoming")) : TEXT("Gate opened! Move to the next section");
    }
    else
    {
        Objective=TEXT("LEVEL COMPLETE / Enter the portal to return to camp");
        CompleteLevel();
    }
    ShowExitPortal(Next>=5);
    if (BossOrb) BossOrb->SetActorHiddenInGame(true);
    Prompt.Empty(); PromptAction=0;
    TickPortalMenus(Hero);
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
    // Stage 1: five waves; Stage 2: six waves and the two-pack army; Stage 3: fourteen waves and the Queen.
    bool Passed=SpawnSites.Num()==(CampaignLevel==1?5:CampaignLevel==2?8:CampaignLevel==3?15:6);
    if (CampaignLevel==2) Passed &= FMath::Abs(Hero->GetActorLocation().X)<100.f;
    for (int32 I=0;I<SpawnSites.Num();++I)
    {
        auto* Site=SpawnSites[I].Get();
        const bool Boss=CampaignLevel>=3 && I==SpawnSites.Num()-1;
        Passed &= Site->bBoss==Boss;
        Passed &= Site->GroundType==(CampaignLevel<=3?(Boss?EHellgirlEnemyType::GoblinQueen:EHellgirlEnemyType::Goblins):(Boss?EHellgirlEnemyType::ImpCommander:EHellgirlEnemyType::Imps));
    }
    if (CampaignLevel==1)
    {
        Hero->SetActorLocation(FVector(4500.f,0,115));
        for (int32 I=0;I<5;++I)
        {
            TickGoblinPrelude(10.f);
            Passed &= SpawnSites[I]->bActivated;
            for (int32 J=I+1;J<5;++J) Passed &= !SpawnSites[J]->bActivated;
            SpawnSites[I]->bCleared=true;
        }
        TickGoblinPrelude(0.f);
        Passed &= IsExitOpen() && ClearedSites==5;
    }
    else if (CampaignLevel<=3)
    {
        // Walk the script: one wave at a time (the army's two packs together), a soul portal where the script
        // puts one, and each gate only once its portal has been passed.
        int32 Waves=0,Portals=0;
        for (int32 Guard=0;Guard<80 && !IsExitOpen();++Guard)
        {
            TickGoblinWaves(10.f);
            if (IsSoulPortalOpen()) { ++Portals; ChoosePortal(EPortalChoice::Continue); continue; }
            int32 Started=0;
            for (auto Site:SpawnSites) if (Site->bActivated && !Site->bCleared) { Site->bCleared=true; ++Started; }
            Waves+=Started>0;
            Passed &= Started<=(CampaignLevel==2 && Waves==7 ? 2 : 1);
            // Nothing started: go to where the next wave waits (through the gate that just opened).
            if (!Started) for (auto Site:SpawnSites) if (!Site->bActivated) { Hero->SetActorLocation(Site->GetActorLocation()+FVector(0,0,115)); break; }
        }
        TickGoblinWaves(0.f);
        Passed &= IsExitOpen() && ClearedSites==SpawnSites.Num() && Waves==(CampaignLevel==2?7:15) && Portals==(CampaignLevel==2?2:3);
        if (!Passed) UE_LOG(LogTemp,Error,TEXT("Stage %d script: %d waves, %d portals, exit %d"),CampaignLevel,Waves,Portals,IsExitOpen());
    }
    if (!Passed) { UE_LOG(LogTemp,Error,TEXT("GOBLIN STAGE CHECK FAILED at %d"),CampaignLevel); FPlatformMisc::RequestExitWithStatus(false,1); return; }
    if (CampaignLevel<4) { TravelToCampaign(CampaignLevel+1); return; }
    UE_LOG(LogTemp,Display,TEXT("GOBLIN STAGE CHECK PASSED: all four destinations, factions, boss placement, Stage 2/3 wave scripts with soul portals and gates, centre spawn"));
    FPlatformMisc::RequestExitWithStatus(false,0);
#endif
}
