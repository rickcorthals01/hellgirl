#include "UI/HellgirlPlayerController.h"
#include "Levels/ArenaGameMode.h"
#include "Fighter/ArenaFighter.h"
#include "Enemies/EnemySpawnPoint.h"
#include "Containers/Ticker.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/PlatformTime.h"
void AHellgirlPlayerController::RunStoryCheck()
{
#if WITH_DEV_AUTOMATION_TESTS
    if (!FParse::Param(FCommandLine::Get(),TEXT("HellgirlStoryCheck"))) return;
    struct FState { int32 Phase=0,Wave=0,Pages=0; TArray<FName> Seen; TWeakObjectPtr<AArenaFighter> Queen; double Start=FPlatformTime::Seconds(); };
    auto State=MakeShared<FState>(); TWeakObjectPtr<AHellgirlPlayerController> Weak(this);
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Weak,State](float) {
        if (!Weak.IsValid()) return false;
        auto* PC=Weak.Get(); auto* GM=Cast<AArenaGameMode>(UGameplayStatics::GetGameMode(PC)); auto* Hero=Cast<AArenaFighter>(PC->GetPawn());
        if (!GM || !Hero) return true;
        auto Fail=[](const TCHAR* Why) { UE_LOG(LogTemp,Error,TEXT("STORY CHECK FAILED: %s"),Why); FPlatformMisc::RequestExitWithStatus(false,1); };
        if (FPlatformTime::Seconds()-State->Start>60) { Fail(TEXT("Story test timed out")); return false; }
        Hero->Health=Hero->MaxHealth=100000.f;
        if (PC->IsDialogueOpen())
        {
            const FName Id=PC->GetConversation();
            if (State->Seen.IsEmpty() || State->Seen.Last()!=Id) State->Seen.Add(Id);
            if (Id==TEXT("Opening") && GM->GetSpawnSites()[0]->bActivated) { Fail(TEXT("Wave started during introduction")); return false; }
            ++State->Pages; PC->ContinueDialogue(); return true;
        }
        const auto& Sites=GM->GetSpawnSites(); if (Sites.Num()!=4) { Fail(TEXT("Level 1 must have three waves plus boss")); return false; }
        if (State->Wave<3)
        {
            auto* Site=Sites[State->Wave].Get();
            Hero->SetActorLocation(Site->GetActorLocation()+FVector(0,0,115));
            if (Site->bCleared) { ++State->Wave; return true; }
            // Clear the wave as its enemies arrive (waves are scaled up, see Rules/EnemyTuning.h).
            if (Site->LivingEnemies()==0) return true;
            for (TActorIterator<AArenaFighter> It(PC->GetWorld());It;++It) if (It->bEnemy && It->EncounterSite==Site) It->Health=0;
            return true;
        }
        Hero->SetActorLocation(FVector(4050,0,115));
        if (!State->Queen.IsValid())
            for (TActorIterator<AArenaFighter> It(PC->GetWorld());It;++It)
                if (It->bEnemy && It->EnemyType==EHellgirlEnemyType::GoblinQueen) { State->Queen=*It; break; }
        if (State->Queen.IsValid())
        {
            auto* Queen=State->Queen.Get();
            if (!Queen->bStorySurrendered)
            {
                // Exercise real phase triggers while bypassing attack timing only.
                if (!Queen->IsBossHidden()) Queen->ApplyPhysicsDamage(100000.f,FVector::ZeroVector);
                for (TActorIterator<AArenaFighter> It(PC->GetWorld());It;++It)
                    if (It->bEnemy && *It!=Queen) It->Health=0;
            }
        }
        if (Sites.Last()->bCleared)
        {
            const TArray<FName> Expected={TEXT("Opening"),TEXT("AfterFirstWave"),TEXT("BossEntrance"),TEXT("QueenLowHealth"),TEXT("QueenReturn"),TEXT("QueenDefeat")};
            if (State->Seen!=Expected || State->Pages!=24) { Fail(TEXT("Wrong dialogue order or page count")); return false; }
            if (State->Queen.IsValid()) { Fail(TEXT("Queen did not flee")); return false; }
            UE_LOG(LogTemp,Display,TEXT("STORY CHECK PASSED: 24 pages in order, three waves, queen phases, surrender and escape"));
            FPlatformMisc::RequestExitWithStatus(false,0); return false;
        }
        return true;
    }));
#endif
}
