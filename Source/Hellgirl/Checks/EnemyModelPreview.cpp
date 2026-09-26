#include "Levels/ArenaGameMode.h"
#include "Animation/SkeletalMeshActor.h"
#include "Animation/AnimSequence.h"
#include "Camera/CameraActor.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

// -HellgirlMiniSuccubusPreview (at camp, with a GPU): the rigged mini succubus six times in a row, each frozen in
// one of her clips (idle, walk, run, attack at contact, hit, end of death), photographed wide and close up to
// Saved/Screenshots/MiniSuccubus_*.png.
void AArenaGameMode::RunEnemyModelPreview(float Dt)
{
#if WITH_DEV_AUTOMATION_TESTS
    if (!FParse::Param(FCommandLine::Get(), TEXT("HellgirlMiniSuccubusPreview"))) return;
    static float Clock = 0.f;
    static int32 Step = 0;
    static TWeakObjectPtr<ACameraActor> Camera;
    Clock += Dt;
    auto* PC = UGameplayStatics::GetPlayerController(this, 0);
    if (!PC || Clock < 1.f) return;
    const FString Folder = TEXT("/Game/Enemies/MiniSuccubus/");
    if (Step == 0)
    {
        Step = 1;
        auto* Mesh = LoadObject<USkeletalMesh>(nullptr, *(Folder + TEXT("MiniSuccubus.MiniSuccubus")));
        if (!Mesh) { UE_LOG(LogTemp, Error, TEXT("MINI SUCCUBUS PREVIEW FAILED: no mesh")); FPlatformMisc::RequestExitWithStatus(false, 1); return; }
        if (APawn* Hero = PC->GetPawn()) Hero->SetActorHiddenInGame(true);
        struct FShot { const TCHAR* Clip; float At; };
        const FShot Shots[] = {{TEXT("Idle"), .5f}, {TEXT("Walk"), .25f}, {TEXT("Run"), .3f}, {TEXT("Attack"), .44f}, {TEXT("Hit"), .4f}, {TEXT("Death"), 1.f}};
        for (int32 I = 0; I < UE_ARRAY_COUNT(Shots); ++I)
        {
            // In a row across the camp, facing +X (the enemy slots turn Meshy models by -90 yaw the same way).
            auto* Actor = GetWorld()->SpawnActor<ASkeletalMeshActor>(FVector(250.f, -625.f + I * 250.f, 0.f), FRotator(0.f, -90.f, 0.f));
            auto* Comp = Actor->GetSkeletalMeshComponent();
            Comp->SetSkeletalMesh(Mesh);
            auto* Clip = LoadObject<UAnimSequence>(nullptr, *(Folder + TEXT("Animations/") + Shots[I].Clip + TEXT(".") + Shots[I].Clip));
            if (!Clip) { UE_LOG(LogTemp, Error, TEXT("MINI SUCCUBUS PREVIEW FAILED: no clip %s"), Shots[I].Clip); FPlatformMisc::RequestExitWithStatus(false, 1); return; }
            Comp->SetAnimationMode(EAnimationMode::AnimationSingleNode);
            Comp->PlayAnimation(Clip, false);
            Comp->Stop();
            Comp->SetPosition(Clip->GetPlayLength() * FMath::Min(Shots[I].At, .999f), false);
        }
        Camera = GetWorld()->SpawnActor<ACameraActor>(FVector(1000.f, 0.f, 230.f), (FVector(250.f, 0.f, 95.f) - FVector(1000.f, 0.f, 230.f)).Rotation());
        PC->SetViewTarget(Camera.Get());
        return;
    }
    if (Step == 1 && Clock > 3.f)
    {
        FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("Screenshots/MiniSuccubus_Row.png"), false, false);
        Step = 2;
    }
    if (Step == 2 && Clock > 3.6f && Camera.IsValid())
    {
        // Close up on the walk and the attack.
        Camera->SetActorLocationAndRotation(FVector(610.f, -330.f, 170.f), (FVector(250.f, -250.f, 100.f) - FVector(610.f, -330.f, 170.f)).Rotation());
        Step = 3;
    }
    if (Step == 3 && Clock > 4.6f)
    {
        FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("Screenshots/MiniSuccubus_Close.png"), false, false);
        Step = 4;
    }
    if (Step == 4 && Clock > 5.4f)
    {
        UE_LOG(LogTemp, Display, TEXT("MINI SUCCUBUS PREVIEW DONE"));
        FPlatformMisc::RequestExitWithStatus(false, 0);
        Step = 5;
    }
#endif
}
