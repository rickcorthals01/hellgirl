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
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "Framework/Application/SlateApplication.h"
#include "InputKeyEventArgs.h"
#include "Engine/GameViewportClient.h"
#include "Progress/CampaignProgress.h"
#include "Progress/HellgirlWallet.h"
void AHellgirlPlayerController::RunStoryCheck()
{
#if WITH_DEV_AUTOMATION_TESTS
    if (!FParse::Param(FCommandLine::Get(),TEXT("HellgirlStoryCheck"))) return;
    struct FState { int32 Phase=0,Wave=0,Pages=0,Portraits=0,Portals=0,LastPage=-1; FName LastId; double PageAt=0; bool bShot=false,bPressed=false; int32 PortalStage=0,CampPhase=0; bool bDeviceTested=false; double PortalAt=0,CampAt=0; TArray<FName> Seen; TWeakObjectPtr<AArenaFighter> Queen; double Start=FPlatformTime::Seconds(); };
    auto State=MakeShared<FState>(); TWeakObjectPtr<AHellgirlPlayerController> Weak(this);
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Weak,State](float) {
        if (!Weak.IsValid()) return false;
        auto* PC=Weak.Get(); auto* GM=Cast<AArenaGameMode>(UGameplayStatics::GetGameMode(PC)); auto* Hero=Cast<AArenaFighter>(PC->GetPawn());
        if (!GM || !Hero) return true;
        auto Fail=[](const TCHAR* Why) { UE_LOG(LogTemp,Error,TEXT("STORY CHECK FAILED: %s"),Why); FPlatformMisc::RequestExitWithStatus(false,1); };
        // Stage 3 alone clears fourteen waves.
        if (FPlatformTime::Seconds()-State->Start>(GM->CampaignLevel==1?60:240)) { Fail(TEXT("Story test timed out")); return false; }
        Hero->Health=Hero->MaxHealth=100000.f;
        // -StoryFromHub: start at camp and travel into Stage 1 the normal way (the check restarts in the new level).
        const bool Camp=FParse::Param(FCommandLine::Get(),TEXT("StoryCamp"));
        if (GM->bForestHub && !Camp)
        {
            if (FParse::Param(FCommandLine::Get(),TEXT("StoryFromHub")) && FPlatformTime::Seconds()-State->Start>2.0) { GM->TravelToCampaign(1); return false; }
            return true;
        }
        if (GM->CampaignLevel==1 && !GM->bForestHub)
        {
            // Stage 1: darkness, waking, the voice, two waves, the Queen's outburst, three waves, leaving.
            const auto& Sites=GM->GetSpawnSites();
            if (Sites.Num()!=5) { Fail(TEXT("Stage 1 must have five waves")); return false; }
            if (PC->IsDialogueOpen())
            {
                const FName Id=PC->GetConversation();
                const int32 Page=PC->GetConversationPage();
                const double Now=FPlatformTime::Seconds();
                // Each page is inspected once, when it first appears.
                if (Id!=State->LastId || Page!=State->LastPage)
                {
                    State->LastId=Id; State->LastPage=Page; State->PageAt=Now; State->bPressed=State->bShot=false;
                    if (State->Seen.IsEmpty() || State->Seen.Last()!=Id) State->Seen.Add(Id);
                    if (!GM->HasPlayedStory(TEXT("L1_Voice")) && Sites[0]->bActivated) { Fail(TEXT("A wave started before the voice")); return false; }
                    if (Id==TEXT("L1_Wake") && !Hero->IsWakingUp()) { Fail(TEXT("Hellgirl is not on the floor while waking")); return false; }
                    State->Portraits+=PC->GetConversationPortrait()!=nullptr;
                    ++State->Pages;
                    if (Id==TEXT("L1_Portal"))
                    {
                        const TArray<FName> Expected={TEXT("L1_Darkness"),TEXT("L1_Wake"),TEXT("L1_Voice"),TEXT("L1_AfterWave2"),TEXT("L1_AfterWave5"),TEXT("L1_Portal")};
                        // Portraits: Hellgirl's two waking lines, four after wave 2, one after wave 5 and one at the portal.
                        if (State->Seen!=Expected || State->Pages!=17 || State->Portraits!=8) { Fail(TEXT("Wrong Stage 1 conversation order, page count or portraits")); return false; }
                        UE_LOG(LogTemp,Display,TEXT("STORY CHECK PASSED: Stage 1 darkness, wake-up, voice before any wave, five waves, Queen outburst, exit lines, 17 pages, 8 portraits%s"),
                            FParse::Param(FCommandLine::Get(),TEXT("StoryRealInput"))?TEXT(", every page advanced by real key presses"):TEXT(""));
                        FPlatformMisc::RequestExitWithStatus(false,0); return false;
                    }
                }
                // -StoryShots (windowed): photograph every page before moving on.
                if (FParse::Param(FCommandLine::Get(),TEXT("StoryShots")))
                {
                    if (Now-State->PageAt<.5) return true;
                    if (!State->bShot)
                    {
                        FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/FString::Printf(TEXT("Screenshots/Story/%02d_%s_%d.png"),State->Pages-1,*Id.ToString(),Page),true,false);
                        State->bShot=true; return true;
                    }
                    if (Now-State->PageAt<.9) return true;
                }
                // -StoryRealInput: advance with real key presses through Slate (Space and controller A in turn), like a player.
                if (FParse::Param(FCommandLine::Get(),TEXT("StoryRealInput")))
                {
                    if (Now-State->PageAt>3.0) { Fail(*FString::Printf(TEXT("A real key press did not advance %s page %d"),*Id.ToString(),Page)); return false; }
                    if (Now-State->PageAt>.4 && !State->bPressed)
                    {
                        const FKey Key=(State->Pages%2)?EKeys::Gamepad_FaceButton_Bottom:EKeys::SpaceBar;
                        FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(Key,FModifierKeysState(),0,false,0,0));
                        FSlateApplication::Get().ProcessKeyUpEvent(FKeyEvent(Key,FModifierKeysState(),0,false,0,0));
                        State->bPressed=true;
                    }
                    return true;
                }
                PC->ContinueDialogue(); return true;
            }
            if (Hero->IsWakingUp() || !GM->HasPlayedStory(TEXT("L1_Wake"))) return true;
            // Once she is up, walk her forward until the voice speaks.
            if (!GM->HasPlayedStory(TEXT("L1_Voice"))) { Hero->SetActorLocation(FVector(-4300,0,115)); return true; }
            int32 Next=0; while (Next<5 && Sites[Next]->bCleared) ++Next;
            if (Next<5)
            {
                auto* Site=Sites[Next].Get();
                Hero->SetActorLocation(Site->GetActorLocation()+FVector(0,0,115));
                if (Site->LivingEnemies()>0)
                    for (TActorIterator<AArenaFighter> It(PC->GetWorld());It;++It) if (It->bEnemy && It->EncounterSite==Site) It->Health=0;
                return true;
            }
            if (GM->HasPlayedStory(TEXT("L1_AfterWave5"))) { Hero->SetActorLocation(GM->ExitPosition+FVector(0,0,115)); GM->UseExitPortal(); }
            return true;
        }
        // Stage 3 is played "on a controller": a real controller button press must switch the key names shown in
        // dialogue, so the ultimate tip later reads "Press Right Thumbstick to ...".
        // (The engine hands every key, button and stick movement to the controller's InputKey; a headless run cannot
        // route a synthetic press there, so the events go to it directly.)
        if (GM->CampaignLevel==3 && !State->bDeviceTested && !PC->IsDialogueOpen() && !PC->IsPauseMenuOpen())
        {
            State->bDeviceTested=true;
            FViewport* View=PC->GetWorld()->GetGameViewport()?PC->GetWorld()->GetGameViewport()->Viewport:nullptr;
            const FInputDeviceId Device=FInputDeviceId::CreateFromInternalId(0);
            auto Send=[&](const FKey& Key,EInputEvent Event,float Amount)
            { PC->InputKey(FInputKeyEventArgs(View,Device,Key,Event,Amount,false,FPlatformTime::Cycles64())); };
            bool Ok=!PC->IsUsingGamepad();
            Send(EKeys::Gamepad_RightX,IE_Axis,.2f); Ok&=!PC->IsUsingGamepad();          // stick drift is ignored
            Send(EKeys::Gamepad_FaceButton_Top,IE_Pressed,1.f); Ok&=PC->IsUsingGamepad(); // a button press
            Send(EKeys::Gamepad_FaceButton_Top,IE_Released,1.f);
            Send(EKeys::MouseX,IE_Axis,.5f); Ok&=PC->IsUsingGamepad();                    // a tiny mouse nudge is ignored
            Send(EKeys::Q,IE_Pressed,1.f); Ok&=!PC->IsUsingGamepad();                     // a key: back to keyboard
            Send(EKeys::Q,IE_Released,1.f);
            Send(EKeys::Gamepad_RightY,IE_Axis,.9f); Ok&=PC->IsUsingGamepad();           // a full stick push
            UE_LOG(LogTemp,Display,TEXT("Input device tracking: %s"),Ok?TEXT("ok"):TEXT("wrong"));
            if (!Ok) { Fail(TEXT("Keyboard / controller tracking for dialogue key names")); return false; }
        }
        // Stages 2 and 3 follow their wave scripts (Levels/GoblinWaves.cpp): every conversation in order, the soul
        // portals (continued at once), the gates, and in Stage 3 the Queen: ultimates unlock at 30%, she begs, flees.
        if (PC->IsDialogueOpen())
        {
            const FName Id=PC->GetConversation();
            const int32 Page=PC->GetConversationPage();
            const double Now=FPlatformTime::Seconds();
            if (Id!=State->LastId || Page!=State->LastPage)
            {
                State->LastId=Id; State->LastPage=Page; State->PageAt=Now; State->bShot=false;
                if (State->Seen.IsEmpty() || State->Seen.Last()!=Id) State->Seen.Add(Id);
                ++State->Pages;
                if (Id==TEXT("L2_PortalHelp") && !GM->IsSoulPortalOpen()) { Fail(TEXT("The portal help showed without its portal")); return false; }
                if (Id==TEXT("L3_SubjectsReply") && GM->IsSoulPortalOpen()) { Fail(TEXT("Subjects? before continuing through the portal")); return false; }
                if (Id==TEXT("QueenLowHealth") && HellgirlProgress::UltimatesUnlocked()) { Fail(TEXT("Ultimates were unlocked before the Queen's 30% moment")); return false; }
                // The how-to box names the ultimate key.
                // (On keyboard it reads "Press Q to ..."; a controller key is named "Right Thumbstick".)
                if (Id==TEXT("L3_UltimateTip")) UE_LOG(LogTemp,Display,TEXT("Ultimate tip reads: %s (gamepad %d, controller name \"%s\")"),*PC->GetConversationLine(),PC->IsUsingGamepad(),*AHellgirlPlayerController::FriendlyKeyName(EKeys::Gamepad_RightThumbstick));
                if (Id==TEXT("L3_UltimateTip") && (!HellgirlProgress::UltimatesUnlocked() || PC->GetConversationLine().Contains(TEXT("UltimateKey"))
                    || !PC->GetConversationLine().StartsWith(PC->IsUsingGamepad()?TEXT("Press Right Thumbstick to"):TEXT("Press Q to")) || PC->GetConversationLine().Contains(TEXT("Gamepad"))
                    || AHellgirlPlayerController::FriendlyKeyName(EKeys::Gamepad_RightThumbstick)!=TEXT("Right Thumbstick")))
                { Fail(TEXT("Ultimate tip before the unlock, or its key was not filled in")); return false; }
                if (Id==TEXT("QueenDefeat") && Page==0 && (!HellgirlProgress::UltimatesUnlocked() || Hero->Energy<Hero->MaxEnergy))
                { Fail(TEXT("Ultimate unlock or full energy missing after 30%")); return false; }
            }
            if (FParse::Param(FCommandLine::Get(),TEXT("StoryShots")))
            {
                if (Now-State->PageAt<.5) return true;
                if (!State->bShot)
                {
                    FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/FString::Printf(TEXT("Screenshots/Story/L%d_%02d_%s_%d.png"),GM->CampaignLevel,State->Pages-1,*Id.ToString(),Page),true,false);
                    State->bShot=true; return true;
                }
                if (Now-State->PageAt<.9) return true;
            }
            PC->ContinueDialogue(); return true;
        }
        // -StoryCamp (at camp, progress as after Stage 3): the three one-time camp moments in order, the goblin's
        // first talk unlocking his shop, then the level select with the endless card.
        if (GM->bForestHub)
        {
            const double Now=FPlatformTime::Seconds();
            auto Shot=[&](const TCHAR* Name) { if (FParse::Param(FCommandLine::Get(),TEXT("StoryShots"))) FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/FString::Printf(TEXT("Screenshots/Story/Camp_%s.png"),Name),true,false); };
            if (State->CampPhase==0)
            {
                if (!HellgirlProgress::Flag(TEXT("GoblinFollowed")) || Now-State->PageAt<1.0) return true;
                const TArray<FName> Expected={TEXT("C_SetUpCamp"),TEXT("C_EndlessUnlocked"),TEXT("C_GoblinFollowed")};
                if (State->Seen!=Expected || State->Pages!=3) { Fail(TEXT("Camp moments out of order")); return false; }
                Hero->SetActorLocation(FVector(-100,440,110)); PC->SetControlRotation(FRotator(-12,90,0));
                PC->InteractWithHub(); State->CampPhase=1; return true;
            }
            if (State->CampPhase==1)
            {
                // The shop opens after the talk.
                if (!PC->IsPauseMenuOpen()) { Fail(TEXT("The shop did not open after the goblin's first talk")); return false; }
                if (!HellgirlProgress::Flag(TEXT("ShopUnlocked")) || State->Seen.Last()!=TEXT("C_MeetGoblin") || State->Pages!=7) { Fail(TEXT("Goblin talk or shop unlock missing")); return false; }
                State->CampAt=Now; State->CampPhase=2; return true;
            }
            if (State->CampPhase==2 && Now-State->CampAt>.6) { Shot(TEXT("Shop")); State->CampPhase=3; return true; }
            if (State->CampPhase==3 && Now-State->CampAt>1.2) { PC->ResumeGame(); PC->OpenHubMenu(1); State->CampPhase=4; return true; }
            if (State->CampPhase==4 && Now-State->CampAt>2.0) { Shot(TEXT("LevelSelect")); State->CampPhase=5; return true; }
            if (State->CampPhase==5 && Now-State->CampAt>2.6)
            {
                UE_LOG(LogTemp,Display,TEXT("STORY CHECK PASSED: camp moments after Stages 1-3 in order (black-screen line, endless unlock, the goblin), his first talk unlocking the shop"));
                FPlatformMisc::RequestExitWithStatus(false,0); return false;
            }
            return true;
        }
        const auto& Sites=GM->GetSpawnSites();
        if (Sites.Num()!=(GM->CampaignLevel==2?8:15)) { Fail(TEXT("Stage 2 needs 8 wave sites, Stage 3 15")); return false; }
        // -StoryShots: photograph a portal from a few steps away, then walk in and photograph its menu.
        // Returns true while the photographs are still being taken.
        auto PortalShots=[&](const FVector& Where,const TCHAR* Name)
        {
            if (!FParse::Param(FCommandLine::Get(),TEXT("StoryShots"))) return false;
            const double Now=FPlatformTime::Seconds();
            auto Shot=[&](const TCHAR* Suffix) { FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/FString::Printf(TEXT("Screenshots/Story/L%d_%s%s.png"),GM->CampaignLevel,Name,Suffix),true,false); };
            switch (State->PortalStage)
            {
            case 0:
                // Some Souls (in memory only) so the HUD and menu show them.
                if (auto* Wallet=Cast<UHellgirlWallet>(PC->GetGameInstance()); Wallet && Wallet->LevelSouls.Souls==0) { Wallet->LevelSouls.Souls=42; Wallet->LevelSouls.Earned+=42; }
                Hero->SetActorLocation(Where+FVector(-750,-250,115)); PC->SetControlRotation(FRotator(-10,18,0)); State->PortalAt=Now; State->PortalStage=1; return true;
            case 1: if (Now-State->PortalAt>.8) { Shot(TEXT("")); State->PortalStage=2; } return true;
            case 2: if (Now-State->PortalAt>1.3) { Hero->SetActorLocation(Where+FVector(-120,0,115)); State->PortalStage=3; } return true;
            case 3: if (Now-State->PortalAt>2.1) { Shot(TEXT("Menu")); State->PortalStage=4; } return true;
            case 4: if (Now-State->PortalAt>2.6) { PC->ResumeGame(); State->PortalStage=5; return false; } return true;
            default: return false;
            }
        };
        if (GM->IsSoulPortalOpen())
        {
            if (GM->IsPortalIntroPending()) return true; // its help text comes first
            if (PortalShots(GM->GetSoulPortalLocation(),TEXT("Portal"))) return true;
            ++State->Portals; State->PortalStage=0; GM->ChoosePortal(AArenaGameMode::EPortalChoice::Continue); return true;
        }
        if (GM->IsExitOpen() && PortalShots(GM->ExitPosition,TEXT("Exit"))) return true;
        if (PC->IsPauseMenuOpen()) { PC->ResumeGame(); return true; }
        if (GM->IsExitOpen())
        {
            const TArray<FName> Expected=GM->CampaignLevel==2
                ? TArray<FName>{TEXT("L2_Start"),TEXT("L2_PortalHelp"),TEXT("L2_Army")}
                : TArray<FName>{TEXT("L3_Goblins"),TEXT("L3_TalkToMe"),TEXT("L3_Subjects"),TEXT("L3_SubjectsReply"),TEXT("L3_BossStart"),TEXT("QueenLowHealth"),TEXT("L3_UltimateTip"),TEXT("QueenDefeat"),TEXT("L3_Escaped")};
            const int32 Pages=GM->CampaignLevel==2?4:29, Portals=GM->CampaignLevel==2?2:3;
            if (State->Seen!=Expected || State->Pages!=Pages || State->Portals!=Portals)
            {
                FString Got; for (FName N:State->Seen) Got+=N.ToString()+TEXT(" ");
                UE_LOG(LogTemp,Error,TEXT("Seen: %s(%d pages, %d portals)"),*Got,State->Pages,State->Portals);
                Fail(TEXT("Wrong conversation order, page count or portal count")); return false;
            }
            for (auto Site:Sites) if (!Site->bCleared) { Fail(TEXT("Exit opened with waves left")); return false; }
            if (GM->CampaignLevel==2)
            {
                UE_LOG(LogTemp,Display,TEXT("STORY CHECK: Stage 2 passed (7 waves, 2 soul portals, 4 pages); on to Stage 3"));
                GM->TravelToCampaign(3); return false;
            }
            if (State->Queen.IsValid()) { Fail(TEXT("Queen did not flee")); return false; }
            UE_LOG(LogTemp,Display,TEXT("STORY CHECK PASSED: Stages 2 and 3 in script order, soul portals and gates, 14 waves, ultimate unlock at 30%%, the Queen's surrender and escape, exit portal"));
            FPlatformMisc::RequestExitWithStatus(false,0); return false;
        }
        const AEnemySpawnPoint* Throne=GM->CampaignLevel==3?Sites.Last().Get():nullptr;
        if (Throne && Throne->bActivated)
        {
            Hero->SetActorLocation(FVector(4050,0,115));
            if (!State->Queen.IsValid())
                for (TActorIterator<AArenaFighter> It(PC->GetWorld());It;++It)
                    if (It->bEnemy && It->EnemyType==EHellgirlEnemyType::GoblinQueen) { State->Queen=*It; break; }
            if (State->Queen.IsValid() && !State->Queen->bStorySurrendered && GM->HasPlayedStory(TEXT("L3_BossStart")))
            {
                auto* Queen=State->Queen.Get();
                // Exercise the real phase triggers, bypassing only attack timing.
                if (!Queen->IsBossHidden()) Queen->ApplyPhysicsDamage(100000.f,FVector::ZeroVector);
                for (TActorIterator<AArenaFighter> It(PC->GetWorld());It;++It) if (It->bEnemy && *It!=Queen) It->Health=0;
            }
            return true;
        }
        // Stand in the running wave and clear it as its goblins arrive; otherwise walk to the next one.
        for (auto Site:Sites)
            if (Site->bActivated && !Site->bCleared)
            {
                Hero->SetActorLocation(Site->GetActorLocation()+FVector(0,0,115));
                for (TActorIterator<AArenaFighter> It(PC->GetWorld());It;++It) if (It->bEnemy && It->EncounterSite==Site) It->Health=0;
                return true;
            }
        for (auto Site:Sites) if (!Site->bActivated) { Hero->SetActorLocation(Site->GetActorLocation()+FVector(0,0,115)); break; }
        return true;
    }));
#endif
}
