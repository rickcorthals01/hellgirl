#include "Progress/HellgirlWallet.h"
#include "Levels/ArenaGameMode.h"
#include "Fighter/ArenaFighter.h"
#include "Enemies/EnemySpawnPoint.h"
#include "Progress/CoinPickup.h"
#include "UI/HellgirlPlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/ConfigCacheIni.h"
#include "Progress/CampaignProgress.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace
{
bool IsSaveCheck() { return FParse::Param(FCommandLine::Get(),TEXT("HellgirlSaveCheck")); }
FString SlotName(int32 Slot) { return FString(IsSaveCheck()?TEXT("HellgirlTestSlot_"):TEXT("HellgirlSlot_"))+FString::FromInt(Slot); }
bool ValidSave(UHellgirlGameSave* Save)
{
    if (Save && Save->Version==1)
    {
        Save->Level=Save->Level==1?3:4;
        Save->Unlocked=Save->Unlocked>=2?Save->Unlocked+2:3;
        Save->Version=2;
    }
    return Save && Save->Version==2 && Save->Level>=1 && Save->Level<=4 && Save->Unlocked>=1 && Save->Unlocked<=5
        && Save->Coins>=0 && Save->Outfit>=0 && Save->Outfit<7 && !Save->Location.ContainsNaN()
        && !Save->Facing.ContainsNaN() && !Save->Camera.ContainsNaN() && FMath::IsFinite(Save->Health) && Save->Health>0.f
        && FMath::IsFinite(Save->Energy) && FMath::IsFinite(Save->Stamina)
        && Save->PickupLocations.Num()==Save->PickupAmounts.Num()
        && Save->Carried>=0
        // Stages 2 and 3 were rebuilt as scripted waves; older saves there restore whatever still matches.
        && (Save->bHub ? Save->Cleared.IsEmpty() : Save->Level==1 ? Save->Cleared.Num()==5 : Save->Level==4 ? Save->Cleared.Num()==6 : Save->Cleared.Num()<=32);
}
}

FString UHellgirlWallet::SaveSlot(int32 Slot)
{
    auto* GM=Cast<AArenaGameMode>(UGameplayStatics::GetGameMode(this));
    auto* Hero=Cast<AArenaFighter>(UGameplayStatics::GetPlayerPawn(this,0));
    auto* PC=Cast<AHellgirlPlayerController>(UGameplayStatics::GetPlayerController(this,0));
    if (Slot<1 || Slot>3 || !GM || !Hero || !Hero->IsAlive() || bLoadFailed || GM->bLegacyMap) return TEXT("Cannot save here.");
    if (GM->bEndless || GM->bForestRun) return TEXT("Runs cannot be saved.");
    if (Hero->GetUltimateTime()>0.f || !Hero->GetCharacterMovement()->IsMovingOnGround() || (PC && PC->IsDialogueOpen())
        || GM->SurrenderedQueen.IsValid() || !GM->PendingStory.IsEmpty()) return TEXT("Finish the current action before saving.");
    for (auto Site:GM->GetSpawnSites()) if (Site->bActivated && !Site->bCleared) return TEXT("Finish this wave before saving.");
    auto* Save=Cast<UHellgirlGameSave>(UGameplayStatics::CreateSaveGameObject(UHellgirlGameSave::StaticClass()));
    if (!Save) return TEXT("Could not create save.");
    Save->Coins=Coins; Save->Carried=Carried; Save->bGoblinQueenOwned=bGoblinQueenOwned;
    for (const FString& Flag : HellgirlProgress::AllFlags()) if (HellgirlProgress::Flag(*Flag)) Save->Flags.Add(Flag);
    Save->Level=GM->CampaignLevel; Save->Unlocked=GM->GetUnlockedLevel(); Save->bHub=GM->bForestHub;
    Save->ImpArenaLayoutVersion=GM->CampaignLevel==4 && !GM->bForestHub ? 1 : 0;
    Save->ForestHubLayoutVersion=GM->bForestHub ? 1 : 0;
    Save->Outfit=Hero->GetOutfit(); Save->Location=Hero->GetActorLocation(); Save->Facing=Hero->GetActorRotation();
    Save->Camera=PC?PC->GetControlRotation():FRotator::ZeroRotator;
    Save->Health=Hero->Health; Save->Energy=Hero->Energy; Save->Stamina=Hero->Stamina;
    for (auto Site:GM->GetSpawnSites()) Save->Cleared.Add(Site->bCleared);
    Save->Story=GM->PlayedStory.Array(); Save->Date=FDateTime::Now().ToString(TEXT("%Y-%m-%d %H:%M"));
    for (TActorIterator<ACoinPickup> It(GetWorld());It;++It) if (!It->bCollected)
    { Save->PickupLocations.Add(It->GetActorLocation()); Save->PickupAmounts.Add(It->bHeart?-1:It->Amount); }
    return UGameplayStatics::SaveGameToSlot(Save,SlotName(Slot),0)?TEXT("Game saved."):TEXT("Save failed. Try again.");
}

