#include "Levels/ArenaGameMode.h"
#include "Fighter/ArenaFighter.h"
#include "UI/HellgirlPlayerController.h"
#include "Enemies/EnemySpawnPoint.h"
#include "Kismet/GameplayStatics.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Progress/CampaignProgress.h"
#include "GameFramework/InputSettings.h"
#include "HAL/PlatformTime.h"

// Stage 1 (Goblins, level 1) follows "Dialog START GAME - Goblins Stage 1.txt":
//   darkness ("... Wake, subject.") -> fade in, Hellgirl on the floor ("Ugh... Where am I?") -> she gets up and walks,
//   no enemies -> a voice ("Prove you're worthy.") -> waves 1-2 -> the Goblin Queen's outburst -> waves 3-5
//   -> "I gotta get out of here." -> at the portal "Let's find a save spot and rest.." -> camp.
// Stages 2 and 3 queue their conversations from their wave scripts (Levels/GoblinWaves.cpp). The Queen adds three:
//   QueenLowHealth at 30% (ultimates unlock here), QueenDefeat when she begs, then L3_Escaped once she has fled.
namespace
{
constexpr float VoiceDistance = 450.f; // how far she walks from where she woke before the voice speaks
}

void AArenaGameMode::QueueStory(FName Moment)
{
    if (bStoryEnabled && !PlayedStory.Contains(Moment)) PendingStory.AddUnique(Moment);
}
void AArenaGameMode::QueenSurrendered(AArenaFighter* Queen)
{
    SurrenderedQueen=Queen;
    QueueStory(TEXT("QueenDefeat"));
}
bool AArenaGameMode::HasPlayedStory(FName Moment) const { return PlayedStory.Contains(Moment); }
void AArenaGameMode::UseExitPortal()
{
    // Stage 1: one last line before leaving for camp.
    if (bStoryEnabled && CampaignLevel==1 && !PlayedStory.Contains(TEXT("L1_Portal"))) { QueueStory(TEXT("L1_Portal")); return; }
    CompleteLevel();
    if (!FString(FCommandLine::Get()).Contains(TEXT("-Hellgirl"))) TravelToHub();
}
void AArenaGameMode::StoryFinished(FName Moment)
{
    auto* PC=Cast<AHellgirlPlayerController>(UGameplayStatics::GetPlayerController(this,0));
    auto* Player=Cast<AArenaFighter>(UGameplayStatics::GetPlayerPawn(this,0));
    if (Moment==TEXT("L1_Darkness"))
    {
        if (PC) PC->FadeFromBlack(1.6f);
        QueueStory(TEXT("L1_Wake"));
    }
    else if (Moment==TEXT("L1_Wake") && Player) Player->ReleaseWakeUp();
    // 01.5: the camp line is spoken on black; camp fades in after it.
    else if (Moment==TEXT("C_SetUpCamp") && PC) PC->FadeFromBlack(1.5f);
    else if (Moment==TEXT("L1_Portal") && !FParse::Param(FCommandLine::Get(),TEXT("HellgirlStoryCheck"))) { CompleteLevel(); TravelToHub(); }
    else if (Moment==TEXT("QueenLowHealth") && CampaignLevel==3)
    {
        // "I'll show you!": her ultimate awakens, with the energy bar already full.
        HellgirlProgress::SetFlag(TEXT("UltimatesUnlocked"));
        if (Player) Player->Energy=Player->MaxEnergy;
        FString Keys;
        for (const FInputActionKeyMapping& Mapping : GetDefault<UInputSettings>()->GetActionMappings())
            if (Mapping.ActionName==TEXT("Special")) Keys+=(Keys.IsEmpty()?TEXT(""):TEXT(" / "))+Mapping.Key.GetDisplayName().ToString();
        Tip=FString::Printf(TEXT("ULTIMATE UNLOCKED  ·  Press %s to unleash your ultimate after filling up the energy bar"),Keys.IsEmpty()?TEXT("Q"):*Keys);
        TipUntil=FPlatformTime::Seconds()+10.0;
    }
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
            QueueStory(TEXT("L3_Escaped"));
        }
    }
    if (PC->IsPauseMenuOpen()) return true;
    if (CampaignLevel==1)
    {
        if (!PlayedStory.Contains(TEXT("L1_Darkness")) && !PendingStory.Contains(TEXT("L1_Darkness")))
        {
            // She starts on the floor and stays there through the opening lines.
            Player->BeginWakeUp();
            WokeAt=Player->GetActorLocation();
            QueueStory(TEXT("L1_Darkness"));
        }
        if (PlayedStory.Contains(TEXT("L1_Wake")) && !Player->IsWakingUp()
            && FVector::Dist2D(Player->GetActorLocation(),WokeAt)>VoiceDistance) QueueStory(TEXT("L1_Voice"));
        if (SpawnSites.Num()>=5)
        {
            if (SpawnSites[1]->bCleared) QueueStory(TEXT("L1_AfterWave2"));
            if (SpawnSites[4]->bCleared) QueueStory(TEXT("L1_AfterWave5"));
        }
    }
    if (!PendingStory.IsEmpty())
    {
        const FName Moment=PendingStory[0];
        if (PC->ShowConversation(Moment))
        {
            PlayedStory.Add(Moment); PendingStory.RemoveAt(0); return true;
        }
        UE_LOG(LogTemp,Error,TEXT("Unable to load story conversation %s"),*Moment.ToString());
        PlayedStory.Add(Moment); PendingStory.RemoveAt(0); // Do not deadlock gameplay if a data file is missing.
        StoryFinished(Moment);
    }
    return false;
}
