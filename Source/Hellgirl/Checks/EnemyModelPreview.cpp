#include "Levels/ArenaGameMode.h"
#include "Fighter/ArenaFighter.h"
#include "Enemies/EnemyTypes.h"
#include "Enemies/EnemyMovesetState.h"
#include "Animation/SkeletalMeshActor.h"
#include "Animation/AnimSequence.h"
#include "Camera/CameraActor.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

// -HellgirlMiniSuccubusPreview (at camp, with a GPU): the mini succubus at her game size, five times in a row in the
// air, each frozen in one of her flying clips (hover, fly, attack at contact, hit, end of death), with Hellgirl at the
// end of the row for scale; photographed wide and close up to Saved/Screenshots/MiniSuccubus_*.png.
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
    const FEnemyModelSlot& Model = GetDefault<UHellgirlEnemyModels>()->ForType(EHellgirlEnemyType::MiniSuccubus);
    if (Step == 0)
    {
        Step = 1;
        auto* Mesh = Model.Mesh.LoadSynchronous();
        if (!Mesh) { UE_LOG(LogTemp, Error, TEXT("MINI SUCCUBUS PREVIEW FAILED: no mesh")); FPlatformMisc::RequestExitWithStatus(false, 1); return; }
        if (APawn* Hero = PC->GetPawn()) Hero->SetActorLocationAndRotation(FVector(250.f, 560.f, 110.f), FRotator::ZeroRotator);
        struct FShot { const TCHAR* Clip; float At; };
        const FShot Shots[] = {{TEXT("Hover"), .25f}, {TEXT("Fly"), .5f}, {TEXT("Attack"), .6f}, {TEXT("Hit"), .25f}, {TEXT("Death"), 1.f}};
        for (int32 I = 0; I < UE_ARRAY_COUNT(Shots); ++I)
        {
            // In the air in a row across the camp, facing +X (the model slot's -90 yaw), at her game scale.
            auto* Actor = GetWorld()->SpawnActor<ASkeletalMeshActor>(FVector(250.f, -520.f + I * 230.f, 120.f), FRotator(0.f, -90.f, 0.f));
            auto* Comp = Actor->GetSkeletalMeshComponent();
            Comp->SetSkeletalMesh(Mesh);
            Actor->SetActorScale3D(Model.Scale);
            auto* Clip = LoadObject<UAnimSequence>(nullptr, *(Folder + TEXT("Animations/") + Shots[I].Clip + TEXT(".") + Shots[I].Clip));
            if (!Clip) { UE_LOG(LogTemp, Error, TEXT("MINI SUCCUBUS PREVIEW FAILED: no clip %s"), Shots[I].Clip); FPlatformMisc::RequestExitWithStatus(false, 1); return; }
            Comp->SetAnimationMode(EAnimationMode::AnimationSingleNode);
            Comp->PlayAnimation(Clip, false);
            Comp->Stop();
            Comp->SetPosition(Clip->GetPlayLength() * FMath::Min(Shots[I].At, .999f), false);
        }
        Camera = GetWorld()->SpawnActor<ACameraActor>(FVector(900.f, 20.f, 230.f), (FVector(250.f, 20.f, 130.f) - FVector(900.f, 20.f, 230.f)).Rotation());
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
        // Close up on the hover, the fly and the attack.
        Camera->SetActorLocationAndRotation(FVector(560.f, -290.f, 200.f), (FVector(250.f, -290.f, 150.f) - FVector(560.f, -290.f, 200.f)).Rotation());
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

// -HellgirlMiniSuccubusCheck (at camp): three live mini succubi must take to the air (whatever a spawn site asks for),
// hover well above the ground, dive at Hellgirl and hurt her, use their model at its game scale, and die when hit.
void AArenaGameMode::RunMiniSuccubusCheck(float Dt)
{
#if WITH_DEV_AUTOMATION_TESTS
    if (!FParse::Param(FCommandLine::Get(), TEXT("HellgirlMiniSuccubusCheck"))) return;
    static float Clock = 0.f;
    static TArray<TWeakObjectPtr<AArenaFighter>> Flyers;
    static float Highest = 0.f;
    static bool bDived = false;
    Clock += Dt;
    auto* Hero = Cast<AArenaFighter>(UGameplayStatics::GetPlayerPawn(this, 0));
    if (!Hero || Clock < 1.f) return;
    auto Finish = [](bool Passed, const FString& Why)
    {
        if (Passed) { UE_LOG(LogTemp, Display, TEXT("MINI SUCCUBUS CHECK PASSED: %s"), *Why); }
        else { UE_LOG(LogTemp, Error, TEXT("MINI SUCCUBUS CHECK FAILED: %s"), *Why); }
        FPlatformMisc::RequestExitWithStatus(false, Passed ? 0 : 1);
    };
    if (Flyers.IsEmpty())
    {
        Hero->MaxHealth = Hero->Health = 100000.f;
        Hero->SetActorLocation(FVector(-550.f, 0.f, 110.f));
        for (int32 I = 0; I < 3; ++I)
        {
            // Spawned as ground enemies on purpose: the type alone must make them fly.
            auto* Enemy = GetWorld()->SpawnActor<AArenaFighter>(FVector(-150.f, -250.f + I * 250.f, 110.f), FRotator::ZeroRotator);
            if (!Enemy) { Finish(false, TEXT("spawn")); return; }
            Enemy->MakeEnemy(1, false);
            Enemy->SetEnemyType(EHellgirlEnemyType::MiniSuccubus);
            Enemy->HomePosition = Enemy->GetActorLocation();
            Flyers.Add(Enemy);
        }
        return;
    }
    for (const auto& Flyer : Flyers)
        if (Flyer.IsValid() && Flyer->IsAlive())
        {
            Highest = FMath::Max(Highest, static_cast<float>(Flyer->GetActorLocation().Z));
            bDived |= Flyer->GetEnemyMove() == EEnemyMove::FlyingDive;
        }
    if (Clock < 14.f) return;
    for (const auto& Flyer : Flyers)
    {
        if (!Flyer.IsValid()) { Finish(false, TEXT("a mini succubus disappeared")); return; }
        if (!Flyer->bFlyingEnemy || Flyer->GetCharacterMovement()->MovementMode != MOVE_Flying) { Finish(false, TEXT("not flying")); return; }
        if (!Flyer->GetMesh()->GetSkeletalMeshAsset() || !Flyer->GetMesh()->GetSkeletalMeshAsset()->GetName().Contains(TEXT("MiniSuccubus"))
            || !FMath::IsNearlyEqual(Flyer->GetMesh()->GetRelativeScale3D().X, .5f, .01f)) { Finish(false, TEXT("model or scale")); return; }
    }
    const float Hurt = Hero->MaxHealth - Hero->Health;
    if (Highest < 250.f) { Finish(false, FString::Printf(TEXT("they never rose above the camp (highest %.0f)"), Highest)); return; }
    if (!bDived || Hurt <= 0.f) { Finish(false, FString::Printf(TEXT("no dive attack landed (dived %d, damage %.0f)"), bDived, Hurt)); return; }
    AArenaFighter* Target = Flyers[0].Get();
    Target->ApplyPhysicsDamage(1000000.f, FVector::ZeroVector);
    if (Target->IsAlive()) { Finish(false, TEXT("did not die")); return; }
    Finish(true, FString::Printf(TEXT("three flew (highest %.0f cm), dived, dealt %.0f damage, half scale, died when hit"), Highest, Hurt));
#endif
}

// -HellgirlCourtFlyersCheck (Succubus Court): the five review mini succubi fly about and dive at Hellgirl without
// taking any health, and one that is killed is replaced a few seconds later.
void AArenaGameMode::RunCourtFlyersCheck(float Dt)
{
#if WITH_DEV_AUTOMATION_TESTS
    if (!bSuccubusCourt || !FParse::Param(FCommandLine::Get(), TEXT("HellgirlCourtFlyersCheck"))) return;
    static float Clock = 0.f;
    static bool bDived = false, bKilled = false;
    static TWeakObjectPtr<AArenaFighter> Victim;
    Clock += Dt;
    auto* Hero = Cast<AArenaFighter>(UGameplayStatics::GetPlayerPawn(this, 0));
    if (!Hero) return;
    auto Finish = [](bool Passed, const FString& Why)
    {
        if (Passed) { UE_LOG(LogTemp, Display, TEXT("COURT FLYERS CHECK PASSED: %s"), *Why); }
        else { UE_LOG(LogTemp, Error, TEXT("COURT FLYERS CHECK FAILED: %s"), *Why); }
        FPlatformMisc::RequestExitWithStatus(false, Passed ? 0 : 1);
    };
    for (const auto& Flyer : CourtFlyers)
        if (Flyer.IsValid() && Flyer->IsAlive()) bDived |= Flyer->GetEnemyMove() == EEnemyMove::FlyingDive;
    if (Clock > 12.f && !bKilled)
    {
        int32 Alive = 0;
        for (const auto& Flyer : CourtFlyers) Alive += Flyer.IsValid() && Flyer->IsAlive() && Flyer->bFlyingEnemy;
        if (Alive != 5) { Finish(false, FString::Printf(TEXT("%d of 5 flyers"), Alive)); return; }
        if (!bDived) { Finish(false, TEXT("none dived at Hellgirl")); return; }
        if (Hero->Health < Hero->MaxHealth) { Finish(false, FString::Printf(TEXT("they hurt Hellgirl (%.0f health)"), Hero->Health)); return; }
        Victim = CourtFlyers[0];
        Victim->ApplyPhysicsDamage(1000000.f, FVector::ZeroVector);
        bKilled = true;
    }
    if (bKilled && Clock > 18.f)
    {
        const bool Replaced = CourtFlyers[0].IsValid() && CourtFlyers[0] != Victim && CourtFlyers[0]->IsAlive();
        if (!Replaced) { Finish(false, TEXT("the killed flyer was not replaced")); return; }
        if (Hero->Health < Hero->MaxHealth) { Finish(false, TEXT("they hurt Hellgirl")); return; }
        Finish(true, TEXT("five flew and dived at Hellgirl for no damage; a killed one was replaced"));
    }
#endif
}
