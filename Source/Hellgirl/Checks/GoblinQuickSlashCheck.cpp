#include "Levels/ArenaGameMode.h"
#include "Fighter/ArenaFighter.h"
#include "Rules/EnemyTuning.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

// -HellgirlQuickSlashCheck (at camp): one goblin next to a still Hellgirl. It must open with the quick slash, which
// a perfect dodge cannot counter and which deals low damage; then use its dagger slash, and quick-slash again only
// after its 6 s cooldown.
void AArenaGameMode::RunQuickSlashCheck(float Dt)
{
#if WITH_DEV_AUTOMATION_TESTS
    if (!FParse::Param(FCommandLine::Get(), TEXT("HellgirlQuickSlashCheck"))) return;
    auto* Hero = Cast<AArenaFighter>(UGameplayStatics::GetPlayerPawn(this, 0));
    if (!Hero) return;
    static TWeakObjectPtr<AArenaFighter> Goblin;
    static EEnemyMove LastMove = EEnemyMove::None;
    static TArray<TPair<EEnemyMove, float>> Moves;
    static float QuickDamage = -1.f, HealthAtQuick = 0.f;
    static bool bCounterable = false;
    auto Finish = [](bool Passed, const TCHAR* Why)
    {
        if (Passed) { UE_LOG(LogTemp, Display, TEXT("QUICK SLASH CHECK PASSED: opens with the quick slash, cannot be countered, low damage, dagger slash in between, 6 s cooldown")); }
        else { UE_LOG(LogTemp, Error, TEXT("QUICK SLASH CHECK FAILED: %s"), Why); }
        FPlatformMisc::RequestExitWithStatus(false, Passed ? 0 : 1);
    };
    const float Now = GetWorld()->GetTimeSeconds();
    if (Now < 1.f) return;
    if (!Goblin.IsValid())
    {
        Hero->MaxHealth = Hero->Health = 1000.f;
        Hero->SetActorLocation(FVector(-550, 0, 110)); Hero->SetActorRotation(FRotator::ZeroRotator);
        Goblin = GetWorld()->SpawnActor<AArenaFighter>(Hero->GetActorLocation() + FVector(260, 0, 0), FRotator(0, 180, 0));
        if (!Goblin.IsValid()) { Finish(false, TEXT("goblin spawn")); return; }
        Goblin->MakeEnemy(1, false); Goblin->SetEnemyType(EHellgirlEnemyType::Goblins);
        Goblin->HomePosition = Goblin->GetActorLocation();
        return;
    }
    Goblin->Health = Goblin->MaxHealth; // it must survive to keep attacking
    const EEnemyMove Move = Goblin->GetEnemyMove();
    if (Move != LastMove && Move != EEnemyMove::None) { Moves.Add({Move, Now}); if (Move == EEnemyMove::GoblinQuickSlash && QuickDamage < 0.f) HealthAtQuick = Hero->Health; }
    if (LastMove == EEnemyMove::GoblinQuickSlash && Move != EEnemyMove::GoblinQuickSlash && QuickDamage < 0.f) QuickDamage = HealthAtQuick - Hero->Health;
    if (Move == EEnemyMove::GoblinQuickSlash) bCounterable |= Hero->CouldCounter(Goblin.Get());
    LastMove = Move;
    if (Now < 14.f) return;
    int32 Quick = 0, Slash = 0;
    float FirstQuick = -1.f, SecondQuick = -1.f;
    for (const auto& M : Moves)
    {
        if (M.Key == EEnemyMove::GoblinQuickSlash) { ++Quick; if (FirstQuick < 0.f) FirstQuick = M.Value; else if (SecondQuick < 0.f) SecondQuick = M.Value; }
        if (M.Key == EEnemyMove::GoblinSlash) ++Slash;
    }
    UE_LOG(LogTemp, Display, TEXT("Quick slash check: %d moves, %d quick, %d slash, quick damage %.1f, second quick after %.2f s"), Moves.Num(), Quick, Slash, QuickDamage, SecondQuick - FirstQuick);
    if (Moves.IsEmpty() || Moves[0].Key != EEnemyMove::GoblinQuickSlash) { Finish(false, TEXT("the goblin did not open with the quick slash")); return; }
    if (bCounterable) { Finish(false, TEXT("a perfect dodge could counter the quick slash")); return; }
    if (QuickDamage <= 0.f || QuickDamage > 8.f) { Finish(false, TEXT("quick slash damage should be low (about 5)")); return; }
    if (Slash == 0) { Finish(false, TEXT("no dagger slash between quick slashes")); return; }
    if (Quick < 2 || SecondQuick - FirstQuick < EnemyTuning::GoblinQuickSlashCooldown - .05f) { Finish(false, TEXT("quick slash cooldown")); return; }
    Finish(true, TEXT(""));
#endif
}
