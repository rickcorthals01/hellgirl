// -HellgirlMapShot (windowed, needs a GPU): on any map, saves the player's view and an overview from above to
// Saved/Screenshots/MapShot/<map>_<0|1>.png, then quits. -MapShotHeight=N sets the overview height.
#include "Levels/ArenaGameMode.h"
#include "Fighter/ArenaFighter.h"
#include "Particles/ParticleSystem.h"
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
    // -MapShotEmitters=/Game/A.A;/Game/B.B: plays each particle effect in front of Hellgirl, captured shortly after.
    FString EmitterList;
    if (FParse::Value(FCommandLine::Get(), TEXT("MapShotEmitters="), EmitterList, false))
    {
        static int32 Played = 0;
        static float PlayedAt = 0.f;
        TArray<FString> Paths;
        EmitterList.ParseIntoArray(Paths, TEXT(";"));
        APawn* Hero = PC->GetPawn();
        if (!Hero || Clock < 3.f) return;
        if (Played < Paths.Num() * 2)
        {
            const int32 Index = Played / 2;
            if (Played % 2 == 0 && Clock - PlayedAt > .9f)
            {
                if (auto* System = LoadObject<UParticleSystem>(nullptr, *Paths[Index]))
                    UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), System, Hero->GetActorLocation() + Hero->GetActorForwardVector() * 220.f + FVector(0, 0, 30),
                        (-Hero->GetActorForwardVector()).Rotation(), FVector(.6f));
                PlayedAt = Clock;
                ++Played;
            }
            else if (Played % 2 == 1 && Clock - PlayedAt > .12f)
            {
                FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / FString::Printf(TEXT("Screenshots/MapShot/Emitter_%d.png"), Index), false, false);
                ++Played;
            }
        }
        else if (Clock - PlayedAt > 1.f) FPlatformMisc::RequestExitWithStatus(false, 0);
        return;
    }
    // -MapShotImpacts: a goblin in front of Hellgirl takes a heavy hit, then a light one, each captured on contact.
    if (FParse::Param(FCommandLine::Get(), TEXT("MapShotImpacts")))
    {
        static TWeakObjectPtr<AArenaFighter> Dummy;
        static int32 Hit = 0;
        auto* Hero = Cast<AArenaFighter>(PC->GetPawn());
        if (!Hero) return;
        if (!Dummy.IsValid() && Clock > 2.f && Hit == 0)
        {
            const FVector At = Hero->GetActorLocation() + Hero->GetActorForwardVector() * 230.f;
            Dummy = GetWorld()->SpawnActor<AArenaFighter>(At, (Hero->GetActorLocation() - At).Rotation());
            if (Dummy.IsValid())
            {
                Dummy->MakeEnemy(1, false);
                Dummy->SetEnemyType(EHellgirlEnemyType::Goblins);
                Dummy->Health = Dummy->MaxHealth = 5000.f;
                Dummy->HomePosition = At;
            }
        }
        const float Times[] = {3.5f, 3.62f, 5.f, 5.08f, 6.5f};
        if (Hit < 5 && Clock > Times[Hit] && Dummy.IsValid())
        {
            const FVector Away = (Dummy->GetActorLocation() - Hero->GetActorLocation()).GetSafeNormal2D();
            if (Hit == 0) Dummy->ReceiveHit(30.f, Away, 700.f, 0.f);
            else if (Hit == 2)
            {
                Dummy->ReceiveHit(8.f, Away, 120.f, 0.f);
                // Drop souls as a defeated enemy would, between Hellgirl and the goblin.
                EnemyDefeated(FMath::Lerp(Hero->GetActorLocation(), Dummy->GetActorLocation(), .5f));
            }
            else if (Hit < 4) FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / FString::Printf(TEXT("Screenshots/MapShot/Impact_%d.png"), Hit / 2), false, false);
            else FPlatformMisc::RequestExitWithStatus(false, 0);
            ++Hit;
        }
        return;
    }
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
