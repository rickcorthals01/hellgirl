// World II's stages in the swamp (Developer idea folder, "Dialog and Story/05, 06 and 07 Swamps Stage ..."):
//   Stage I: a six-second camera overview of the swamp, the Goblin Queen's intro, ten waves of rats and frogs with her
//     lines after waves 2, 6 and 9, then the purple portal.
//   Stage II: a run of ten swamp rooms, one wave in each; her lines at rooms 1, 5, 7 and 10. Room 10 is the Frog
//     King's (the mini-boss, on his giant lily pad, with a guard of rats and frogs); beating it brings her last line
//     and the purple portal.
//   Stage III: two waves, then the doorway conversation; the Rat Queen's fight comes later (her own boss area), so
//     for now the purple portal leads back to camp.
// URL options: Swamp=1?Stage=S?Seed=N?Room=R (Stage 0 is the map preview). Rooms and scenery: Levels/Swamp.cpp.
#include "Levels/ArenaGameMode.h"
#include "Levels/WavePortal.h"
#include "Rules/SwampRules.h"
#include "Rules/EnemyTuning.h"
#include "Fighter/ArenaFighter.h"
#include "Enemies/EnemySpawnPoint.h"
#include "UI/HellgirlPlayerController.h"
#include "Progress/CampaignProgress.h"
#include "Camera/CameraActor.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/ConfigCacheIni.h"

namespace SwampStages
{
// A wave: how many rats and frogs (before the global wave scaling), and the conversation once it is beaten.
struct FWave { int32 Rats; int32 Frogs; const TCHAR* After; };
const FWave StageOne[] = {
    {3, 0, nullptr}, {4, 0, TEXT("S1_AfterWave2")}, {3, 1, nullptr}, {3, 2, nullptr}, {0, 3, nullptr},
    {4, 2, TEXT("S1_AfterWave6")}, {5, 2, nullptr}, {3, 4, nullptr}, {5, 4, TEXT("S1_AfterWave9")}, {6, 5, nullptr}};
const FWave StageThree[] = {{5, 2, nullptr}, {4, 4, TEXT("S3_Doorway")}};
constexpr int32 RunRooms = 10;
constexpr float OverviewSeconds = 6.f;
// Stage II, room R (1-9): more of both as the run goes on.
FWave RoomWave(int32 Room) { return {2 + Room / 3, 1 + Room / 3, nullptr}; }
const TCHAR* RoomLine(int32 Room)
{
    return Room == 1 ? TEXT("S2_Room1") : Room == 5 ? TEXT("S2_Room5") : Room == 7 ? TEXT("S2_Room7") : Room == 10 ? TEXT("S2_Room10") : nullptr;
}
}

void AArenaGameMode::StartSwampStage(int32 Stage)
{
    SwampStage = Stage;
    // Stages I and III always use the same stretch of swamp; each Stage II run rolls its own ten rooms.
    TravelToSwampRoom(Stage == 1 ? 1101 : Stage == 3 ? 3303 : FMath::RandRange(1, 999999), 1);
}

bool AArenaGameMode::SwampSaid(FName Conversation)
{
    // Without the story (automated checks) every conversation counts as said.
    if (!bStoryEnabled) return true;
    QueueStory(Conversation);
    return HasPlayedStory(Conversation);
}