FString UHellgirlWallet::DescribeSlot(int32 Slot) const
{
    if (!UGameplayStatics::DoesSaveGameExist(SlotName(Slot),0)) return TEXT("Empty");
    auto* Save=Cast<UHellgirlGameSave>(UGameplayStatics::LoadGameFromSlot(SlotName(Slot),0));
    if (!ValidSave(Save)) return TEXT("Unreadable or incompatible save");
    return FString::Printf(TEXT("%s / %lld souls / %s"),Save->bHub?TEXT("Forest camp"):*FString::Printf(TEXT("Stage %d - Level %d"),Save->Level<=3?1:2,Save->Level<=3?Save->Level:1),Save->Coins,*Save->Date);
}

bool UHellgirlWallet::LoadSlot(int32 Slot)
{
    if (Slot<1 || Slot>3) return false;
    auto* Save=Cast<UHellgirlGameSave>(UGameplayStatics::LoadGameFromSlot(SlotName(Slot),0));
    if (!ValidSave(Save)) return false;
    PendingLoad=Save;
    if (auto* PC=Cast<AHellgirlPlayerController>(UGameplayStatics::GetPlayerController(this,0))) PC->ResumeGame();
    UGameplayStatics::OpenLevel(this,TEXT("/Engine/Maps/Entry"),true,
        FString::Printf(TEXT("StageMap=1?CampaignLevel=%d?ForestHub=%d"),Save->Level,Save->bHub?1:0));
    return true;
}

