#include "Levels/ArenaGameMode.h"
#include "Fighter/ArenaFighter.h"
#include "Enemies/EnemySpawnPoint.h"
#include "Enemies/EnemyMovesetState.h"
#include "Rules/EnemyTuning.h"
#include "Rules/SwampArenaRules.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

// -HellgirlRatQueenCheck (Swamp=1?Stage=3, the boss arena): Stage III jumps to the Rat Queen's wave, against a
// Hellgirl who cannot die.
//   0-12 s: Hellgirl stays close and keeps swinging. The Queen must slash right, then left, then heavy-slash, and
//   dodge-roll twice in a row.
//   12-24 s: Hellgirl keeps stepping 8 m away. The Queen must close in with a jump slam or the dashing heavy slash.
//   Then her health drops below half: she must strike her summon pose and call two waves of rats into her wave.
//   Her hits must have hurt Hellgirl.
void AArenaGameMode::RunRatQueenCheck(float Dt)
{
#if WITH_DEV_AUTOMATION_TESTS
    if (!FParse::Param(FCommandLine::Get(), TEXT("HellgirlRatQueenCheck"))) return;
    auto* Hero = Cast<AArenaFighter>(UGameplayStatics::GetPlayerPawn(this, 0));
    if (!Hero) return;
    static float Clock = 0.f, SwingClock = 0.f, StepClock = 0.f, LastRoll = -10.f, HealthBefore = 0.f, LastMoveClock = 0.f;
    static bool bStarted = false, bDoubleRoll = false, bCalled = false;
    static int32 Rolls = 0;
    static EEnemyMove Last = EEnemyMove::None;
    static TArray<EEnemyMove> Seen;
    Clock += Dt;
    auto Finish = [](bool Passed, const FString& Why)
    {
        if (Passed) { UE_LOG(LogTemp, Display, TEXT("RAT QUEEN CHECK PASSED: %s"), *Why); }
        else { UE_LOG(LogTemp, Error, TEXT("RAT QUEEN CHECK FAILED: %s"), *Why); }
        FPlatformMisc::RequestExitWithStatus(false, Passed ? 0 : 1);
    };
    if (Clock < 1.f) return;
    if (!bSwampArena) { Finish(false, TEXT("not the boss arena")); return; }
    if (!bStarted)
    {
        bStarted = true;
        // Straight to her wave: the waves before are done.
        for (int32 I = 0; I < SpawnSites.Num(); ++I)
            if (!SpawnSites[I]->bBoss) { SpawnSites[I]->bEnabled = false; SpawnSites[I]->bCleared = true; }
        SwampWave = SwampWaveCount - 1;
        SwampWaveClock = 0.f;
        Hero->MaxHealth = Hero->Health = 100000.f;
        HealthBefore = Hero->Health;
        const FVector2D Spot = SwampArena::WavePoint(3) - SwampArena::CryptFacing() * -350.f;
        Hero->SetActorLocation(FVector(Spot, 115.f));
        return;
    }
    AArenaFighter* Queen = nullptr;
    for (TActorIterator<AArenaFighter> It(GetWorld()); It; ++It)
        if (It->bEnemy && It->EnemyType == EHellgirlEnemyType::RatQueen && It->IsAlive()) Queen = *It;
    if (!Queen) { if (Clock > 6.f) Finish(false, TEXT("the Rat Queen never came")); return; }
    Queen->Health = FMath::Max(Queen->Health, Clock < 24.f ? Queen->MaxHealth * .9f : 1.f); // she must last the test
    const EEnemyMove Move = Queen->GetEnemyMove();
    if (Move != Last && Move != EEnemyMove::None)
    {
        Seen.Add(Move);
        if (Move == EEnemyMove::RatRoll)
        {
            if (Clock - LastRoll < 1.2f) bDoubleRoll = true;
            LastRoll = Clock; ++Rolls;
        }
    }
    // The second roll can start the same frame the first ends: the roll's clock starting over counts too.
    if (Move == EEnemyMove::RatRoll && Last == EEnemyMove::RatRoll && Queen->GetAttackClock() > LastMoveClock + .1f)
    {
        Seen.Add(Move);
        if (Clock - LastRoll < 1.2f) bDoubleRoll = true;
        LastRoll = Clock; ++Rolls;
    }
    Last = Move;
    LastMoveClock = Queen->GetAttackClock();
    const FVector ToQueen = Queen->GetActorLocation() - Hero->GetActorLocation();
    if (Clock < 12.f)
    {
        // Close in and keep swinging at her.
        Hero->SetActorRotation(ToQueen.GetSafeNormal2D().Rotation());
        if (ToQueen.Size2D() > 220.f) Hero->AddMovementInput(ToQueen.GetSafeNormal2D(), 1.f);
        if ((SwingClock -= Dt) <= 0.f && ToQueen.Size2D() < 400.f) { Hero->Attack(); SwingClock = .6f; }
    }
    else if (Clock < 24.f)
    {
        // Step well away from her now and then.
        if ((StepClock -= Dt) <= 0.f)
        {
            FVector Away = -ToQueen.GetSafeNormal2D() * 800.f;
            FVector2D To = FVector2D(Queen->GetActorLocation() + FVector(Away.X, Away.Y, 0.f));
            To = FVector2D(FMath::Clamp(To.X, -SwampArena::Half + 400.f, SwampArena::Half - 400.f), FMath::Clamp(To.Y, -SwampArena::Half + 400.f, SwampArena::Half - 400.f));
            Hero->SetActorLocation(FVector(To, 115.f), false, nullptr, ETeleportType::TeleportPhysics);
            StepClock = 3.f;
        }
    }
    else if (!bCalled)
    {
        // Below half: she calls her rats.
        Queen->Health = Queen->MaxHealth * .45f;
        bCalled = true;
    }
    if (Clock < 26.f) return;
    int32 Rats = 0;
    for (int32 I = 0; I < SpawnSites.Num(); ++I)
        if (SpawnSites[I]->SiteName == TEXT("THE QUEEN'S RATS") && SwampWaveOfSite.IsValidIndex(I) && SwampWaveOfSite[I] == SwampWave && SpawnSites[I]->bActivated) ++Rats;
    auto Has = [&](EEnemyMove M) { return Seen.Contains(M); };
    const int32 SlashAt = Seen.IndexOfByKey(EEnemyMove::RatQueenSlash);
    const bool String = SlashAt != INDEX_NONE && Seen.IsValidIndex(SlashAt + 2) && Seen[SlashAt + 1] == EEnemyMove::RatQueenSlash2 && Seen[SlashAt + 2] == EEnemyMove::RatQueenHeavy;
    // The first full string may be cut short by a roll; any right-left-heavy run counts.
    bool AnyString = String;
    for (int32 I = 0; I + 2 < Seen.Num(); ++I)
        AnyString |= Seen[I] == EEnemyMove::RatQueenSlash && Seen[I + 1] == EEnemyMove::RatQueenSlash2 && Seen[I + 2] == EEnemyMove::RatQueenHeavy;
    FString Moves;
    for (const EEnemyMove M : Seen) Moves += FString::Printf(TEXT("%d "), static_cast<int32>(M));
    UE_LOG(LogTemp, Display, TEXT("Rat Queen check: moves %s; %d rolls, double %d; rat waves %d; Hellgirl lost %.0f"), *Moves, Rolls, bDoubleRoll, Rats, HealthBefore - Hero->Health);
    if (!AnyString) { Finish(false, TEXT("no right slash, left slash, heavy slash string")); return; }
    if (!bDoubleRoll) { Finish(false, TEXT("no double dodge roll")); return; }
    if (!Has(EEnemyMove::RatQueenJumpSlam) && Seen.FilterByPredicate([](EEnemyMove M) { return M == EEnemyMove::RatQueenHeavy; }).Num() < 2)
    { Finish(false, TEXT("she never closed in with a jump slam or a heavy slash")); return; }
    if (!Has(EEnemyMove::RatQueenSummon) || Rats != 2) { Finish(false, FString::Printf(TEXT("no summon at half health (%d rat waves)"), Rats)); return; }
    if (Hero->Health >= HealthBefore) { Finish(false, TEXT("her hits never hurt Hellgirl")); return; }
    Finish(true, FString::Printf(TEXT("slash string, %d rolls with a double, %s, two rat waves at half health"), Rolls,
        Has(EEnemyMove::RatQueenJumpSlam) ? TEXT("jump slam") : TEXT("dashing heavy slashes")));
#endif
}
