// The Frozen Maze's hunt: one Deprived at a time hunts Hellgirl (Enemies/DeprivedCombat.cpp). The first rises a few
// seconds after she arrives; when one falls its Souls drop and the next rises DeprivedRiseDelay later, from the nearest
// lair she cannot see that is at least DeprivedMinSteps squares' walk from her (Rules/EnemyTuning.h). The maze steers it:
// a few times a second it works out the walk to her from every square, and points it at the next square of that walk.
// Check: -HellgirlDeprivedCheck.
#include "Levels/ArenaGameMode.h"
#include "Rules/FrozenMazeRules.h"
#include "Rules/EnemyTuning.h"
#include "Enemies/EnemyMovesetState.h"
#include "Fighter/ArenaFighter.h"
#include "Progress/CoinPickup.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Particles/ParticleSystem.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

int32 AArenaGameMode::PickDeprivedLair(const FVector& Hero) const
{
    using namespace FrozenMaze;
    const FCell From = CellAt(FVector2D(Hero));
    if (!Inside(From.X, From.Y)) return -1;
    const TArray<int32> Steps = Walk({From});
    FCollisionQueryParams Query(SCENE_QUERY_STAT(DeprivedLairSight), false);
    Query.AddIgnoredActor(UGameplayStatics::GetPlayerPawn(this, 0));
    if (MazeDeprived.IsValid()) Query.AddIgnoredActor(MazeDeprived.Get());
    int32 Best = -1, BestSteps = MAX_int32;
    for (int32 I = 0; I < static_cast<int32>(UE_ARRAY_COUNT(FrozenMazeLayout::Lairs)); ++I)
    {
        const FCell Lair = FrozenMazeLayout::Lairs[I];
        const int32 Away = Steps[Index(Lair.X, Lair.Y)];
        if (Away < EnemyTuning::DeprivedMinSteps || Away >= BestSteps) continue;
        // Out of her sight: a wall stands between her eyes and the pool.
        const FVector2D P = Centre(Lair);
        FHitResult Wall;
        if (!GetWorld()->LineTraceSingleByChannel(Wall, Hero + FVector(0.f, 0.f, 60.f), FVector(P.X, P.Y, 120.f), ECC_Visibility, Query)) continue;
        Best = I;
        BestSteps = Away;
    }
    return Best;
}

AArenaFighter* AArenaGameMode::RaiseDeprived(int32 Lair)
{
    using namespace FrozenMaze;
    if (Lair < 0 || Lair >= static_cast<int32>(UE_ARRAY_COUNT(FrozenMazeLayout::Lairs))) return nullptr;
    const FVector2D P = Centre(FrozenMazeLayout::Lairs[Lair]);
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
    auto* Deprived = GetWorld()->SpawnActor<AArenaFighter>(FVector(P.X, P.Y, 110.f), FRotator::ZeroRotator, Params);
    if (!Deprived) return nullptr;
    Deprived->MakeEnemy(EnemyTuning::DeprivedDifficulty, false);
    Deprived->SetEnemyType(EHellgirlEnemyType::Deprived);
    Deprived->HomePosition = Deprived->GetActorLocation();
    // It rises out of its shadow pool in a gust of black smoke.
    if (auto* Smoke = LoadObject<UParticleSystem>(nullptr, TEXT("/Game/Realistic_Starter_VFX_Pack_Vol2/Particles/Smoke/P_Smoke_A.P_Smoke_A")))
        UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), Smoke, FVector(P.X, P.Y, 40.f), FRotator::ZeroRotator, FVector(1.2f));
    ++MazeDeprivedRisen;
    MazeDeprivedLair = Lair;
    UE_LOG(LogTemp, Display, TEXT("MAZE HUNT: Deprived %d rises at lair D%d"), MazeDeprivedRisen, Lair + 1);
    return Deprived;
}

