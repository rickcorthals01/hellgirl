#include "Levels/ArenaGameMode.h"
#include "Fighter/ArenaFighter.h"
#include "Enemies/EnemySpawnPoint.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
void AArenaGameMode::RunCampaignCheck(float Dt)
{
#if WITH_DEV_AUTOMATION_TESTS
    if (!FParse::Param(FCommandLine::Get(),TEXT("HellgirlCampaignCheck"))) return;
    static int32 Level=0,Index=0; static float Time=0.f; static bool Layout=false;
    if (Level!=CampaignLevel) { Level=CampaignLevel; Index=0; Time=0; Layout=false; }
    Time+=Dt;
    auto Fail=[](const TCHAR* Why) { UE_LOG(LogTemp,Error,TEXT("CAMPAIGN CHECK FAILED: %s"),Why); FPlatformMisc::RequestExitWithStatus(false,1); };
    if (Time>75.f) { Fail(TEXT("Wave progression timed out")); return; }
    auto* Player=Cast<AArenaFighter>(UGameplayStatics::GetPlayerPawn(this,0)); if (!Player || Time<1.f) return;
    Player->Health=Player->MaxHealth=100000.f;
    if (!Layout)
    {
        if (MapNumber!=1 || SpawnSites.Num()!=(CampaignLevel==1?4:6) || SectionBarriers.Num()!=2) { Fail(TEXT("Castle layout missing")); return; }
        if (CampaignLevel==2 && !FMath::IsNearlyEqual(Player->Energy,57.f)) { Fail(TEXT("Energy lost during level travel")); return; }
        for (int32 I=0;I<SpawnSites.Num();++I)
        {
            const auto Expected=CampaignLevel==1 ? (I==SpawnSites.Num()-1 ? EHellgirlEnemyType::GoblinQueen:EHellgirlEnemyType::Goblins)
                : (I==SpawnSites.Num()-1 ? EHellgirlEnemyType::ImpCommander:EHellgirlEnemyType::Imps);
            if (SpawnSites[I]->GroundType!=Expected || (CampaignLevel==1 && SpawnSites[I]->FlyingCount!=0)) { Fail(TEXT("Wrong faction")); return; }
        }
        Layout=true;
    }
    if (Index<SpawnSites.Num())
    {
        auto* Site=SpawnSites[Index].Get();
        if (Site->bCleared) { ++Index; return; }
        Player->SetActorLocation(Site->GetActorLocation()+FVector(0,0,115));
        for (int32 I=Index+1;I<SpawnSites.Num();++I) if (SpawnSites[I]->bActivated) { Fail(TEXT("Future wave started early")); return; }
        if (Site->LivingEnemies() < Site->EnemyCount) return;
        for (TActorIterator<AArenaFighter> It(GetWorld());It;++It)
            if (It->bEnemy && It->IsAlive() && It->EncounterSite==Site)
            {
                if ((It->EnemyType==EHellgirlEnemyType::Goblins || It->EnemyType==EHellgirlEnemyType::GoblinQueen) && (!It->GetMesh()->GetSkeletalMeshAsset() || !It->GetMesh()->GetMaterial(0))) { Fail(TEXT("Goblin model/material missing")); return; }
                It->Health=0.f; // No wallet writes from tests.
            }
        return;
    }
    Player->SetActorLocation(ExitPosition+FVector(0,0,115));
    if (Prompt.IsEmpty()) return;
    if (CampaignLevel==1) { Player->Energy=57.f; AnswerPrompt(true); return; }
    AnswerPrompt(true);
    if (!bWon) { Fail(TEXT("Level 2 did not finish")); return; }
    UE_LOG(LogTemp,Display,TEXT("CAMPAIGN CHECK PASSED: both castle levels, factions, models, gated waves, bosses, travel and completion"));
    FPlatformMisc::RequestExitWithStatus(false,0);
#endif
}