void AArenaGameMode::BuildSwampWaves()
{
    using namespace SwampStages;
    TArray<FWave> Waves;
    if (SwampStage == 1) Waves.Append(StageOne, UE_ARRAY_COUNT(StageOne));
    else if (SwampStage == 3) Waves.Append(StageThree, UE_ARRAY_COUNT(StageThree));
    else if (SwampStage == 2) Waves.Add(SwampRoomNumber < RunRooms ? RoomWave(SwampRoomNumber) : FWave{3, 3, nullptr});
    for (int32 I = 0; I < Waves.Num(); ++I)
    {
        const FWave& W = Waves[I];
        auto* S = Site(FVector(0.f, 0.f, 10.f), FString::Printf(TEXT("WAVE %d"), I + 1), W.Rats + W.Frogs, 0, false);
        if (!S) continue;
        S->GroundType = W.Rats > 0 ? EHellgirlEnemyType::Rats : EHellgirlEnemyType::Frogs;
        S->MixType = EHellgirlEnemyType::Frogs;
        S->MixCount = W.Rats > 0 ? W.Frogs : 0;
        S->FlyingCount = 0;
        S->Difficulty = SwampStage == 3 ? 2 : 1;
        S->bInstantGroup = false;
        S->bGroupGuardsHome = false;
        SwampWaveOfSite.Add(I);
        SwampWaveAfter.Add(W.After ? FName(W.After) : NAME_None);
    }
    // The Frog King waits on his lily pad in the run's last room, part of its wave.
    if (SwampStage == 2 && SwampRoomNumber >= RunRooms)
        if (auto* King = Site(FVector(SwampRoom::FrogSpot.X, SwampRoom::FrogSpot.Y, 60.f), TEXT("THE FROG KING"), 1, 0, false, true))
        {
            King->GroundType = EHellgirlEnemyType::FrogKing;
            King->FlyingCount = 0;
            King->Difficulty = 2;
            SwampWaveOfSite.Add(0);
        }
    SwampWaveCount = Waves.Num();
    TotalSites = SpawnSites.Num();
    ExitGate = GetWorld()->SpawnActor<AWavePortal>();
}

