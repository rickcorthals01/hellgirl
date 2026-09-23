#include "ArenaGameMode.h"
#include "ArenaFighter.h"
#include "HellgirlPlayerController.h"
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
void AArenaGameMode::RunForestHubCheck(float Dt)
{
#if WITH_DEV_AUTOMATION_TESTS
    const bool Preview=FParse::Param(FCommandLine::Get(),TEXT("HellgirlHubPreview"));
    const bool Check=FParse::Param(FCommandLine::Get(),TEXT("HellgirlHubCheck"));
    const bool MenuPreview=FParse::Param(FCommandLine::Get(),TEXT("HellgirlLevelSelectPreview"));
    const bool StagePreview=FParse::Param(FCommandLine::Get(),TEXT("HellgirlLevelSelectStagePreview"));
    if (!Preview && !Check && !MenuPreview && !StagePreview) return;
    static int32 Phase=0; static float Time=0.f; Time+=Dt;
    auto* Player=Cast<AArenaFighter>(UGameplayStatics::GetPlayerPawn(this,0));
    auto* PC=Cast<AHellgirlPlayerController>(UGameplayStatics::GetPlayerController(this,0));
    if (!Player || !PC || Time<1.f) return;
    auto Fail=[](const TCHAR* Why) { UE_LOG(LogTemp,Error,TEXT("HUB CHECK FAILED: %s"),Why); FPlatformMisc::RequestExitWithStatus(false,1); };
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
        if (Phase==2 && Time>4.f) { UE_LOG(LogTemp,Display,TEXT("HUB PREVIEW CHECK PASSED")); FPlatformMisc::RequestExitWithStatus(false,0); }
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

