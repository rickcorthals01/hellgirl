#include "Levels/ArenaGameMode.h"
#include "Fighter/ArenaFighter.h"
#include "Enemies/EnemyTypes.h"
#include "Enemies/EnemyMovesetState.h"
#include "Animation/SkeletalMeshActor.h"
#include "Animation/AnimSequence.h"
#include "Camera/CameraActor.h"
#include "Components/PointLightComponent.h"
#include "Engine/PointLight.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

// Preview modes (at camp, with a GPU): pages of five enemy models in a row, each frozen in a clip at a fraction of its
// length, with Hellgirl near the row for scale; each page is photographed to Saved/Screenshots/<Name>_<Page>.png.
//   -HellgirlMiniSuccubusPreview: the mini succubus in the air (hover, fly, attack at contact, hit, end of death),
//    wide and close up.
//   -HellgirlGoblinPreview: goblins on the ground, each page one clip from start to end: the dagger slash, the quick
//    slash, the Goblin Queen's slash and the hit.
void AArenaGameMode::RunEnemyModelPreview(float Dt)
{
#if WITH_DEV_AUTOMATION_TESTS
    struct FShot { EHellgirlEnemyType Type; const TCHAR* Clip; float At; };
    struct FPage { const TCHAR* Label; FShot Shots[5]; };
    const bool bGoblins = FParse::Param(FCommandLine::Get(), TEXT("HellgirlGoblinPreview"));
    if (!bGoblins && !FParse::Param(FCommandLine::Get(), TEXT("HellgirlMiniSuccubusPreview"))) return;
    const TCHAR* Name = bGoblins ? TEXT("Goblin") : TEXT("MiniSuccubus");
    static float Clock = 0.f;
    static int32 Page = -1;
    static float PageClock = 0.f;
    static bool bPosed = false, bTaken = false;
    static TArray<TWeakObjectPtr<ASkeletalMeshActor>> Actors;
    static TWeakObjectPtr<ACameraActor> Camera;
    Clock += Dt;
    auto* PC = UGameplayStatics::GetPlayerController(this, 0);
    if (!PC || Clock < 1.f) return;
    auto Fail = [Name](const FString& Why)
    {
        UE_LOG(LogTemp, Error, TEXT("%s PREVIEW FAILED: %s"), Name, *Why);
        FPlatformMisc::RequestExitWithStatus(false, 1);
    };
    using T = EHellgirlEnemyType;
    const FShot Flyers[5] = {{T::MiniSuccubus, TEXT("Hover"), .25f}, {T::MiniSuccubus, TEXT("Fly"), .5f},
        {T::MiniSuccubus, TEXT("Attack"), .6f}, {T::MiniSuccubus, TEXT("Hit"), .25f}, {T::MiniSuccubus, TEXT("Death"), 1.f}};
    auto Sweep = [](T Type, const TCHAR* Clip, float A, float B, float C, float D, float E)
    {
        FPage Result{Clip, {{Type, Clip, A}, {Type, Clip, B}, {Type, Clip, C}, {Type, Clip, D}, {Type, Clip, E}}};
        return Result;
    };
    TArray<FPage> Pages;
    if (bGoblins)
    {
        // Each sweep passes through the clip's contact point (GoblinSlash .29, GoblinQuickSlash .57).
        Pages.Add(Sweep(T::Goblins, TEXT("GoblinSlash"), 0.f, .15f, .2895f, .5f, .85f));
        Pages.Add(Sweep(T::Goblins, TEXT("GoblinQuickSlash"), 0.f, .3f, .5714f, .75f, .95f));
        Pages.Add(Sweep(T::GoblinQueen, TEXT("GoblinSlash"), 0.f, .15f, .2895f, .5f, .85f));
        Pages.Add(Sweep(T::Goblins, TEXT("GoblinHit"), 0.f, .2f, .4f, .6f, .9f));
        Pages[2].Label = TEXT("QueenSlash");
    }
    else
    {
        Pages.Add({TEXT("Row"), {Flyers[0], Flyers[1], Flyers[2], Flyers[3], Flyers[4]}});
        Pages.Add({TEXT("Close"), {Flyers[0], Flyers[1], Flyers[2], Flyers[3], Flyers[4]}});
    }
    // Flyers hang in the air in a wide row; goblins stand on the ground (their capsule centre is 110, the model 88
    // below it) in a tighter row.
    const float Height = bGoblins ? 22.f : 120.f;
    const float Spacing = bGoblins ? 150.f : 230.f;
    const float First = bGoblins ? -300.f : -520.f;
    if (Page == -2) return; // done
    if (Page < 0)
    {
        if (APawn* Hero = PC->GetPawn()) Hero->SetActorLocationAndRotation(FVector(250.f, 560.f, 110.f), FRotator::ZeroRotator);
        for (int32 I = 0; I < 5; ++I)
            // Facing +X (the model slot's -90 yaw), toward the camera.
            Actors.Add(GetWorld()->SpawnActor<ASkeletalMeshActor>(FVector(250.f, First + I * Spacing, Height), FRotator(0.f, -90.f, 0.f)));
        Camera = GetWorld()->SpawnActor<ACameraActor>(FVector::ZeroVector, FRotator::ZeroRotator);
        PC->SetViewTarget(Camera.Get());
        Page = 0;
    }
    if (Page >= Pages.Num())
    {
        PageClock += Dt;
        if (PageClock > .8f)
        {
            UE_LOG(LogTemp, Display, TEXT("%s PREVIEW DONE"), Name);
            FPlatformMisc::RequestExitWithStatus(false, 0);
            Page = -2;
        }
        return;
    }
    if (!Camera.IsValid()) return;
    if (!bPosed)
    {
        // Pose this page's five models and aim the camera.
        const FPage& P = Pages[Page];
        for (int32 I = 0; I < 5; ++I)
        {
            const FEnemyModelSlot& Model = GetDefault<UHellgirlEnemyModels>()->ForType(P.Shots[I].Type);
            auto* Mesh = Model.Mesh.LoadSynchronous();
            if (!Mesh || !Actors[I].IsValid()) { Fail(TEXT("no mesh")); return; }
            auto* Comp = Actors[I]->GetSkeletalMeshComponent();
            Comp->SetSkeletalMesh(Mesh);
            Actors[I]->SetActorScale3D(Model.Scale);
            const FString Folder = FPaths::GetPath(Model.Mesh.ToSoftObjectPath().GetLongPackageName()) / TEXT("Animations/");
            auto* Clip = LoadObject<UAnimSequence>(nullptr, *(Folder + P.Shots[I].Clip + TEXT(".") + P.Shots[I].Clip));
            if (!Clip) { Fail(FString::Printf(TEXT("no clip %s"), P.Shots[I].Clip)); return; }
            Comp->SetAnimationMode(EAnimationMode::AnimationSingleNode);
            Comp->PlayAnimation(Clip, false);
            Comp->Stop();
            Comp->SetPosition(Clip->GetPlayLength() * FMath::Min(P.Shots[I].At, .999f), false);
        }
        const bool bClose = bGoblins || Page == 1;
        const float Across = bGoblins ? 0.f : bClose ? -290.f : 20.f;
        const FVector Eye(bGoblins ? 760.f : bClose ? 560.f : 900.f, Across, Height + (bGoblins ? 90.f : bClose ? 80.f : 110.f));
        Camera->SetActorLocationAndRotation(Eye, (FVector(250.f, Across, Height + (bGoblins ? 45.f : bClose ? 30.f : 10.f)) - Eye).Rotation());
        bPosed = true;
        bTaken = false;
        PageClock = 0.f;
    }
    PageClock += Dt;
    if (!bTaken && PageClock > 1.2f)
    {
        FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / FString::Printf(TEXT("Screenshots/%s_%s.png"), Name, Pages[Page].Label), false, false);
        bTaken = true;
    }
    if (bTaken && PageClock > 1.8f)
    {
        ++Page;
        bPosed = false;
        PageClock = 0.f;
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

// -HellgirlOutfitPreview=<Outfit> (at camp, with a GPU): the outfit alone on the camp's edge, frozen a moment into its
// idle, lit by a key and a fill light, photographed from the front, three-quarters and behind, and close on the face,
// to Saved/Screenshots/Outfit/<Outfit>_<n>.png.
void AArenaGameMode::RunOutfitPreview(float Dt)
{
#if WITH_DEV_AUTOMATION_TESTS
    FString Outfit;
    if (!FParse::Value(FCommandLine::Get(), TEXT("HellgirlOutfitPreview="), Outfit)) return;
    static float Clock = 0.f;
    static int32 Shot = -1;
    static TWeakObjectPtr<ACameraActor> Camera;
    Clock += Dt;
    auto* PC = UGameplayStatics::GetPlayerController(this, 0);
    if (!PC || Clock < 1.f) return;
    const FVector Spot(-600.f, -300.f, 0.f); // open ground west of the campfire
    if (Shot < 0)
    {
        Shot = 0;
        if (APawn* Hero = PC->GetPawn()) Hero->SetActorLocation(FVector(600.f, 900.f, 110.f));
        const FString Folder = FString::Printf(TEXT("/Game/Hellgirl/Outfits/%s/"), *Outfit);
        auto* Mesh = LoadObject<USkeletalMesh>(nullptr, *(Folder + Outfit + TEXT(".") + Outfit));
        auto* Idle = LoadObject<UAnimSequence>(nullptr, *(Folder + TEXT("Animations/Idle.Idle")));
        if (!Mesh || !Idle) { UE_LOG(LogTemp, Error, TEXT("OUTFIT PREVIEW FAILED: no mesh or idle for %s"), *Outfit); FPlatformMisc::RequestExitWithStatus(false, 1); return; }
        // Facing +X like the enemy rows (the models face +Y in their own space).
        auto* Actor = GetWorld()->SpawnActor<ASkeletalMeshActor>(Spot, FRotator(0.f, -90.f, 0.f));
        auto* Comp = Actor->GetSkeletalMeshComponent();
        Comp->SetSkeletalMesh(Mesh);
        Comp->SetAnimationMode(EAnimationMode::AnimationSingleNode);
        Comp->PlayAnimation(Idle, false);
        Comp->Stop();
        Comp->SetPosition(Idle->GetPlayLength() * .3f, false);
        auto Light = [&](FVector At, float Intensity, FLinearColor Color)
        {
            auto* L = GetWorld()->SpawnActor<APointLight>(At, FRotator::ZeroRotator);
            L->PointLightComponent->SetMobility(EComponentMobility::Movable);
            L->PointLightComponent->SetIntensity(Intensity);
            L->PointLightComponent->SetLightColor(Color);
            L->PointLightComponent->SetAttenuationRadius(900.f);
        };
        Light(Spot + FVector(260.f, -200.f, 230.f), 30000.f, FLinearColor(1.f, .92f, .85f));
        Light(Spot + FVector(200.f, 260.f, 150.f), 9000.f, FLinearColor(.75f, .82f, 1.f));
        Light(Spot + FVector(-250.f, 0.f, 220.f), 12000.f, FLinearColor(.9f, .9f, 1.f));
        Camera = GetWorld()->SpawnActor<ACameraActor>();
        PC->SetViewTarget(Camera.Get());
        return;
    }
    struct FView { FVector Eye; FVector Look; };
    const FView Views[] = {
        {Spot + FVector(330.f, 0.f, 100.f), Spot + FVector(0.f, 0.f, 90.f)},          // front
        {Spot + FVector(240.f, -230.f, 110.f), Spot + FVector(0.f, 0.f, 90.f)},       // three-quarters
        {Spot + FVector(-330.f, 0.f, 100.f), Spot + FVector(0.f, 0.f, 90.f)},         // back
        {Spot + FVector(95.f, -25.f, 158.f), Spot + FVector(0.f, 0.f, 150.f)},        // face
    };
    if (!Camera.IsValid() || Shot >= static_cast<int32>(UE_ARRAY_COUNT(Views))) return;
    Camera->SetActorLocationAndRotation(Views[Shot].Eye, (Views[Shot].Look - Views[Shot].Eye).Rotation());
    if (Clock > 2.5f + Shot * 1.2f)
    {
        FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / FString::Printf(TEXT("Screenshots/Outfit/%s_%d.png"), *Outfit, Shot), false, false);
        if (++Shot >= static_cast<int32>(UE_ARRAY_COUNT(Views)))
        {
            UE_LOG(LogTemp, Display, TEXT("OUTFIT PREVIEW DONE"));
            FPlatformMisc::RequestExitWithStatus(false, 0);
        }
    }
#endif
}