bool AArenaGameMode::TickSwampStage(float Dt, AArenaFighter* Hero)
{
    using namespace SwampStages;
    auto* PC = Cast<AHellgirlPlayerController>(Hero->GetController());
    ActivatedSites = ClearedSites = EnemiesRemaining = 0;
    for (auto S : SpawnSites) { ActivatedSites += S->bActivated; ClearedSites += S->bCleared; EnemiesRemaining += S->LivingEnemies(); }

    // Stage I opens on a six-second flight over the swamp, from the far end back to Hellgirl.
    if (SwampStage == 1 && SwampIntroClock < OverviewSeconds)
    {
        if (!bStoryEnabled) { SwampIntroClock = OverviewSeconds; return true; }
        if (!SwampCamera)
        {
            SwampCamera = GetWorld()->SpawnActor<ACameraActor>();
            if (PC) { PC->SetViewTarget(SwampCamera); PC->SetIgnoreMoveInput(true); }
        }
        SwampIntroClock += Dt;
        const float T = FMath::SmoothStep(0.f, 1.f, SwampIntroClock / OverviewSeconds);
        const FVector From(SwampRoom::Half - 400.f, -900.f, 1500.f), To(Hero->GetActorLocation() + FVector(-500.f, 0.f, 330.f));
        const FVector Eye = FMath::Lerp(From, To, T);
        const FVector Look = FMath::Lerp(FVector(0.f, 0.f, 0.f), Hero->GetActorLocation() + FVector(1200.f, 0.f, 0.f), T);
        SwampCamera->SetActorLocationAndRotation(Eye, (Look - Eye).Rotation());
        Objective = TEXT("THE SWAMP OF SOULS");
        if (SwampIntroClock >= OverviewSeconds && PC) { PC->SetViewTargetWithBlend(Hero, .6f); PC->SetIgnoreMoveInput(false); }
        return true;
    }
    if (SwampStage == 1 && !SwampSaid(TEXT("S1_Intro"))) return true;
    if (SwampStage == 2)
        if (const TCHAR* Line = RoomLine(SwampRoomNumber); Line && !SwampSaid(Line)) return true;

    // The waves, one after another, each coming at her from further along the swamp.
    if (SwampWave < SwampWaveCount)
    {
        TArray<AEnemySpawnPoint*> Current;
        for (int32 I = 0; I < SpawnSites.Num(); ++I)
            if (SwampWaveOfSite.IsValidIndex(I) && SwampWaveOfSite[I] == SwampWave) Current.Add(SpawnSites[I]);
        bool Started = false, Beaten = true;
        for (auto* S : Current) { Started |= S->bActivated; Beaten &= S->bCleared; }
        const int32 Shown = SwampStage == 2 ? SwampRoomNumber : SwampWave + 1;
        const int32 Of = SwampStage == 2 ? RunRooms : SwampWaveCount;
        if (!Started)
        {
            SwampWaveClock -= Dt;
            Objective = FString::Printf(TEXT("%s %d / %d — Incoming"), SwampStage == 2 ? TEXT("ROOM") : TEXT("WAVE"), Shown, Of);
            if (SwampWaveClock > 0.f) return true;
            FRandomStream Spot(SwampSeed * 31 + SwampRoomNumber * 7 + SwampWave);
            for (auto* S : Current)
            {
                if (!S->bBoss)
                {
                    // Stages I and III: ahead of her along the corridor. Stage II: somewhere in the room's water.
                    const float X = SwampStage == 2 ? Spot.FRandRange(SwampWaterStart + 800.f, SwampWaterEnd - 800.f)
                        : FMath::Clamp(static_cast<float>(Hero->GetActorLocation().X) + 1100.f, -SwampRoom::Half + 900.f, SwampRoom::Half - 1000.f);
                    S->SetActorLocation(FVector(X, Spot.FRandRange(-650.f, 650.f), 10.f));
                }
                S->bEnabled = S->bActivated = true;
            }
            return true;
        }
        if (!Beaten)
        {
            Objective = FString::Printf(TEXT("%s %d / %d — %s"), SwampStage == 2 ? TEXT("ROOM") : TEXT("WAVE"), Shown, Of,
                SwampStage == 2 && SwampRoomNumber >= RunRooms ? TEXT("Defeat the Frog King") : TEXT("Rats and frogs"));
            return true;
        }
        if (SwampWaveAfter.IsValidIndex(SwampWave) && !SwampWaveAfter[SwampWave].IsNone() && !SwampSaid(SwampWaveAfter[SwampWave])) return true;
        ++SwampWave;
        SwampWaveClock = EnemyTuning::WaveBreak(3.f);
        return true;
    }

    // Every wave beaten. Mid-run rooms open the light at the end onto the next room.
    if (SwampStage == 2 && SwampRoomNumber < SwampRooms)
    {
        Objective = FString::Printf(TEXT("ROOM %d / %d CLEAR — Follow the light"), SwampRoomNumber, RunRooms);
        return false;
    }
    if (SwampStage == 2 && !SwampSaid(TEXT("S2_Won"))) return true;
    // The purple portal, by the light at the end.
    if (ExitGate && !ExitGate->IsOpen())
        ExitGate->Open(FVector(SwampRoom::Exit.X - 250.f, 0.f, 0.f), FVector(SwampRoom::Exit.X - 900.f, 0.f, 0.f), true);
    Objective = SwampStage == 3 ? TEXT("THE RAT QUEEN / her fight comes later · the portal leads back to camp")
        : TEXT("LEVEL COMPLETE / Enter the purple portal");
    Prompt.Empty();
    PromptAction = 0;
    TickPortalMenus(Hero);
    return true;
}

