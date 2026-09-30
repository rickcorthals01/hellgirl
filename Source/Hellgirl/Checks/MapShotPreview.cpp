// -HellgirlMapShot (windowed, needs a GPU): on any map, saves the player's view and an overview from above to
// Saved/Screenshots/MapShot/<map>_<0|1>.png, then quits. -MapShotHeight=N sets the overview height.
#include "Levels/ArenaGameMode.h"
#include "Levels/WavePortal.h"
#include "Fighter/ArenaFighter.h"
#include "Progress/HellgirlWallet.h"
#include "UI/HellgirlPlayerController.h"
#include "Enemies/EnemySpawnPoint.h"
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
#include "EngineUtils.h"
#include "Containers/Ticker.h"
#include "ShaderCompiler.h"

void AArenaGameMode::RunMapShot(float Dt)
{
#if WITH_DEV_AUTOMATION_TESTS
    if (!FParse::Param(FCommandLine::Get(), TEXT("HellgirlMapShot"))) return;
    static float Clock = 0.f;
    static int32 Shot = 0;
#if WITH_EDITOR
    // New materials would be drawn with the default one until their shaders are compiled: the clock waits for them.
    if (GShaderCompilingManager && GShaderCompilingManager->IsCompiling()) return;
#endif
    Clock += Dt;
    auto* PC = UGameplayStatics::GetPlayerController(this, 0);
    if (!PC || Shot > 2) return;
    const FString Name = bForestHub ? TEXT("Hub") : bForestRun ? TEXT("ForestRun") : bGraveyard ? TEXT("Graveyard") : bSwamp ? TEXT("Swamp") : bFrozenMaze ? TEXT("Maze") : bSuccubusCourt ? TEXT("Court")
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
    // -MapShotDeath: Hellgirl falls; the death menu is captured once it is up (the game is paused then, so the
    // capture and the exit run on the core ticker).
    if (FParse::Param(FCommandLine::Get(), TEXT("MapShotDeath")))
    {
        static bool Killed = false;
        auto* Hero = Cast<AArenaFighter>(PC->GetPawn());
        if (Killed || !Hero || Clock < 2.f) return;
        Killed = true;
        Hero->ApplyPhysicsDamage(100000.f, FVector::ZeroVector);
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float) {
            FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("Screenshots/MapShot/Death.png"), true, false); return false; }), 3.f);
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float) { FPlatformMisc::RequestExitWithStatus(false, 0); return false; }), 4.5f);
        return;
    }
    // -MapShotPortalMenu: the blue portal's menu with fresh offers and a pocket of Souls, captured once it is up.
    if (FParse::Param(FCommandLine::Get(), TEXT("MapShotPortalMenu")))
    {
        static bool bOpened = false;
        if (bOpened || Clock < 2.f) return;
        bOpened = true;
        if (auto* Wallet = Cast<UHellgirlWallet>(GetGameInstance())) Wallet->LevelSouls.Souls = 60;
        RollPortalOffers();
        if (auto* Controller = Cast<AHellgirlPlayerController>(PC)) Controller->OpenPortalMenu(false);
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float) {
            FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("Screenshots/MapShot/PortalMenu.png"), true, false); return false; }), 1.f);
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float) { FPlatformMisc::RequestExitWithStatus(false, 0); return false; }), 2.5f);
        return;
    }
    // -MapShotPortals: a blue soul portal and a purple exit open ahead of Hellgirl, captured opening and then open.
    if (FParse::Param(FCommandLine::Get(), TEXT("MapShotPortals")))
    {
        static bool Opened = false;
        static int32 PortalShot = 0;
        APawn* Hero = PC->GetPawn();
        if (!Hero || Clock < 2.f) return;
        if (!Opened)
        {
            Opened = true;
            const FVector Ahead = Hero->GetActorLocation() + Hero->GetActorForwardVector() * 900.f;
            const FVector Side = Hero->GetActorRightVector() * 330.f;
            if (auto* Blue = GetWorld()->SpawnActor<AWavePortal>()) Blue->Open(Ahead - Side, Hero->GetActorLocation(), false);
            if (auto* Purple = GetWorld()->SpawnActor<AWavePortal>()) Purple->Open(Ahead + Side, Hero->GetActorLocation(), true);
        }
        const float Times[] = {2.25f, 4.f, 4.4f};
        if (PortalShot < 3 && Clock > Times[PortalShot])
        {
            FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / FString::Printf(TEXT("Screenshots/MapShot/Portals_%d.png"), PortalShot), false, false);
            ++PortalShot;
        }
        else if (PortalShot >= 3 && Clock > 5.5f) FPlatformMisc::RequestExitWithStatus(false, 0);
        return;
    }
    // -MapShotSword: Hellgirl draws her sword (-MapShotInfernal: the shop's Infernal Sword) and is seen close up from the
    // front and the side, then mid-slash: Sword_0..2.png.
    if (FParse::Param(FCommandLine::Get(), TEXT("MapShotSword")))
    {
        static int32 SwordShot = 0;
        static TWeakObjectPtr<ACameraActor> Close;
        auto* Hero = Cast<AArenaFighter>(PC->GetPawn());
        if (!Hero || Clock < 2.f) return;
        if (!Close.IsValid())
        {
            // -BladeRot=P,Y,R and -BladeOff=X,Y,Z try another grip.
            FString Grip;
            if (FParse::Value(FCommandLine::Get(), TEXT("BladeRot="), Grip, false))
            {
                TArray<FString> V; Grip.ParseIntoArray(V, TEXT(","));
                if (V.Num() == 3) Hero->BladeGripRotation = FRotator(FCString::Atof(*V[0]), FCString::Atof(*V[1]), FCString::Atof(*V[2]));
            }
            if (FParse::Value(FCommandLine::Get(), TEXT("BladeOff="), Grip, false))
            {
                TArray<FString> V; Grip.ParseIntoArray(V, TEXT(","));
                if (V.Num() == 3) Hero->BladeGripOffset = FVector(FCString::Atof(*V[0]), FCString::Atof(*V[1]), FCString::Atof(*V[2]));
            }
            if (FParse::Param(FCommandLine::Get(), TEXT("MapShotInfernal"))) { Hero->Shop.bInfernalSword = true; Hero->Shop.SwordDamage = 1.25f; }
            Hero->ApplySwordLook();
            Hero->SelectWeapon(1);
            // Where the fingers are in the hand bone's own space (to set the grip).
            const FTransform Hand = Hero->GetMesh()->GetSocketTransform(TEXT("RightHand"), RTS_World);
            TArray<FName> Bones;
            Hero->GetMesh()->GetBoneNames(Bones);
            for (const FName& Bone : Bones)
                if (Bone.ToString().Contains(TEXT("Right")) && (Bone.ToString().Contains(TEXT("Hand")) || Bone.ToString().Contains(TEXT("Fore"))))
                    UE_LOG(LogTemp, Display, TEXT("SWORD GRIP %s at %s"), *Bone.ToString(), *Hand.InverseTransformPosition(Hero->GetMesh()->GetSocketLocation(Bone)).ToString());
            Close = GetWorld()->SpawnActor<ACameraActor>();
            Close->GetCameraComponent()->SetFieldOfView(50.f);
            PC->SetViewTarget(Close.Get());
        }
        // Front, side, then front again mid-slash.
        const FVector At = Hero->GetActorLocation() + FVector(0, 0, 20);
        const FVector Dir = SwordShot == 1 ? Hero->GetActorRightVector() : Hero->GetActorForwardVector();
        Close->SetActorLocationAndRotation(At + Dir * 330.f + FVector(0, 0, 30), (-Dir).Rotation() + FRotator(-5.f, 0.f, 0.f));
        const float Times[] = {3.f, 3.6f, 4.6f};
        if (SwordShot == 2 && Clock > 4.2f && Clock < 4.3f) Hero->Attack();
        if (SwordShot < 3 && Clock > Times[SwordShot])
        {
            FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / FString::Printf(TEXT("Screenshots/MapShot/Sword_%d.png"), SwordShot), false, false);
            ++SwordShot;
        }
        else if (SwordShot >= 3 && Clock > 5.5f) FPlatformMisc::RequestExitWithStatus(false, 0);
        return;
    }
    // -MapShotBoss: the room's boss sites wake at once (to see a boss and its bar without fighting through the waves).
    if (FParse::Param(FCommandLine::Get(), TEXT("MapShotBoss")) && Clock > 1.5f)
        for (auto& Site : SpawnSites)
            if (Site && Site->bBoss && !Site->bActivated) { Site->bEnabled = true; Site->bActivated = true; }
    // ...and with -MapShotBossClose Hellgirl stands 4 m from the boss, facing it, before the first picture.
    static bool bMovedToBoss = false;
    if (FParse::Param(FCommandLine::Get(), TEXT("MapShotBossClose")) && Clock > 3.f && !bMovedToBoss)
        if (APawn* Hero = PC->GetPawn())
            for (TActorIterator<AArenaFighter> It(GetWorld()); It; ++It)
                if (It->bEnemy && It->bBossEncounter && It->IsAlive())
                {
                    bMovedToBoss = true;
                    const FVector Boss = It->GetActorLocation();
                    const FVector From = Boss + (Hero->GetActorLocation() - Boss).GetSafeNormal2D() * 400.f;
                    Hero->SetActorLocation(FVector(From.X, From.Y, Boss.Z + 30.f), false, nullptr, ETeleportType::TeleportPhysics);
                    PC->SetControlRotation(FRotator(-15.f, (Boss - From).Rotation().Yaw, 0.f));
                    break;
                }
    // -MapShotCombo=Points holds the combo meter at that value, to see how it looks on the HUD.
    float ComboPoints = -1.f;
    if (FParse::Value(FCommandLine::Get(), TEXT("MapShotCombo="), ComboPoints))
        if (auto* Hero = Cast<AArenaFighter>(PC->GetPawn())) Hero->SetComboPointsForPreview(ComboPoints);
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