void AArenaGameMode::TickMazeDeprived(float Dt, AArenaFighter* Hero)
{
    using namespace FrozenMaze;
    AArenaFighter* Deprived = MazeDeprived.Get();
    // It falls: its Souls drop, and the next one rises a while later.
    if (Deprived && !Deprived->IsAlive() && !bMazeDeprivedDown)
    {
        bMazeDeprivedDown = true;
        EnemyDefeated(Deprived->GetActorLocation());
        MazeDeprivedClock = EnemyTuning::DeprivedRiseDelay;
    }
    if (!Deprived || !Deprived->IsAlive())
    {
        if (!Hero || !Hero->IsAlive() || (MazeDeprivedClock -= Dt) > 0.f) return;
        const int32 Lair = PickDeprivedLair(Hero->GetActorLocation());
        if (Lair < 0) { MazeDeprivedClock = 1.f; return; } // every lair is in sight or too near: try again shortly
        if (Deprived) Deprived->Destroy();                 // the fallen one's body
        MazeDeprived = RaiseDeprived(Lair);
        bMazeDeprivedDown = false;
        MazeHuntClock = 0.f;
        return;
    }
    if (!Hero) return;
    // The walk to her from every square, a few times a second; it heads for the next square along it.
    if ((MazeHuntClock -= Dt) <= 0.f)
    {
        MazeHuntClock = .25f;
        const FCell Target = CellAt(FVector2D(Hero->GetActorLocation()));
        MazeHuntSteps = Inside(Target.X, Target.Y) ? Walk({Target}) : TArray<int32>();
    }
    Deprived->bHasHuntWaypoint = false;
    const FCell At = CellAt(FVector2D(Deprived->GetActorLocation()));
    if (!MazeHuntSteps.Num() || !Inside(At.X, At.Y) || MazeHuntSteps[Index(At.X, At.Y)] <= 0) return;
    for (int32 D = 0; D < 4; ++D)
    {
        const int32 X = At.X + StepX[D], Y = At.Y + StepY[D];
        if (!Open(At.X, At.Y, StepX[D], StepY[D]) || MazeHuntSteps[Index(X, Y)] != MazeHuntSteps[Index(At.X, At.Y)] - 1) continue;
        const FVector2D Next = Centre(X, Y);
        Deprived->HuntWaypoint = FVector(Next.X, Next.Y, Deprived->GetActorLocation().Z);
        Deprived->bHasHuntWaypoint = true;
        break;
    }
}