// -HellgirlSwampStageCheck (with Swamp=1?Stage=S): walks the stage's script by beating each wave as it starts. Stage I
// must run ten waves, Stage III two, a Stage II room one (room 10 with the Frog King in it); every wave only rats and
// frogs; then the purple portal must open (a mid-run Stage II room opens the light instead). Every conversation the
// stages use must be in Content/Dialogue/LevelTwo.ini.
void AArenaGameMode::RunSwampStageCheck(float Dt)
{
#if WITH_DEV_AUTOMATION_TESTS
    using namespace SwampStages;
    if (!FParse::Param(FCommandLine::Get(), TEXT("HellgirlSwampStageCheck"))) return;
    static float Clock = 0.f;
    static bool Done = false;
    Clock += Dt;
    auto* Hero = Cast<AArenaFighter>(UGameplayStatics::GetPlayerPawn(this, 0));
    if (Done || Clock < 1.f || !Hero) return;
    Done = true;
    auto Finish = [](bool Passed, const FString& Why)
    {
        if (Passed) { UE_LOG(LogTemp, Display, TEXT("SWAMP STAGE CHECK PASSED: %s"), *Why); }
        else { UE_LOG(LogTemp, Error, TEXT("SWAMP STAGE CHECK FAILED: %s"), *Why); }
        FPlatformMisc::RequestExitWithStatus(false, Passed ? 0 : 1);
    };
    if (SwampStage < 1) { Finish(false, TEXT("not a swamp stage")); return; }
    // The conversations.
    FConfigFile Script;
    Script.Read(FPaths::ProjectContentDir() / TEXT("Dialogue/LevelTwo.ini"));
    for (const TCHAR* Id : {TEXT("S1_Intro"), TEXT("S1_AfterWave2"), TEXT("S1_AfterWave6"), TEXT("S1_AfterWave9"), TEXT("S2_Room1"), TEXT("S2_Room5"),
                            TEXT("S2_Room7"), TEXT("S2_Room10"), TEXT("S2_Won"), TEXT("S3_Doorway")})
    {
        FString Line;
        if (!Script.GetString(Id, TEXT("Line0"), Line)) { Finish(false, FString::Printf(TEXT("conversation %s missing"), Id)); return; }
    }
    Hero->MaxHealth = Hero->Health = 1000000.f;
    for (auto S : SpawnSites)
        if (S->GroundType != EHellgirlEnemyType::Rats && S->GroundType != EHellgirlEnemyType::Frogs && S->GroundType != EHellgirlEnemyType::FrogKing)
        { Finish(false, TEXT("a wave with enemies other than rats and frogs")); return; }
    const bool King = SwampStage == 2 && SwampRoomNumber >= RunRooms;
    bool HasKing = false;
    for (auto S : SpawnSites) HasKing |= S->GroundType == EHellgirlEnemyType::FrogKing && S->bBoss;
    if (HasKing != King) { Finish(false, TEXT("the Frog King is in the wrong room")); return; }
    int32 Waves = 0;
    bool LightOpen = false;
    for (int32 Guard = 0; Guard < 100; ++Guard)
    {
        const bool Shut = TickSwampStage(10.f, Hero);
        if (!Shut) { LightOpen = true; break; }
        if (ExitGate && ExitGate->IsOpen()) break;
        int32 Started = 0;
        for (auto S : SpawnSites) if (S->bActivated && !S->bCleared) { S->bCleared = true; ++Started; }
        Waves += Started > 0 && (Started == 1 || King);
    }
    const int32 Expected = SwampStage == 1 ? UE_ARRAY_COUNT(StageOne) : SwampStage == 3 ? UE_ARRAY_COUNT(StageThree) : 1;
    const bool WantLight = SwampStage == 2 && SwampRoomNumber < RunRooms;
    UE_LOG(LogTemp, Display, TEXT("Swamp stage check: stage %d room %d: %d waves, light %d, portal %d"), SwampStage, SwampRoomNumber, Waves, LightOpen, ExitGate && ExitGate->IsOpen());
    if (Waves != Expected) { Finish(false, FString::Printf(TEXT("%d waves, expected %d"), Waves, Expected)); return; }
    if (WantLight != LightOpen || (!WantLight && !(ExitGate && ExitGate->IsOpen()))) { Finish(false, TEXT("the way out did not open")); return; }
    Finish(true, FString::Printf(TEXT("stage %d room %d: %d waves of rats and frogs%s, then %s"), SwampStage, SwampRoomNumber, Waves,
        King ? TEXT(" with the Frog King") : TEXT(""), WantLight ? TEXT("the light to the next room") : TEXT("the purple portal")));
#endif
}