void UHellgirlWallet::RestorePending()
{
    auto* GM=Cast<AArenaGameMode>(UGameplayStatics::GetGameMode(this));
    auto* Hero=Cast<AArenaFighter>(UGameplayStatics::GetPlayerPawn(this,0));
    if (!PendingLoad || !GM || !Hero) return;
    const auto* Save=PendingLoad.Get();
    Coins=Save->Coins; Carried=Save->Carried; bGoblinQueenOwned=Save->bGoblinQueenOwned; bLoadFailed=false;
    if (!IsSaveCheck())
    {
    SaveWallet();
    GConfig->SetInt(TEXT("HellgirlCampaign"),TEXT("UnlockedLevel"),Save->Unlocked,GGameUserSettingsIni);
    GConfig->SetInt(TEXT("HellgirlCampaign"),TEXT("ProgressVersion"),2,GGameUserSettingsIni);
    GConfig->SetInt(TEXT("HellgirlAppearance"),TEXT("Outfit"),Save->Outfit,GGameUserSettingsIni);
    for (const FString& Flag : HellgirlProgress::AllFlags()) GConfig->SetBool(HellgirlProgress::Section,*Flag,Save->Flags.Contains(Flag),GGameUserSettingsIni);
    GConfig->Flush(false,GGameUserSettingsIni);
    }
    Hero->SetOutfit(Save->Outfit==4 && !bGoblinQueenOwned?0:Save->Outfit);
    Hero->Health=FMath::Clamp(Save->Health,1.f,Hero->MaxHealth); Hero->Energy=FMath::Clamp(Save->Energy,0.f,Hero->MaxEnergy);
    Hero->Stamina=FMath::Clamp(Save->Stamina,0.f,100.f);
    const bool bOldImpLayout=Save->Level==4 && !Save->bHub && Save->ImpArenaLayoutVersion==0;
    const bool bOldHubLayout=Save->bHub && Save->ForestHubLayoutVersion==0;
    const FVector RestoredLocation=bOldImpLayout?FVector(0.f,0.f,115.f):bOldHubLayout?FVector(-550.f,0.f,110.f):Save->Location;
    Hero->SetActorLocationAndRotation(RestoredLocation,Save->Facing,false,nullptr,ETeleportType::TeleportPhysics);
    Hero->GetCharacterMovement()->StopMovementImmediately(); GM->LastSafePosition=RestoredLocation;
    if (auto* PC=Hero->GetController()) PC->SetControlRotation(Save->Camera);
    for (int32 I=0;!bOldImpLayout && I<GM->SpawnSites.Num() && I<Save->Cleared.Num();++I)
    { auto* Site=GM->SpawnSites[I].Get(); Site->bCleared=Save->Cleared[I]; Site->bActivated=Site->bCleared; Site->bEnabled=Site->bCleared; }
    GM->PlayedStory.Reset(); for (FName Moment:Save->Story) GM->PlayedStory.Add(Moment);
    GM->PendingStory.Reset(); GM->bHubLevelMenuTriggered=GM->GetHubInteraction()==1;
    for (int32 I=0;!bOldImpLayout && !bOldHubLayout && I<Save->PickupLocations.Num();++I)
        if (auto* Pickup=GetWorld()->SpawnActor<ACoinPickup>(Save->PickupLocations[I],FRotator::ZeroRotator))
        { if (Save->PickupAmounts[I]<0) Pickup->SetHeart(); else Pickup->SetAmount(Save->PickupAmounts[I]); }
    PendingLoad=nullptr;
}

bool UHellgirlWallet::RunSaveCheck()
{
    if (!IsSaveCheck()) return false;
    static int32 Step=0;
    auto* GM=Cast<AArenaGameMode>(UGameplayStatics::GetGameMode(this));
    auto* Hero=Cast<AArenaFighter>(UGameplayStatics::GetPlayerPawn(this,0));
    if (!GM || !Hero || GM->SpawnSites.IsEmpty()) return true;
    bool Passed=true;
    if (Step==0)
    {
        Hero->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
        GM->SpawnSites[0]->bActivated=true;
        Passed=SaveSlot(1)==TEXT("Finish this wave before saving.");
        GM->SpawnSites[0]->bCleared=true;
        GM->PlayedStory.Add(TEXT("Opening"));
        Coins=123; bGoblinQueenOwned=true; Hero->SetOutfit(4); Hero->Health=77.f; Hero->Energy=37.f;
        Passed &= SaveSlot(1)==TEXT("Game saved.");
        Coins=0; bGoblinQueenOwned=false; Hero->Health=20.f; Hero->Energy=0.f;
        Step=1;
        Passed &= LoadSlot(1);
        if (Passed) return true;
    }
    else if (PendingLoad) return true;
    else
    {
        Passed=Coins==123 && bGoblinQueenOwned && Hero->GetOutfit()==4 && Hero->Health==77.f && Hero->Energy==37.f
            && GM->SpawnSites[0]->bCleared && !GM->SpawnSites[1]->bCleared && GM->PlayedStory.Contains(TEXT("Opening"));
    }
    UGameplayStatics::DeleteGameInSlot(SlotName(1),0);
    if (Passed) { UE_LOG(LogTemp,Display,TEXT("SAVE CHECK PASSED: disk round trip, travel, currency, outfit, health, energy, waves, story and combat save blocking")); }
    else { UE_LOG(LogTemp,Error,TEXT("SAVE CHECK FAILED")); }
    FPlatformMisc::RequestExitWithStatus(false,Passed?0:1);
    return true;
}
