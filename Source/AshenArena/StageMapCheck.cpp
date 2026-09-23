#include "ArenaGameMode.h"
#include "ArenaFighter.h"
#include "EnemySpawnPoint.h"
#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/PlatformMisc.h"

// Explicit unattended test mode only; never runs in normal editor/game sessions.
void AArenaGameMode::RunMapCheck(float Dt)
{
#if WITH_DEV_AUTOMATION_TESTS
    if (!FParse::Param(FCommandLine::Get(), TEXT("HellgirlMapCheck"))) return;
    static float Elapsed = 0.f;
    static int32 Index = 0;
    static bool CheckedLayout = false;
    static bool WaitingForClear = false;
    static int32 SeenMap = 0, ExitStep = 0;
    static bool EnergyRestartRequested = false;
    if (SeenMap != MapNumber)
    {
        SeenMap = MapNumber; Elapsed = 0; Index = 0; ExitStep = 0;
        CheckedLayout = WaitingForClear = false;
    }
    Elapsed += Dt;
    auto Fail = [&](const TCHAR* Reason) {
        UE_LOG(LogTemp, Error, TEXT("MAP CHECK FAILED map=%d: %s"), MapNumber, Reason);
        FPlatformMisc::RequestExitWithStatus(false, 1);
    };
    if (Elapsed > 50.f) { Fail(TEXT("Encounter sequence timed out")); return; }
    auto* Player = Cast<AArenaFighter>(UGameplayStatics::GetPlayerPawn(this, 0));
    Player->Health = Player->MaxHealth = 100000.f;
    if (Elapsed < 1.f) return;
    if (EnergyRestartRequested)
    {
        if (MapNumber != 3 || Player->Energy != 0.f) { Fail(TEXT("Restart did not reset energy")); return; }
        UE_LOG(LogTemp, Display, TEXT("MAP ENERGY CHECK PASSED: fresh start empty, portal carryover, restart empty"));
        FPlatformMisc::RequestExitWithStatus(false, 0);
        return;
    }
    if (!CheckedLayout)
    {
        const float ExpectedEnergy = MapNumber == 1 ? 0.f : MapNumber == 2 ? 57.f : 23.f;
        if (!FMath::IsNearlyEqual(Player->Energy, ExpectedEnergy)) { Fail(TEXT("Portal did not preserve energy")); return; }
        if (SpawnSites.Num() != (MapNumber == 1 ? 6 : MapNumber == 3 ? 3 : 5)) { Fail(TEXT("Wrong encounter count")); return; }
        if (Player->GetActorLocation().Z < 70.f) { Fail(TEXT("Start has no floor")); return; }
        if (SpawnSites.Last()->bEnabled) { Fail(TEXT("Boss unlocked early")); return; }
        for (auto S : SpawnSites)
        {
            FHitResult Hit;
            if (!GetWorld()->LineTraceSingleByChannel(Hit, S->GetActorLocation()+FVector(0,0,60), S->GetActorLocation()-FVector(0,0,80), ECC_Visibility))
            { Fail(TEXT("Encounter has no floor")); return; }
        }
        CheckedLayout = true;
        Player->SetActorLocation(SpawnSites[0]->GetActorLocation()+FVector(-650,0,115));
        UE_LOG(LogTemp, Display, TEXT("MAP CHECK layout passed map=%d platforms=%d"), MapNumber, MapPlatforms.Num());
    }
    if (Index >= SpawnSites.Num())
    {
        if (ExitPortal->IsHidden() || bWon) { Fail(TEXT("Exit gating incorrect")); return; }
        if (ExitStep == 0) { Player->SetActorLocation(ExitPosition+FVector(0,0,115)); ++ExitStep; return; }
        if (ExitStep == 1)
        {
            if (Prompt.IsEmpty()) return;
            AnswerPrompt(false);
            if (!bDeclined || !Prompt.IsEmpty()) { Fail(TEXT("Decline did not close portal prompt")); return; }
            Player->SetActorLocation(ExitPosition+FVector(-600,0,115));
            ++ExitStep; return;
        }
        if (ExitStep == 2) { Player->SetActorLocation(ExitPosition+FVector(0,0,115)); ++ExitStep; return; }
        if (Prompt.IsEmpty()) return;
        Player->Energy = MapNumber == 1 ? 57.f : 23.f;
        AnswerPrompt(true);
        UE_LOG(LogTemp, Display, TEXT("MAP CHECK PASSED map=%d: floor, encounters, boss gate, portal accept/decline"), MapNumber);
        if (MapNumber == 3)
        {
            if (!bWon) { Fail(TEXT("Final portal did not complete stage")); return; }
            Player->Energy = 80.f;
            EnergyRestartRequested = true;
            SeenMap = 0;
            RestartMap();
        }
        return;
    }
    auto S = SpawnSites[Index];
    if (WaitingForClear)
    {
        if (S->bCleared) { ++Index; WaitingForClear = false; }
        return;
    }
    if (MapNumber == 1)
    {
        if (SectionBarriers[0]->GetActorEnableCollision() != (Index < 2)
            || SectionBarriers[1]->GetActorEnableCollision() != (Index < 5))
        { Fail(TEXT("Section gate unlocked before waves cleared or failed to open")); return; }
        for (int32 Later = Index + 1; Later < SpawnSites.Num(); ++Later)
            if (SpawnSites[Later]->bActivated) { Fail(TEXT("Later wave activated early")); return; }
        Player->SetActorLocation(S->GetActorLocation()+FVector(0,0,115));
    }
    if (S->LivingEnemies() != S->EnemyCount) return;
    if (Index < SpawnSites.Num()-1 && !SpawnSites.Last()->bActivated && !ExitPortal->IsHidden())
    { Fail(TEXT("Portal visible before boss")); return; }
    int32 Flyers = 0;
    for (TActorIterator<AArenaFighter> It(GetWorld()); It; ++It)
        if (It->bEnemy && It->IsAlive() && It->HomePosition.Equals(S->GetActorLocation(), 1.f))
        {
            Flyers += It->bFlyingEnemy ? 1 : 0;
            const bool Boss = Index == SpawnSites.Num()-1;
            const EHellgirlEnemyType ExpectedType = MapNumber == 1
                ? (Boss ? EHellgirlEnemyType::ImpCommander : It->bFlyingEnemy ? EHellgirlEnemyType::FlyingImps : EHellgirlEnemyType::Imps)
                : MapNumber == 2 ? (Boss ? EHellgirlEnemyType::GulpBoss : EHellgirlEnemyType::Gulps)
                : (Boss ? EHellgirlEnemyType::SuccubusBoss : EHellgirlEnemyType::Succubus);
            if (It->EnemyType != ExpectedType) { Fail(TEXT("Wrong enemy identity for encounter")); return; }
            It->Health = 0.f; // Test-only defeat without modifying the user's wallet.
        }
    if (Flyers != S->FlyingCount) { Fail(TEXT("Wrong flying enemy count")); return; }
    UE_LOG(LogTemp, Display, TEXT("MAP CHECK encounter %d passed count=%d flyers=%d"), Index+1, S->EnemyCount, Flyers);
    WaitingForClear = true;
#endif
}
