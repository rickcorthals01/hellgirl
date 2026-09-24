// -HellgirlMapShot (windowed, needs a GPU): on any map, saves the player's view and an overview from above to
// Saved/Screenshots/MapShot/<map>_<0|1>.png, then quits. -MapShotHeight=N sets the overview height.
#include "Levels/ArenaGameMode.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

void AArenaGameMode::RunMapShot(float Dt)
{
#if WITH_DEV_AUTOMATION_TESTS
    if (!FParse::Param(FCommandLine::Get(), TEXT("HellgirlMapShot"))) return;
    static float Clock = 0.f;
    static int32 Shot = 0;
    Clock += Dt;
    auto* PC = UGameplayStatics::GetPlayerController(this, 0);
    if (!PC || Shot > 2) return;
    const FString Name = bForestHub ? TEXT("Hub") : bForestRun ? TEXT("ForestRun") : bSuccubusCourt ? TEXT("Court")
        : IsImpArena() ? TEXT("ImpArena") : FString::Printf(TEXT("Stage%d_Level%d"), MapNumber, CampaignLevel);
    if (Shot == 0 && Clock > 4.f)
    {
        FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / FString::Printf(TEXT("Screenshots/MapShot/%s_0.png"), *Name), false, false);
        Shot = 1;
    }
    else if (Shot == 1 && Clock > 5.f)
    {
        float Height = 4500.f;
        FParse::Value(FCommandLine::Get(), TEXT("MapShotHeight="), Height);
        const FVector Focus = PC->GetPawn() ? PC->GetPawn()->GetActorLocation() : FVector::ZeroVector;
        auto* View = GetWorld()->SpawnActor<ACameraActor>(Focus + FVector(-Height * .55f, 0.f, Height), FRotator(-58.f, 0.f, 0.f));
        View->GetCameraComponent()->SetFieldOfView(75.f);
        PC->SetViewTarget(View);
        Shot = 2;
    }
    else if (Shot == 2 && Clock > 6.5f)
    {
        FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / FString::Printf(TEXT("Screenshots/MapShot/%s_1.png"), *Name), false, false);
        Shot = 3;
        FTimerHandle Quit;
        GetWorldTimerManager().SetTimer(Quit, [] { FPlatformMisc::RequestExitWithStatus(false, 0); }, 1.f, false);
    }
#endif
}
