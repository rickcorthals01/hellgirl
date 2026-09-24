#include "Levels/ArenaGameMode.h"
#include "Fighter/ArenaFighter.h"
#include "UI/HellgirlPlayerController.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/SkeletalMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "Containers/Ticker.h"
#include "Framework/Application/SlateApplication.h"
void AArenaGameMode::RunForestHubCheck(float Dt)
{
#if WITH_DEV_AUTOMATION_TESTS
    const bool Preview=FParse::Param(FCommandLine::Get(),TEXT("HellgirlHubPreview"));
    const bool Check=FParse::Param(FCommandLine::Get(),TEXT("HellgirlHubCheck"));
    const bool MenuPreview=FParse::Param(FCommandLine::Get(),TEXT("HellgirlLevelSelectPreview"));
    const bool StagePreview=FParse::Param(FCommandLine::Get(),TEXT("HellgirlLevelSelectStagePreview"));
    const bool RollCheck=FParse::Param(FCommandLine::Get(),TEXT("HellgirlHubRollCheck"));
    if (!Preview && !Check && !MenuPreview && !StagePreview && !RollCheck) return;
    static int32 Phase=0; static float Time=0.f; Time+=Dt;
    auto* Player=Cast<AArenaFighter>(UGameplayStatics::GetPlayerPawn(this,0));
    auto* PC=Cast<AHellgirlPlayerController>(UGameplayStatics::GetPlayerController(this,0));
    if (!Player || !PC || Time<1.f) return;
    auto Fail=[](const TCHAR* Why) { UE_LOG(LogTemp,Error,TEXT("HUB CHECK FAILED: %s"),Why); FPlatformMisc::RequestExitWithStatus(false,1); };
    // Roll into the level-select road the way a player would, then log what the menu and input do.
    if (RollCheck)
    {
        if (Phase==0)
        {
            Phase=1;
            // Close enough that the roll itself carries her into the road trigger (x > 510) at any frame rate.
            Player->SetActorLocation(FVector(420,0,110)); Player->SetActorRotation(FRotator::ZeroRotator);
            PC->SetControlRotation(FRotator(-20,0,0));
            // Real controller input through Slate, like XInput sends it: stick forward, B pressed and held
            // (with the repeats a held button produces), released, then A and B in the menu.
            auto Button=[](FKey Key,bool Down,bool Repeat=false)
            {
                const FKeyEvent Event(Key,FModifierKeysState(),0,Repeat,0,0);
                if (Down) FSlateApplication::Get().ProcessKeyDownEvent(Event); else FSlateApplication::Get().ProcessKeyUpEvent(Event);
            };
            auto Stick=[](float Value)
            {
                FSlateApplication::Get().ProcessAnalogInputEvent(FAnalogInputEvent(EKeys::Gamepad_LeftY,FModifierKeysState(),0,false,0,0,Value));
            };
            TWeakObjectPtr<AHellgirlPlayerController> Weak(PC); TWeakObjectPtr<AArenaFighter> WeakPlayer(Player);
            FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Weak,WeakPlayer,Button,Stick,Elapsed=0.f,Logged=0,Step=0](float Delta) mutable
            {
                Elapsed+=Delta;
                if (!Weak.IsValid() || !WeakPlayer.IsValid()) return false;
                // XInput repeats a held button 0.2 s after the press, then every 0.1 s.
                struct FStep { float At; int32 Kind; };  // 0 stick on, 1 B down, 2 B repeat, 3 B up, 4 stick off, 5 A tap, 6 B tap
                static const FStep Normal[]={{0.f,0},{.15f,1},{.35f,2},{.45f,2},{.55f,2},{.65f,2},{.75f,3},{.9f,4},{1.4f,5},{1.9f,6},{2.4f,6},{2.9f,0},{3.3f,4}};
                // -RollQuickBack: a fresh B press right after the menu opens (inside its 0.1 s focus delay) closes it;
                // control must return to the game. Steps are padded so the same checks apply at index 10.
                static const FStep Quick[]={{0.f,0},{.15f,1},{.2f,3},{.3f,4},{.31f,4},{.32f,4},{.33f,4},{.34f,4},{.35f,4},{.36f,4},{2.5f,4},{2.9f,0},{3.3f,4}};
                static bool bQuickTapped=false;
                const bool bQuick=FParse::Param(FCommandLine::Get(),TEXT("RollQuickBack"));
                const FStep* Steps=bQuick?Quick:Normal;
                constexpr int32 StepCount=UE_ARRAY_COUNT(Normal);
                static FVector AfterClose=FVector::ZeroVector;
                auto Expect=[](bool Good,const TCHAR* Why) { if (!Good) { UE_LOG(LogTemp,Error,TEXT("HUB ROLL CHECK FAILED: %s"),Why); FPlatformMisc::RequestExitWithStatus(false,1); } return Good; };
                const int32 Before=Step;
                // Quick case: press B the very frame the menu appears, inside its 0.1 s focus delay.
                if (bQuick && !bQuickTapped && Weak->IsPauseMenuOpen())
                {
                    bQuickTapped=true;
                    UE_LOG(LogTemp,Display,TEXT("ROLL INPUT %.2fs: menu just opened -> B tap"),Elapsed);
                    Button(EKeys::Gamepad_FaceButton_Right,true); Button(EKeys::Gamepad_FaceButton_Right,false);
                    UE_LOG(LogTemp,Display,TEXT("ROLL %.2fs: after quick B: menu %d, paused %d"),Elapsed,Weak->IsPauseMenuOpen()?1:0,Weak->IsPaused()?1:0);
                }
                while (Step<StepCount && Elapsed>=Steps[Step].At)
                {
                    const int32 Kind=Steps[Step++].Kind;
                    UE_LOG(LogTemp,Display,TEXT("ROLL INPUT %.2fs: %s"),Elapsed,Kind==0?TEXT("stick forward"):Kind==1?TEXT("B down"):Kind==2?TEXT("B repeat"):Kind==3?TEXT("B up"):Kind==4?TEXT("stick released"):Kind==5?TEXT("A tap"):TEXT("B tap"));
                    if (Kind==0) Stick(1.f);
                    else if (Kind==4) Stick(0.f);
                    else if (Kind==5) { Button(EKeys::Gamepad_FaceButton_Bottom,true); Button(EKeys::Gamepad_FaceButton_Bottom,false); }
                    else if (Kind==6) { Button(EKeys::Gamepad_FaceButton_Right,true); Button(EKeys::Gamepad_FaceButton_Right,false); }
                    else Button(EKeys::Gamepad_FaceButton_Right,Kind!=3,Kind==2);
                }
                // Expectations, checked right after the step with index Before has been sent.
                if (Step!=Before)
                {
                    const bool Open=Weak->IsPauseMenuOpen() && Weak->IsPaused() && Weak->bShowMouseCursor;
                    if (!bQuick && Before==6 && !Expect(Open,TEXT("Level select closed by a held B from the dodge"))) return false;
                    if (!bQuick && Before==8 && !Expect(Open,TEXT("A should open a world, not close the menu"))) return false;
                    if (!bQuick && Before==9 && !Expect(Open,TEXT("B in a world should go back to the world list, not close the menu"))) return false;
                    if (Before==10 && !Expect(!Weak->IsPauseMenuOpen() && !Weak->IsPaused() && !Weak->bShowMouseCursor
                        && FSlateApplication::Get().GetKeyboardFocusedWidget().IsValid() && FSlateApplication::Get().GetKeyboardFocusedWidget()->GetTypeAsString()==TEXT("SViewport"),
                        TEXT("Closing the menu must return input to the game"))) return false;
                    if (Before==10) AfterClose=WeakPlayer->GetActorLocation();
                }
                if (Elapsed>=Logged*.1f)
                {
                    ++Logged;
                    const TSharedPtr<SWidget> Focus=FSlateApplication::Get().GetKeyboardFocusedWidget();
                    UE_LOG(LogTemp,Display,TEXT("ROLL %.2fs: pos %s, menu %d, paused %d, cursor %d, focus %s"),Elapsed,*WeakPlayer->GetActorLocation().ToCompactString(),
                        Weak->IsPauseMenuOpen()?1:0,Weak->IsPaused()?1:0,Weak->bShowMouseCursor?1:0,Focus.IsValid()?*Focus->GetTypeAsString():TEXT("none"));
                }
                if (Elapsed<3.6f) return true;
                if (bQuick && !Expect(bQuickTapped,TEXT("Level select never opened in the quick-back case"))) return false;
                if (!Expect(FVector::Dist2D(WeakPlayer->GetActorLocation(),AfterClose)>50.f,TEXT("The stick no longer moves Hellgirl after the menu closed"))) return false;
                UE_LOG(LogTemp,Display,TEXT("HUB ROLL CHECK PASSED: rolling in with a held B keeps the level select open; A/B navigate; closing returns control"));
                FPlatformMisc::RequestExitWithStatus(false,0);
                return false;
            }),0.f);
        }
        return;
    }
    if (MenuPreview || StagePreview)
    {
        if (Phase==0)
        {
            Phase=1;
            Player->SetActorLocation(FVector(800,0,110));
            PC->OpenHubMenu(1);
            TWeakObjectPtr<AHellgirlPlayerController> Weak(PC);
            FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Weak,StagePreview,Elapsed=0.f](float Delta) mutable
            {
                Elapsed+=Delta;
                if (Elapsed<1.f) return true;
                if (!Weak.IsValid() || !Weak->IsPauseMenuOpen()) FPlatformMisc::RequestExitWithStatus(false,1);
                else FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()
                    /(StagePreview?TEXT("Screenshots/LevelSelectStages.png"):TEXT("Screenshots/LevelSelect.png")),true,false);
                FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float)
                {
                    UE_LOG(LogTemp,Display,TEXT("LEVEL SELECT PREVIEW PASSED"));
                    FPlatformMisc::RequestExitWithStatus(false,0);
                    return false;
                }),1.f);
                return false;
            }),.01f);
        }
        return;
    }
    if (Preview)
    {
        if (Phase==0)
        {
            auto* View=GetWorld()->SpawnActor<ACameraActor>(FVector(-700,-540,1120),FRotator(-43,34,0));
            View->GetCameraComponent()->SetFieldOfView(72); PC->SetViewTarget(View); Phase=1;
        }
        if (Phase==1 && Time>3.f) { FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/ForestHub.png"),false,false); Phase=2; }
        // Then the player's own view of camp.
        if (Phase==2 && Time>4.f) { PC->SetViewTarget(PC->GetPawn()); Phase=3; }
        if (Phase==3 && Time>5.5f) { FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/ForestHubPlayer.png"),false,false); Phase=4; }
        if (Phase==4 && Time>6.5f) { UE_LOG(LogTemp,Display,TEXT("HUB PREVIEW CHECK PASSED")); FPlatformMisc::RequestExitWithStatus(false,0); }
        return;
    }
    if (Phase==0)
    {
        if (!bForestHub || !SpawnSites.IsEmpty() || !HubMerchant || !HubMerchant->GetSkeletalMeshComponent()->GetSkeletalMeshAsset()) { Fail(TEXT("Hub layout, merchant model or empty encounter list")); return; }
        for (TActorIterator<AArenaFighter> It(GetWorld());It;++It) if (It->bEnemy) { Fail(TEXT("Enemy present in safe hub")); return; }
        auto HitsBoundary=[&](FVector From,FVector To)
        {
            FHitResult Hit;
            FCollisionQueryParams Query; Query.AddIgnoredActor(Player);
            return GetWorld()->LineTraceSingleByChannel(Hit,From,To,ECC_Visibility,Query)
                && Hit.GetActor() && Hit.GetActor()->ActorHasTag(TEXT("HubBoundary"));
        };
        if (!HitsBoundary(FVector(0,0,115),FVector(-1450,0,115))
            || !HitsBoundary(FVector(0,0,115),FVector(0,1450,115))
            || HitsBoundary(FVector(800,0,115),FVector(1300,0,115))
            || !HitsBoundary(FVector(1100,0,115),FVector(1700,0,115)))
        { Fail(TEXT("Forest boundary or level-select road collision")); return; }
        const FVector Spots[]={FVector(0,-220,110),FVector(800,0,110),FVector(-100,440,110)};
        for (int32 I=0;I<3;++I)
        {
            Player->SetActorLocation(Spots[I]);
            FHitResult Floor; FCollisionQueryParams Q; Q.AddIgnoredActor(Player);
            if (!GetWorld()->LineTraceSingleByChannel(Floor,Spots[I],Spots[I]-FVector(0,0,500),ECC_Visibility,Q)) { Fail(TEXT("Interaction missing floor")); return; }
            if (GetHubInteraction()!=I) { Fail(TEXT("Wrong proximity interaction")); return; }
            Player->Health=42.f; PC->InteractWithHub();
            if (!PC->IsPauseMenuOpen() || !PC->IsPaused() || !PC->bShowMouseCursor) { Fail(TEXT("Interaction menu failed to open")); return; }
            if (I==0 && Player->Health!=Player->MaxHealth) { Fail(TEXT("Campfire did not heal")); return; }
            PC->ResumeGame();
            if (PC->IsPaused() || PC->bShowMouseCursor) { Fail(TEXT("Menu did not restore play")); return; }
        }
        Player->Energy=100; Player->Attack(); Player->HeavyAttack(); Player->ReleaseHeavyAttack(); Player->Special();
        if (Player->Energy!=100) { Fail(TEXT("Combat active in hub")); return; }
        Phase=1; TravelToCampaign(2); return;
    }
    if (Phase==1 && !bForestHub)
    {
        if (CampaignLevel!=2 || Player->Energy!=0.f) { Fail(TEXT("Hub departure loaded wrong level or resources")); return; }
        Phase=2; TravelToHub(); return;
    }
    if (Phase==2 && bForestHub)
    {
        UE_LOG(LogTemp,Display,TEXT("HUB CHECK PASSED: safe clearing, merchant, floor, three menus, healing, input restore, level departure and return"));
        FPlatformMisc::RequestExitWithStatus(false,0); Phase=3;
    }
    if (Time>30.f) Fail(TEXT("Hub travel timeout"));
#endif
}

