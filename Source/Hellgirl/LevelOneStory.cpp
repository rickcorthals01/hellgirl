#include "ArenaGameMode.h"
#include "ArenaFighter.h"
#include "HellgirlPlayerController.h"
#include "EnemySpawnPoint.h"
#include "Kismet/GameplayStatics.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
void AArenaGameMode::QueueStory(FName Moment)
{
    if (bStoryEnabled && !PlayedStory.Contains(Moment)) PendingStory.AddUnique(Moment);
}
void AArenaGameMode::QueenSurrendered(AArenaFighter* Queen)
{
    SurrenderedQueen=Queen;
    QueueStory(TEXT("QueenDefeat"));
}
void AArenaGameMode::StoryFinished(FName Moment)
{
    if (Moment==TEXT("QueenDefeat") && SurrenderedQueen.IsValid())
    {
        QueenFleeClock=2.5f;
        auto* Queen=SurrenderedQueen.Get();
        const auto& Model=GetDefault<UHellgirlEnemyModels>()->ForType(EHellgirlEnemyType::GoblinQueen);
        Queen->GetMesh()->PlayAnimation(Model.Move.LoadSynchronous(),true);
        Queen->SetActorEnableCollision(false);
    }
}
bool AArenaGameMode::TickStory(float Dt)
{
    auto* PC=Cast<AHellgirlPlayerController>(UGameplayStatics::GetPlayerController(this,0));
    auto* Player=Cast<AArenaFighter>(UGameplayStatics::GetPlayerPawn(this,0));
    if (!PC || !Player || !Player->IsAlive()) return false;
    if (QueenFleeClock>0.f && SurrenderedQueen.IsValid())
    {
        auto* Queen=SurrenderedQueen.Get();
        QueenFleeClock=FMath::Max(0.f,QueenFleeClock-Dt);
        const FVector Away=FVector(0,1,0);
        Queen->SetActorRotation(Away.Rotation()); Queen->AddActorWorldOffset(Away*480.f*Dt);
        if (QueenFleeClock<=0.f)
        {
            if (!FParse::Param(FCommandLine::Get(),TEXT("HellgirlStoryCheck"))) EnemyDefeated(Queen->GetActorLocation());
            Queen->Health=0.f; Queen->Destroy(); SurrenderedQueen.Reset();
        }
    }
    if (PC->IsPauseMenuOpen()) return true;
    QueueStory(TEXT("Opening"));
    if (!SpawnSites.IsEmpty() && SpawnSites[0]->bCleared) QueueStory(TEXT("AfterFirstWave"));
    bool AllWavesDone=SpawnSites.Num()>1;
    for (int32 I=0;I<SpawnSites.Num()-1;++I) AllWavesDone &= SpawnSites[I]->bCleared;
    if (CampaignLevel==3 && AllWavesDone && Player->GetActorLocation().X>3500.f) QueueStory(TEXT("BossEntrance"));
    if (!PendingStory.IsEmpty())
    {
        const FName Moment=PendingStory[0];
        if (PC->ShowConversation(Moment))
        {
            PlayedStory.Add(Moment); PendingStory.RemoveAt(0); return true;
        }
        UE_LOG(LogTemp,Error,TEXT("Unable to load story conversation %s"),*Moment.ToString());
        PlayedStory.Add(Moment); PendingStory.RemoveAt(0); // Do not deadlock gameplay if a data file is missing.
    }
    return false;
}
