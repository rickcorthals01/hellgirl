#include "Fighter/ArenaFighter.h"
#include "Levels/ArenaGameMode.h"
#include "Progress/CoinPickup.h"
#include "Enemies/EnemySpawnPoint.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/PlatformMisc.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"

// Exercise real CharacterMovement sweeps and Chaos bodies in a running game world.
// This explicit test mode never runs during normal play and stays away from the
// player, so its test coin drops cannot change the user's saved wallet.
void AArenaFighter::RunPhysicsCheck(float Dt)
{
#if WITH_DEV_AUTOMATION_TESTS
    if (bEnemy || UGameplayStatics::GetPlayerPawn(this, 0) != this
        || !FParse::Param(FCommandLine::Get(), TEXT("HellgirlPhysicsCheck"))) return;

    struct FState
    {
        int32 Phase = 0;
        float Elapsed = 0.f, PhaseTime = 0.f;
        bool Finished = false, SawTransferredLaunch = false;
        float WallHealth = 0.f;
        int32 KillsBefore = 0, CoinsBefore = 0;
        TWeakObjectPtr<AArenaFighter> First, Second;
        TArray<TWeakObjectPtr<AArenaFighter>> Corpses;
        TArray<FVector> CorpseStarts;
        TArray<FName> CorpseBones;
    };
    static FState State;
    if (State.Finished) return;
    State.Elapsed += Dt;
    State.PhaseTime += Dt;
    auto Check = [&](bool Good, const TCHAR* Reason)
    {
        if (!Good)
        {
            State.Finished = true;
            UE_LOG(LogTemp, Error, TEXT("PHYSICS CHECK FAILED phase=%d: %s"), State.Phase, Reason);
            FPlatformMisc::RequestExitWithStatus(false, 1);
        }
        return Good;
    };
    if (!Check(State.Elapsed < 20.f, TEXT("Runtime physics regression timed out"))) return;
    auto Next = [&]() { ++State.Phase; State.PhaseTime = 0.f; };
    auto* GM = Cast<AArenaGameMode>(UGameplayStatics::GetGameMode(this));
    if (!Check(GM != nullptr && GM->MapNumber == 1, TEXT("Physics check requires StageMap=1"))) return;
    auto CountCoins = [&]()
    {
        int32 Count = 0;
        // Hearts are a separate random drop; only coins track one-per-defeat.
        for (TActorIterator<ACoinPickup> It(GetWorld()); It; ++It) if (!It->IsHeart()) ++Count;
        return Count;
    };
    auto Enemy = [&](FVector Position, EHellgirlEnemyType Type = EHellgirlEnemyType::Succubus)
    {
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* NewEnemy = GetWorld()->SpawnActor<AArenaFighter>(Position, FRotator::ZeroRotator, Params);
        if (!NewEnemy) return NewEnemy;
        NewEnemy->MakeEnemy(1, Type == EHellgirlEnemyType::FlyingImps);
        NewEnemy->SetEnemyType(Type);
        NewEnemy->Health = NewEnemy->MaxHealth = 1000.f;
        NewEnemy->HomePosition = Position;
        NewEnemy->bGuardHome = true;
        return NewEnemy;
    };
    auto DestroyLiveSubjects = [&]()
    {
        if (State.First.IsValid()) State.First->Destroy();
        if (State.Second.IsValid()) State.Second->Destroy();
        State.First.Reset(); State.Second.Reset();
    };
    auto BodyPosition = [](AArenaFighter* Fighter, FName Bone)
    {
        return Bone.IsNone() ? Fighter->GetCapsuleComponent()->GetCenterOfMass() : Fighter->GetMesh()->GetCenterOfMass(Bone);
    };

    if (State.Phase == 0)
    {
        if (State.Elapsed < 1.f) return;
        for (AEnemySpawnPoint* Site : GM->GetSpawnSites()) Site->SetActorTickEnabled(false);
        auto Cube = [&](FVector Position, FVector Scale)
        {
            auto* Actor = GetWorld()->SpawnActor<AStaticMeshActor>(Position, FRotator::ZeroRotator);
            if (!Actor) return Actor;
            Actor->SetMobility(EComponentMobility::Movable);
            Actor->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
            Actor->SetActorScale3D(Scale);
            Actor->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
            return Actor;
        };
        if (!Check(Cube(FVector(-1700,-2500,700), FVector(45,25,1)) != nullptr, TEXT("Test floor spawned"))) return;
        if (!Check(Cube(FVector(-1800,-2500,1100), FVector(.5f,4,8)) != nullptr, TEXT("Test wall spawned"))) return;
        FHitResult Wall;
        if (!Check(GetWorld()->LineTraceSingleByChannel(Wall, FVector(-2000,-2500,850), FVector(-1600,-2500,850), ECC_Visibility)
            && Wall.ImpactNormal.Equals(-FVector::ForwardVector, .01f), TEXT("Wall has expected blocking plane and normal"))) return;
        State.First = Enemy(FVector(-2000,-2500,850));
        if (!Check(State.First.IsValid(), TEXT("Wall launch subject spawned"))) return;
        State.First->HitClock = 1.f; // An immediate crash must bypass ordinary hit invulnerability.
        State.First->ApplyCombatLaunch(FVector(1800,0,200));
        Next(); return;
    }
    if (State.Phase == 1)
    {
        if (State.PhaseTime < .4f) return;
        if (!Check(State.First.IsValid() && State.First->Health < 1000.f && State.First->Health >= 965.f,
            TEXT("Swept wall collision deals one capped impact hit despite HitClock"))) return;
        if (!Check(State.First->GetActorLocation().X < -1860.f, TEXT("Fast launched capsule cannot pass through wall"))) return;
        State.WallHealth = State.First->Health;
        Next(); return;
    }
    if (State.Phase == 2)
    {
        if (State.PhaseTime < .5f) return;
        if (!Check(State.First.IsValid() && State.First->Health == State.WallHealth,
            TEXT("Resting/sliding along the same wall does not repeat damage"))) return;
        UE_LOG(LogTemp, Display, TEXT("PHYSICS CHECK wall passed: swept contact, hit immunity bypass, bounded damage, no repeated hit"));
        DestroyLiveSubjects();
        State.First = Enemy(FVector(-2200,-3100,850));
        State.Second = Enemy(FVector(-1950,-3100,850));
        if (!Check(State.First.IsValid() && State.Second.IsValid(), TEXT("Chain collision subjects spawned"))) return;
        State.First->ApplyCombatLaunch(FVector(1500,0,100));
        Next(); return;
    }
    if (State.Phase == 3)
    {
        if (!Check(State.First.IsValid() && State.Second.IsValid(), TEXT("Chain subjects remain valid"))) return;
        State.SawTransferredLaunch |= State.Second->IsCombatLaunched();
        if (State.PhaseTime < .65f) return;
        if (!Check(State.First->Health < 1000.f && State.First->Health >= 965.f
            && State.Second->Health < 1000.f && State.Second->Health >= 965.f,
            TEXT("Enemy collision damages both participants once within impact cap"))) return;
        if (!Check(State.SawTransferredLaunch && State.Second->GetActorLocation().X > -1910.f,
            TEXT("Enemy collision transfers real launched movement"))) return;
        State.Second->ResetAfterRecovery();
        if (!Check(!State.Second->IsCombatLaunched() && State.Second->GetVelocity().IsNearlyZero()
            && State.Second->GetCharacterMovement()->PendingLaunchVelocity.IsNearlyZero(), TEXT("Recovery clears launch and pending movement"))) return;
        UE_LOG(LogTemp, Display, TEXT("PHYSICS CHECK chain passed: both enemies damaged, momentum transfer, recovery reset"));
        DestroyLiveSubjects();
        State.First = Enemy(FVector(-1880,-2500,850));
        if (!Check(State.First.IsValid(), TEXT("Slow collision subject spawned"))) return;
        State.First->ApplyCombatLaunch(FVector(300,0,50));
        Next(); return;
    }
    if (State.Phase == 4)
    {
        if (State.PhaseTime < .4f) return;
        if (!Check(State.First.IsValid() && State.First->Health == 1000.f && State.First->GetActorLocation().X <= -1860.f,
            TEXT("Slow wall contact blocks movement without impact damage"))) return;
        DestroyLiveSubjects();
        State.First = Enemy(FVector(-1880,-2500,850));
        if (!Check(State.First.IsValid(), TEXT("Unlaunched collision subject spawned"))) return;
        State.First->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
        State.First->GetCharacterMovement()->Velocity = FVector(1000,0,50);
        Next(); return;
    }
    if (State.Phase == 5)
    {
        if (State.PhaseTime < .4f) return;
        if (!Check(State.First.IsValid() && State.First->Health == 1000.f && !State.First->IsCombatLaunched(),
            TEXT("Ordinary movement contact cannot cause combat collision damage"))) return;
        UE_LOG(LogTemp, Display, TEXT("PHYSICS CHECK thresholds passed: slow and unlaunched contacts are harmless"));
        DestroyLiveSubjects();
        State.KillsBefore = GM->Kills; State.CoinsBefore = CountCoins();
        const EHellgirlEnemyType Types[] = {EHellgirlEnemyType::Imps, EHellgirlEnemyType::FlyingImps, EHellgirlEnemyType::Gulps, EHellgirlEnemyType::Succubus};
        for (int32 Index = 0; Index < UE_ARRAY_COUNT(Types); ++Index)
        {
            auto* Corpse = Enemy(FVector(-3100 + Index * 600,-1650,1200), Types[Index]);
            if (!Check(Corpse != nullptr, TEXT("Death subject spawned"))) return;
            Corpse->Health = 1.f; Corpse->HitClock = 1.f;
            Corpse->ApplyPhysicsDamage(10000.f, FVector(180,0,80));
            if (!Check(!Corpse->IsAlive() && Corpse->IsDeathRagdollActive(), TEXT("Lethal impact starts ragdoll despite HitClock"))) return;
            FName SimulatedBone = NAME_None;
            if (Corpse->GetMesh()->IsVisible())
            {
                if (UPhysicsAsset* Asset = Corpse->GetMesh()->GetPhysicsAsset())
                    for (USkeletalBodySetup* Setup : Asset->SkeletalBodySetups)
                        if (Setup && Corpse->GetMesh()->IsSimulatingPhysics(Setup->BoneName))
                        { SimulatedBone = Setup->BoneName; break; }
            }
            if (!Check(!SimulatedBone.IsNone() || Corpse->GetCapsuleComponent()->IsSimulatingPhysics(), TEXT("Skeletal or placeholder corpse has a simulated body"))) return;
            State.Corpses.Add(Corpse); State.CorpseBones.Add(SimulatedBone);
            State.CorpseStarts.Add(BodyPosition(Corpse, SimulatedBone));
            Corpse->ApplyPhysicsDamage(10000.f, FVector(500,0,0));
            Corpse->ReceiveHit(10000.f, FVector::ForwardVector);
        }
        if (!Check(GM->Kills == State.KillsBefore + 4 && CountCoins() == State.CoinsBefore + 4,
            TEXT("Each lethal impact counts one defeat and one pickup, including repeated death calls"))) return;
        State.First = Enemy(FVector(-2000,-2500,850));
        if (!Check(State.First.IsValid(),TEXT("Lethal wall collision subject spawned"))) return;
        State.First->Health=1.f;
        State.First->HitClock=1.f;
        State.First->ApplyCombatLaunch(FVector(1800,0,200));
        Next(); return;
    }
    if (State.Phase == 6)
    {
        if (State.PhaseTime < .8f) return;
        for (int32 Index = 0; Index < State.Corpses.Num(); ++Index)
        {
            auto* Corpse = State.Corpses[Index].Get();
            if (!Check(Corpse != nullptr && Corpse->IsDeathRagdollActive(), TEXT("Corpse persists for visible ragdoll motion"))) return;
            const FVector Position = BodyPosition(Corpse, State.CorpseBones[Index]);
            if (!Check(!Position.ContainsNaN() && Position.Z < State.CorpseStarts[Index].Z - 20.f
                && FVector::Dist(Position, State.CorpseStarts[Index]) < 1500.f,
                TEXT("Configured and placeholder ragdolls visibly fall without unstable displacement"))) return;
        }
        if (!Check(State.First.IsValid() && !State.First->IsAlive() && State.First->IsDeathRagdollActive(),
            TEXT("Real swept wall impact can kill and start a corpse"))) return;
        if (!Check(GM->Kills == State.KillsBefore + 5 && CountCoins() == State.CoinsBefore + 5,
            TEXT("Lethal wall hit counts once and corpse simulation cannot repeat rewards"))) return;
        State.Finished = true;
        UE_LOG(LogTemp, Display, TEXT("PHYSICS CHECK PASSED: wall sweep, enemy chain, speed thresholds, reset, Imp/FlyingImp/Gulp/placeholder ragdolls, exactly-once coins and kills"));
        FPlatformMisc::RequestExitWithStatus(false, 0);
    }
#endif
}
