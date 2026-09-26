#include "Levels/ArenaGameMode.h"
#include "Fighter/ArenaFighter.h"
#include "Enemies/EnemyMovesetState.h"
#include "Rules/EnemyTuning.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

// -HellgirlSwampEnemyCheck (in the swamp map, on the mud at the way in: attacks are off at camp): a rat and a frog against a Hellgirl who cannot die.
//   For 14 s she stands still: the rat must bite (a perfect dodge cannot counter it) and punch; the frog must hop,
//   and slam her from the air (hurting her), and its splash must also catch the second rat standing by her.
//   Then for 14 s she keeps swinging at the rat: it must dodge-roll away at least once.
void AArenaGameMode::RunSwampEnemyCheck(float Dt)
{
#if WITH_DEV_AUTOMATION_TESTS
    if (!FParse::Param(FCommandLine::Get(), TEXT("HellgirlSwampEnemyCheck"))) return;
    auto* Hero = Cast<AArenaFighter>(UGameplayStatics::GetPlayerPawn(this, 0));
    if (!Hero) return;
    static float Clock = 0.f;
    static TWeakObjectPtr<AArenaFighter> Rat, Frog, Bystander;
    static TSet<uint8> RatMoves, FrogMoves;
    static bool bBiteCounterable = false, bHopped = false, bSlamHurt = false, bRolled = false;
    static float LastHealth = 0.f, SwingClock = 0.f;
    Clock += Dt;
    auto Finish = [](bool Passed, const FString& Why)
    {
        if (Passed) { UE_LOG(LogTemp, Display, TEXT("SWAMP ENEMY CHECK PASSED: %s"), *Why); }
        else { UE_LOG(LogTemp, Error, TEXT("SWAMP ENEMY CHECK FAILED: %s"), *Why); }
        FPlatformMisc::RequestExitWithStatus(false, Passed ? 0 : 1);
    };
    if (Clock < 1.f) return;
    if (!Rat.IsValid() && !Frog.IsValid())
    {
        Hero->MaxHealth = Hero->Health = 100000.f;
        Hero->SetActorLocation(FVector(-5500.f, 0.f, 110.f));
        Hero->SetActorRotation(FRotator::ZeroRotator);
        auto Spawn = [&](FVector At, EHellgirlEnemyType Type) -> AArenaFighter*
        {
            auto* Enemy = GetWorld()->SpawnActor<AArenaFighter>(At, FRotator(0.f, 180.f, 0.f));
            if (!Enemy) return nullptr;
            Enemy->MakeEnemy(1, false);
            Enemy->SetEnemyType(Type);
            Enemy->HomePosition = Enemy->GetActorLocation();
            return Enemy;
        };
        Rat = Spawn(Hero->GetActorLocation() + FVector(260.f, 0.f, 0.f), EHellgirlEnemyType::Rats);
        Frog = Spawn(Hero->GetActorLocation() + FVector(700.f, 250.f, 0.f), EHellgirlEnemyType::Frogs);
        Bystander = Spawn(Hero->GetActorLocation() + FVector(0.f, -200.f, 0.f), EHellgirlEnemyType::Rats);
        if (!Rat.IsValid() || !Frog.IsValid() || !Bystander.IsValid()) { Finish(false, TEXT("spawn")); return; }
        LastHealth = Hero->Health;
        return;
    }
    // Nobody dies during the check (the frog's splash and her swings would otherwise end it early).
    for (auto* Enemy : {Rat.Get(), Frog.Get(), Bystander.Get()})
        if (Enemy && Enemy->IsAlive()) Enemy->Health = Enemy->MaxHealth;
    if (Rat.IsValid())
    {
        const EEnemyMove Move = Rat->GetEnemyMove();
        if (Move != EEnemyMove::None) RatMoves.Add(static_cast<uint8>(Move));
        if (Move == EEnemyMove::RatBite) bBiteCounterable |= Hero->CouldCounter(Rat.Get());
        bRolled |= Move == EEnemyMove::RatRoll;
    }
    if (Frog.IsValid())
    {
        const EEnemyMove Move = Frog->GetEnemyMove();
        if (Move != EEnemyMove::None) FrogMoves.Add(static_cast<uint8>(Move));
        bHopped |= Frog->GetEnemyAirTime() > .4f;
        if (Move == EEnemyMove::FrogSlam && Hero->Health < LastHealth) bSlamHurt = true;
    }
    LastHealth = Hero->Health;
    if (Clock < 15.f) return;
    if (Clock < 15.f + Dt * 1.5f)
    {
        // Phase 2: the frog and the bystander leave; she swings at the rat.
        if (Frog.IsValid()) Frog->Destroy();
        if (Bystander.IsValid()) Bystander->Destroy();
    }
    if (Clock < 29.f)
    {
        SwingClock -= Dt;
        if (Rat.IsValid() && SwingClock <= 0.f)
        {
            Hero->SetActorRotation((Rat->GetActorLocation() - Hero->GetActorLocation()).GetSafeNormal2D().Rotation());
            Hero->Attack();
            SwingClock = 1.1f;        }
        return;
    }
    const bool Bit = RatMoves.Contains(static_cast<uint8>(EEnemyMove::RatBite));
    const bool Punched = RatMoves.Contains(static_cast<uint8>(EEnemyMove::RatPunch));
    const bool Slammed = FrogMoves.Contains(static_cast<uint8>(EEnemyMove::FrogSlam));
    const int32 Splashed = Bystander.IsValid() ? Bystander->GetSplashHitsTaken() : -1;
    UE_LOG(LogTemp, Display, TEXT("Swamp enemy check: rat moves %d (bite %d punch %d second %d roll %d), frog moves %d (punch %d air %d slam %d), hopped %d, slam hurt %d"),
        RatMoves.Num(), Bit, Punched, RatMoves.Contains(static_cast<uint8>(EEnemyMove::RatPunch2)), bRolled, FrogMoves.Num(),
        FrogMoves.Contains(static_cast<uint8>(EEnemyMove::FrogPunch)), FrogMoves.Contains(static_cast<uint8>(EEnemyMove::FrogAirPunch)), Slammed, bHopped, bSlamHurt);
    if (!Bit || !Punched) { Finish(false, TEXT("the rat did not both bite and punch")); return; }
    if (bBiteCounterable) { Finish(false, TEXT("a perfect dodge could counter the rat's bite")); return; }
    if (!bHopped) { Finish(false, TEXT("the frog never hopped")); return; }
    if (!Slammed || !bSlamHurt) { Finish(false, TEXT("the frog never slammed her")); return; }
    if (!bRolled) { Finish(false, TEXT("the rat never rolled away from her swings")); return; }
    Finish(true, TEXT("the rat bit (uncounterable), punched and rolled away; the frog hopped and slammed her"));
#endif
}