void AArenaGameMode::RunDeprivedCheck(float Dt)
{
#if WITH_DEV_AUTOMATION_TESTS
    // -HellgirlDeprivedPreview (needs a GPU): a Deprived rises 5 m in front of her; close-ups of it standing, running at
    // her and charging its execute, to Saved/Screenshots/Deprived/<shot>.png.
    if (FParse::Param(FCommandLine::Get(), TEXT("HellgirlDeprivedPreview")))
    {
        static float Clock = 0.f;
        static int32 Shot = 0;
        static TWeakObjectPtr<ACameraActor> Camera;
        Clock += Dt;
        auto* PC = UGameplayStatics::GetPlayerController(this, 0);
        auto* Hero = PC ? Cast<AArenaFighter>(PC->GetPawn()) : nullptr;
        if (!Hero || Clock < 3.f) return;
        Hero->MaxHealth = Hero->Health = 100000.f;
        if (!MazeDeprived.IsValid())
        {
            MazeDeprived = RaiseDeprived(FMath::Max(0, PickDeprivedLair(Hero->GetActorLocation())));
            if (AArenaFighter* D = MazeDeprived.Get())
            {
                const FVector At = Hero->GetActorLocation() + Hero->GetActorForwardVector() * 700.f;
                D->SetActorLocationAndRotation(At, (-Hero->GetActorForwardVector()).Rotation(), false, nullptr, ETeleportType::TeleportPhysics);
            }
            Camera = GetWorld()->SpawnActor<ACameraActor>();
            Camera->GetCameraComponent()->SetFieldOfView(45.f);
            PC->SetViewTarget(Camera.Get());
        }
        AArenaFighter* D = MazeDeprived.Get();
        if (!D || !Camera.IsValid()) return;
        const FVector Side = Hero->GetActorRightVector();
        const FVector Look = D->GetActorLocation() + FVector(0.f, 0.f, 40.f);
        Camera->SetActorLocationAndRotation(Look - D->GetActorForwardVector() * -420.f + Side * 120.f + FVector(0.f, 0.f, 30.f),
            (Look - (Look - D->GetActorForwardVector() * -420.f + Side * 120.f + FVector(0.f, 0.f, 30.f))).Rotation());
        const float Times[] = {3.3f, 4.2f, 5.6f, 6.4f};
        if (Shot < 4 && Clock >= Times[Shot])
            FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / FString::Printf(TEXT("Screenshots/Deprived/%d.png"), Shot++), false, false);
        if (Shot >= 4 && Clock > 7.f) FPlatformMisc::RequestExitWithStatus(false, 0);
        return;
    }
    if (!FParse::Param(FCommandLine::Get(), TEXT("HellgirlDeprivedCheck"))) return;
    using namespace FrozenMaze;
    static int32 Phase = 0, FirstLair = -1, StartSteps = 0, SwingsSeen = 0;
    static float Clock = 0.f, PhaseClock = 0.f, BoostStartedAt = -1.f, BoostLasted = 0.f, CooldownSeen = 0.f, HealthBefore = 0.f, DeprivedHealthBefore = 0.f, DownAt = 0.f;
    static bool BoostSpeedRight = true, Dodged = false;
    static EEnemyMove LastMove = EEnemyMove::None;
    static TWeakObjectPtr<AArenaFighter> First;
    Clock += Dt;
    PhaseClock += Dt;
    auto* Hero = Cast<AArenaFighter>(UGameplayStatics::GetPlayerPawn(this, 0));
    if (Phase == 99 || !Hero || Clock < 1.f) return;
    auto Finish = [&](bool Passed, const FString& Why)
    {
        if (Passed) { UE_LOG(LogTemp, Display, TEXT("DEPRIVED CHECK PASSED: %s"), *Why); }
        else { UE_LOG(LogTemp, Error, TEXT("DEPRIVED CHECK FAILED: %s"), *Why); }
        Phase = 99;
        FPlatformMisc::RequestExitWithStatus(false, Passed ? 0 : 1);
    };
    auto Next = [&](int32 P) { Phase = P; PhaseClock = 0.f; };
    auto StepsBetween = [](const FVector& A, const FVector& B)
    {
        const FCell From = CellAt(FVector2D(A)), To = CellAt(FVector2D(B));
        return Inside(From.X, From.Y) && Inside(To.X, To.Y) ? Walk({From})[Index(To.X, To.Y)] : -1;
    };
    auto Hidden = [&](const FVector& From, const FVector& To)
    {
        FCollisionQueryParams Query(SCENE_QUERY_STAT(DeprivedCheckSight), false, Hero);
        if (MazeDeprived.IsValid()) Query.AddIgnoredActor(MazeDeprived.Get());
        FHitResult Wall;
        return GetWorld()->LineTraceSingleByChannel(Wall, From + FVector(0.f, 0.f, 60.f), FVector(To.X, To.Y, 120.f), ECC_Visibility, Query);
    };
    AArenaFighter* Deprived = MazeDeprived.Get();
    // How long until the execute's hit lands, from its attack clock.
    auto UntilHit = [&]() { return Deprived->GetAttackClock() - (EnemyTuning::DeprivedExecuteSeconds - EnemyTuning::DeprivedExecuteCharge); };

    if (Phase == 0)
    {
        // She stands in her spawn room, far too tough to fall during the check. The first rises now, out of her sight.
        Hero->MaxHealth = Hero->Health = 100000.f;
        FirstLair = PickDeprivedLair(Hero->GetActorLocation());
        if (FirstLair < 0) return Finish(false, TEXT("no lair to rise from"));
        const FVector2D Lair = Centre(FrozenMazeLayout::Lairs[FirstLair]);
        if (StepsBetween(Hero->GetActorLocation(), FVector(Lair, 0.f)) < EnemyTuning::DeprivedMinSteps || !Hidden(Hero->GetActorLocation(), FVector(Lair, 0.f)))
            return Finish(false, TEXT("the first lair is in her sight or too near"));
        MazeDeprivedClock = 0.f;
        Next(1);
        return;
    }
    if (Phase == 1)
    {
        // It rises at that lair, with twice an ordinary enemy's health.
        if (!Deprived) { if (PhaseClock > 2.f) Finish(false, TEXT("no Deprived rose")); return; }
        const FVector2D Lair = Centre(FrozenMazeLayout::Lairs[FirstLair]);
        if (FVector::Dist2D(Deprived->GetActorLocation(), FVector(Lair, 0.f)) > 250.f) return Finish(false, TEXT("it rose away from its lair"));
        const float Expected = EnemyTuning::OrdinaryHealth(EnemyTuning::DeprivedDifficulty) * EnemyTuning::DeprivedHealthScale;
        if (Deprived->EnemyType != EHellgirlEnemyType::Deprived || !FMath::IsNearlyEqual(Deprived->MaxHealth, Expected, .5f))
            return Finish(false, FString::Printf(TEXT("it has %.1f health, not %.1f"), Deprived->MaxHealth, Expected));
        First = Deprived;
        StartSteps = StepsBetween(Deprived->GetActorLocation(), Hero->GetActorLocation());
        Next(2);
        return;
    }
    if (!Deprived && Phase < 5) return Finish(false, TEXT("the Deprived vanished"));
    if (Phase == 2)
    {
        // It hunts her through the maze, boosting: twice its speed for 6 s, then a 5 s cooldown.
        if (Deprived->GetDeprivedBoost() > 0.f)
        {
            if (BoostStartedAt < 0.f) BoostStartedAt = Clock;
            // (Its speed is set before its tactics each moment, so the boost's first and last instants are skipped.)
            const float Left = Deprived->GetDeprivedBoost();
            if (Left > .1f && Left < EnemyTuning::DeprivedBoostSeconds - .1f && Deprived->GetAttackClock() <= 0.f && !FMath::IsNearlyEqual(Deprived->GetCharacterMovement()->MaxWalkSpeed, Deprived->WalkSpeed * EnemyTuning::DeprivedBoostScale, 1.f))
                BoostSpeedRight = false;
        }
        else if (BoostStartedAt >= 0.f && BoostLasted <= 0.f) BoostLasted = Clock - BoostStartedAt;
        CooldownSeen = FMath::Max(CooldownSeen, Deprived->GetDeprivedBoostCooldown());
        if (Deprived->GetEnemyMove() == EEnemyMove::DeprivedExecute)
        {
            // The execute's charge: half a second from its start to the hit.
            if (UntilHit() < EnemyTuning::DeprivedExecuteCharge - .06f || UntilHit() > EnemyTuning::DeprivedExecuteCharge + .01f)
                return Finish(false, FString::Printf(TEXT("the execute charges %.2f s, not %.2f"), UntilHit(), EnemyTuning::DeprivedExecuteCharge));
            DeprivedHealthBefore = Deprived->Health;
            Next(3);
            return;
        }
        if (PhaseClock > 55.f)
            return Finish(false, FString::Printf(TEXT("it did not reach her through the maze (%d of %d squares still to go)"),
                StepsBetween(Deprived->GetActorLocation(), Hero->GetActorLocation()), StartSteps));
        return;
    }
    if (Phase == 3)
    {
        // She parries it: a perfect dodge while it charges counters it; she takes nothing.
        if (!Dodged && Deprived->GetEnemyMove() == EEnemyMove::DeprivedExecute && UntilHit() <= .25f)
        {
            Hero->Dodge();
            Dodged = true;
            return;
        }
        if (!Dodged) { if (PhaseClock > 2.f) Finish(false, TEXT("the execute stopped before she could dodge")); return; }
        if (PhaseClock < .8f) return;
        if (Deprived->Health >= DeprivedHealthBefore || Hero->Health < Hero->MaxHealth || !Deprived->IsDeprivedExecuteUsed())
            return Finish(false, FString::Printf(TEXT("the parry failed (it %.0f -> %.0f, she %.0f)"), DeprivedHealthBefore, Deprived->Health, Hero->Health));
        HealthBefore = Hero->Health;
        LastMove = EEnemyMove::None;
        Next(4);
        return;
    }
    if (Phase == 4)
    {
        // After its execute: slow slices and claws, one after the other, never the execute again; they hurt her.
        const EEnemyMove Move = Deprived->GetEnemyMove();
        if (Move != LastMove && Move != EEnemyMove::None)
        {
            const EEnemyMove Expected = SwingsSeen % 2 ? EEnemyMove::DeprivedClaw : EEnemyMove::DeprivedSlice;
            if (Move != Expected) return Finish(false, FString::Printf(TEXT("swing %d was move %d, not %d"), SwingsSeen + 1, static_cast<int32>(Move), static_cast<int32>(Expected)));
            ++SwingsSeen;
        }
        LastMove = Move;
        if (SwingsSeen >= 2 && Move == EEnemyMove::None)
        {
            if (Hero->Health >= HealthBefore) return Finish(false, TEXT("its slices and claws do not hurt her"));
            Deprived->Health = 0.f; // it falls (no wallet writes from checks)
            DownAt = Clock;
            Next(5);
            return;
        }
        if (PhaseClock > 25.f) return Finish(false, FString::Printf(TEXT("only %d swings after the execute"), SwingsSeen));
        return;
    }
    if (Phase == 5)
    {
        // Its Souls drop; the next rises DeprivedRiseDelay later, from a lair out of her sight and far enough.
        if (!Deprived || Deprived == First.Get() || !Deprived->IsAlive())
        {
            if (PhaseClock > EnemyTuning::DeprivedRiseDelay + 3.f) Finish(false, TEXT("no second Deprived rose"));
            return;
        }
        const float RoseAfter = Clock - DownAt;
        if (RoseAfter < EnemyTuning::DeprivedRiseDelay - .3f || RoseAfter > EnemyTuning::DeprivedRiseDelay + 1.5f)
            return Finish(false, FString::Printf(TEXT("the next rose %.1f s after the last fell"), RoseAfter));
        // Its fall counts as a kill and drops Souls (EnemyDefeated; she picks them up at once, standing right there).
        if (Kills < 1) return Finish(false, TEXT("its fall was not counted and dropped no Souls"));
        if (StepsBetween(Deprived->GetActorLocation(), Hero->GetActorLocation()) < EnemyTuning::DeprivedMinSteps || !Hidden(Hero->GetActorLocation(), Deprived->GetActorLocation()))
            return Finish(false, TEXT("the next rose in her sight or too near"));
        // Put it in front of her, to see its execute land when she does not dodge.
        const FVector Front = Hero->GetActorLocation() + Hero->GetActorForwardVector() * 260.f;
        Deprived->SetActorLocation(FVector(Front.X, Front.Y, Hero->GetActorLocation().Z), false, nullptr, ETeleportType::TeleportPhysics);
        HealthBefore = Hero->Health;
        Next(6);
        return;
    }
    if (Phase == 6)
    {
        if (Hero->Health < HealthBefore)
        {
            const float Taken = HealthBefore - Hero->Health;
            if (!FMath::IsNearlyEqual(Taken, EnemyTuning::DeprivedExecuteDamage, 1.f) || !Deprived->IsDeprivedExecuteUsed())
                return Finish(false, FString::Printf(TEXT("the execute hit for %.0f, not %.0f"), Taken, EnemyTuning::DeprivedExecuteDamage));
            if (BoostLasted < EnemyTuning::DeprivedBoostSeconds - .2f || BoostLasted > EnemyTuning::DeprivedBoostSeconds + .2f || !BoostSpeedRight
                || CooldownSeen < EnemyTuning::DeprivedBoostCooldown - .2f || CooldownSeen > EnemyTuning::DeprivedBoostCooldown + .01f)
                return Finish(false, FString::Printf(TEXT("the boost lasted %.2f s (speed doubled: %d) with a %.2f s cooldown"), BoostLasted, BoostSpeedRight, CooldownSeen));
            return Finish(true, FString::Printf(TEXT("rose out of sight %d squares away and hunted her through the maze, boosted %.1f s at double speed "
                "(cooldown %.1f s), its execute charged %.1f s and was parried by a perfect dodge, then slice and claw hurt her; "
                "the next rose %.0f s after it fell and its execute hit for %.0f"),
                StartSteps, BoostLasted, CooldownSeen, EnemyTuning::DeprivedExecuteCharge, EnemyTuning::DeprivedRiseDelay, Taken));
        }
        if (PhaseClock > 8.f) return Finish(false, TEXT("the second Deprived's execute never landed"));
    }
#endif
}
