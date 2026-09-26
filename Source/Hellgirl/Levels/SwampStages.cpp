// World II's stages in the swamp (Developer idea folder, "Dialog and Story/05, 06 and 07 Swamps Stage ..."):
//   Stage I: a six-second camera overview of the swamp, the Goblin Queen's intro, ten waves of rats and frogs. Blue
//     portals open after waves 2, 6 and 9; each plays her conversation when Hellgirl steps in, then its menu
//     (continue, stock Souls, upgrades). After wave 10, the purple portal.
//   Stage II: a run of ten swamp rooms; her lines at rooms 1, 5, 7 and 10. Room 10 is the Frog King's (the mini-boss,
//     on his giant lily pad, with a guard of rats and frogs); beating it brings her last line and the purple portal.
//   Stage III: two waves, then the doorway conversation; the Rat Queen's fight comes later (her own boss area), so
//     for now the purple portal leads back to camp.
// Every wave comes from a fixed point in one of the room's five areas, west to east, once Hellgirl gets near it:
//   0 the first mud, 1 the start of the water, 2 its middle, 3 where it nears the mud again, 4 the last mud.
// Stage I sets which enemies come from each area (rats first, then mostly rats, frogs, mostly frogs, both); Stage II
// rolls random rats and frogs at those points.
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
// A wave: its area and side of the corridor (0 north, 1 south), how many rats and frogs (before the global wave
// scaling), the conversation once it is beaten, and whether a blue portal opens first (the conversation then plays
// when she steps into it).
struct FWave { int32 Area; int32 Side; int32 Rats; int32 Frogs; const TCHAR* After; bool bPortal; };
const FWave StageOne[] = {
    {0, 0, 3, 0, nullptr, false}, {0, 1, 4, 0, TEXT("S1_Portal1"), true},   // the first mud: rats
    {1, 0, 3, 1, nullptr, false}, {1, 1, 4, 2, nullptr, false},             // the start of the water: mostly rats
    {2, 0, 0, 3, nullptr, false}, {2, 1, 0, 4, TEXT("S1_Portal2"), true},   // the middle of the water: frogs
    {3, 0, 2, 4, nullptr, false}, {3, 1, 2, 5, nullptr, false},             // nearing the mud: mostly frogs
    {4, 0, 4, 4, TEXT("S1_Portal3"), true}, {4, 1, 6, 5, nullptr, false}};  // the last mud: rats and frogs
const FWave StageThree[] = {{1, 0, 5, 2, nullptr, false}, {3, 1, 4, 4, TEXT("S3_Doorway"), false}};
constexpr int32 RunRooms = 10;
constexpr int32 Areas = 5;
constexpr float OverviewSeconds = 6.f;
// A wave starts once Hellgirl is this close to its point along the corridor (or past it).
constexpr float NearX = 1300.f;
const TCHAR* RoomLine(int32 Room)
{
    return Room == 1 ? TEXT("S2_Room1") : Room == 5 ? TEXT("S2_Room5") : Room == 7 ? TEXT("S2_Room7") : Room == 10 ? TEXT("S2_Room10") : nullptr;
}
// Stage II, room R: groups at three to five of the areas (all five in the Frog King's room), each one or two
// enemies (more often two later in the run), rats and frogs at random.
TArray<FWave> RoomWaves(int32 Seed, int32 Room)
{
    FRandomStream Dice(Seed * 131 + Room * 17);
    TArray<int32> Picked = {0, 1, 2, 3, 4};
    const int32 Groups = Room >= RunRooms ? Areas : 3 + (Room >= 4) + (Room >= 7);
    while (Picked.Num() > Groups) Picked.RemoveAt(Dice.RandRange(0, Picked.Num() - 1));
    TArray<FWave> Waves;
    for (const int32 Area : Picked)
    {
        const int32 Size = Room >= RunRooms ? 2 : 1 + (Dice.FRand() < Room / 10.f);
        const int32 Frogs = Dice.RandRange(0, Size);
        Waves.Add({Area, Dice.RandRange(0, 1), Size - Frogs, Frogs, nullptr, false});
    }
    return Waves;
}
}

FVector AArenaGameMode::SwampAreaPoint(int32 Area, int32 Side) const
{
    const float Water = SwampWaterEnd - SwampWaterStart;
    const float X = Area == 0 ? FMath::Lerp(-SwampRoom::Half, SwampWaterStart, .62f)
        : Area == 4 ? FMath::Lerp(SwampWaterEnd, SwampRoom::Half, .45f)
        : SwampWaterStart + Water * (Area == 1 ? .18f : Area == 2 ? .5f : .82f);
    return FVector(X, Side ? -550.f : 550.f, 10.f);
}

FVector AArenaGameMode::SwampPortalSpot(const FVector& Near) const
{
    // A little ahead of her, or beside or behind her if a zombie arm is in the way.
    const FVector2D Offsets[] = {{550.f, 0.f}, {550.f, 450.f}, {550.f, -450.f}, {0.f, 550.f}, {0.f, -550.f}, {-550.f, 0.f}};
    FVector2D Best(Near.X + 550.f, Near.Y);
    for (const FVector2D& O : Offsets)
    {
        const FVector2D P(FMath::Clamp(static_cast<float>(Near.X) + O.X, -SwampRoom::Half + 700.f, SwampRoom::Half - 1000.f),
                          FMath::Clamp(static_cast<float>(Near.Y) + O.Y, -SwampRoom::HalfWidth + 500.f, SwampRoom::HalfWidth - 500.f));
        bool Clear = true;
        for (const FSwampArm& Arm : SwampArms) Clear &= FVector2D::Distance(P, Arm.P) > SwampRoom::ArmReach + 350.f;
        if (Clear) { Best = P; break; }
    }
    return FVector(Best.X, Best.Y, 0.f);
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
    else if (SwampStage == 2) Waves = RoomWaves(SwampSeed, SwampRoomNumber);
    const bool King = SwampStage == 2 && SwampRoomNumber >= RunRooms;
    for (int32 I = 0; I < Waves.Num(); ++I)
    {
        const FWave& W = Waves[I];
        auto* S = Site(SwampAreaPoint(W.Area, W.Side), FString::Printf(TEXT("WAVE %d"), I + 1), W.Rats + W.Frogs, 0, false);
        if (!S) continue;
        // The larger kind leads; the other is mixed in.
        const bool MostlyRats = W.Rats >= W.Frogs;
        S->GroundType = MostlyRats ? EHellgirlEnemyType::Rats : EHellgirlEnemyType::Frogs;
        S->MixType = MostlyRats ? EHellgirlEnemyType::Frogs : EHellgirlEnemyType::Rats;
        S->MixCount = MostlyRats ? W.Frogs : W.Rats;
        S->FlyingCount = 0;
        S->Difficulty = SwampStage == 3 ? 2 : 1;
        S->bInstantGroup = false;
        S->bGroupGuardsHome = false;
        SwampWaveOfSite.Add(I);
        SwampWaveAfter.Add(W.After ? FName(W.After) : NAME_None);
        SwampWavePortal.Add(W.bPortal);
    }
    // The Frog King waits on his lily pad in the run's last room and joins the wave from the middle of the water.
    if (King)
        if (auto* KingSite = Site(FVector(SwampRoom::FrogSpot.X, SwampRoom::FrogSpot.Y, 60.f), TEXT("THE FROG KING"), 1, 0, false, true))
        {
            KingSite->GroundType = EHellgirlEnemyType::FrogKing;
            KingSite->FlyingCount = 0;
            KingSite->Difficulty = 2;
            SwampWaveOfSite.Add(Waves.IndexOfByPredicate([](const FWave& W) { return W.Area == 2; }));
        }
    SwampWaveCount = Waves.Num();
    TotalSites = SpawnSites.Num();
    SoulPortal = GetWorld()->SpawnActor<AWavePortal>();
    bPortalIntroOnEnter = true;
    ExitGate = GetWorld()->SpawnActor<AWavePortal>();
}

bool AArenaGameMode::TickSwampStage(float Dt, AArenaFighter* Hero)
{
    using namespace SwampStages;
    auto* PC = Cast<AHellgirlPlayerController>(Hero->GetController());
    ActivatedSites = ClearedSites = EnemiesRemaining = 0;
    for (auto S : SpawnSites) { ActivatedSites += S->bActivated; ClearedSites += S->bCleared; EnemiesRemaining += S->LivingEnemies(); }
    Prompt.Empty();
    PromptAction = 0;

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

    // The waves, one after another, each from its fixed point once she gets near it.
    if (SwampWave < SwampWaveCount)
    {
        TArray<AEnemySpawnPoint*> Current;
        for (int32 I = 0; I < SpawnSites.Num(); ++I)
            if (SwampWaveOfSite.IsValidIndex(I) && SwampWaveOfSite[I] == SwampWave) Current.Add(SpawnSites[I]);
        bool Started = false, Beaten = true, Near = false;
        for (auto* S : Current)
        {
            Started |= S->bActivated; Beaten &= S->bCleared;
            Near |= !S->bBoss && Hero->GetActorLocation().X > S->GetActorLocation().X - NearX;
        }
        const bool Rooms = SwampStage == 2;
        const FString Counter = Rooms ? FString::Printf(TEXT("ROOM %d / %d"), SwampRoomNumber, RunRooms)
            : FString::Printf(TEXT("WAVE %d / %d"), SwampWave + 1, SwampWaveCount);
        if (!Started)
        {
            SwampWaveClock -= Dt;
            if (!Near) { Objective = Counter + TEXT(" — Press on through the swamp"); return true; }
            Objective = Counter + TEXT(" — Incoming");
            if (SwampWaveClock > 0.f) return true;
            for (auto* S : Current) S->bEnabled = S->bActivated = true;
            return true;
        }
        if (!Beaten)
        {
            Objective = Counter + (Rooms && SwampRoomNumber >= RunRooms && Current.ContainsByPredicate([](const AEnemySpawnPoint* S) { return S->bBoss; })
                ? TEXT(" — Defeat the Frog King") : TEXT(" — Rats and frogs"));
            return true;
        }
        const FName After = SwampWaveAfter.IsValidIndex(SwampWave) ? SwampWaveAfter[SwampWave] : NAME_None;
        if (SwampWavePortal.IsValidIndex(SwampWave) && SwampWavePortal[SwampWave])
        {
            // A blue portal: her conversation when Hellgirl steps in, then continue, stock Souls or buy upgrades.
            const FName PortalId(*FString::Printf(TEXT("S%d_PortalAfterWave%d"), SwampStage, SwampWave + 1));
            if (!PlayedStory.Contains(PortalId))
            {
                if (SoulPortal && OpenPortalId != PortalId)
                {
                    SoulPortal->Open(SwampPortalSpot(Hero->GetActorLocation()), Hero->GetActorLocation(), false);
                    OpenPortalId = PortalId; OpenPortalIntro = After; bPortalMenuDeclined = false;
                    RollPortalOffers();
                }
                Objective = Counter + TEXT(" CLEARED — A blue portal has opened / Step inside");
                TickPortalMenus(Hero);
                return true;
            }
        }
        else if (!After.IsNone() && !SwampSaid(After)) return true;
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
    TickPortalMenus(Hero);
    return true;
}

// -HellgirlSwampStageCheck (with Swamp=1?Stage=S): walks the stage's script, stepping Hellgirl up to each wave's point
// and beating the wave once it starts. Checks:
// - Stage I runs ten waves, Stage III two, a Stage II room three to five (room 10 five, with the Frog King).
// - Every wave is only rats and frogs, starts only once she is near its point, and the points run west to east.
// - Stage I: the first mud sends only rats, the middle of the water only frogs, and blue portals open after waves 2,
//   6 and 9 (continued here).
// - Then the purple portal opens (a mid-run Stage II room opens the light instead).
// - Every conversation the stages use is in Content/Dialogue/LevelTwo.ini.
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
    for (const TCHAR* Id : {TEXT("S1_Intro"), TEXT("S1_Portal1"), TEXT("S1_Portal2"), TEXT("S1_Portal3"), TEXT("S2_Room1"), TEXT("S2_Room5"),
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
    // The points run west to east, wave after wave; Stage I's first mud is rats only and the middle of the water frogs only.
    float LastX = -SwampRoom::Half;
    for (int32 I = 0; I < SpawnSites.Num(); ++I)
    {
        const AEnemySpawnPoint* S = SpawnSites[I];
        if (S->bBoss) continue;
        const float X = static_cast<float>(S->GetActorLocation().X);
        if (X < LastX - 1.f) { Finish(false, FString::Printf(TEXT("wave %d comes from behind the wave before it"), I + 1)); return; }
        LastX = X;
        if (SwampStage == 1 && SwampWaveOfSite.IsValidIndex(I))
        {
            const int32 Area = StageOne[SwampWaveOfSite[I]].Area;
            const bool OneKind = S->MixCount == 0;
            if ((Area == 0 && !(OneKind && S->GroundType == EHellgirlEnemyType::Rats)) || (Area == 2 && !(OneKind && S->GroundType == EHellgirlEnemyType::Frogs)))
            { Finish(false, FString::Printf(TEXT("wave %d has the wrong enemies for its area"), I + 1)); return; }
            if (!FVector2D(S->GetActorLocation()).Equals(FVector2D(SwampAreaPoint(Area, StageOne[SwampWaveOfSite[I]].Side)), 1.f))
            { Finish(false, FString::Printf(TEXT("wave %d is not at its area's point"), I + 1)); return; }
        }
    }
    int32 Waves = 0;
    bool LightOpen = false;
    TArray<int32> PortalsAfter;
    const FVector Home = Hero->GetActorLocation();
    for (int32 Guard = 0; Guard < 200; ++Guard)
    {
        // Far from the next wave's point, it must not start.
        if (SwampWave < SwampWaveCount)
            for (int32 I = 0; I < SpawnSites.Num(); ++I)
                if (SwampWaveOfSite.IsValidIndex(I) && SwampWaveOfSite[I] == SwampWave && !SpawnSites[I]->bBoss && !SpawnSites[I]->bActivated)
                {
                    const FVector Point = SpawnSites[I]->GetActorLocation();
                    if (Point.X - NearX - 1500.f > -SwampRoom::Half + 400.f)
                    {
                        Hero->SetActorLocation(FVector(Point.X - NearX - 1500.f, 0.f, Home.Z));
                        TickSwampStage(10.f, Hero);
                        if (SpawnSites[I]->bActivated) { Finish(false, FString::Printf(TEXT("wave %d started before she came near"), SwampWave + 1)); return; }
                    }
                    Hero->SetActorLocation(FVector(Point.X - 700.f, 0.f, Home.Z));
                    break;
                }
        const bool Shut = TickSwampStage(10.f, Hero);
        if (!Shut) { LightOpen = true; break; }
        if (ExitGate && ExitGate->IsOpen()) break;
        if (IsSoulPortalOpen()) { PortalsAfter.Add(Waves); ChoosePortal(EPortalChoice::Continue); continue; }
        TSet<int32> Started;
        for (int32 I = 0; I < SpawnSites.Num(); ++I)
            if (SpawnSites[I]->bActivated && !SpawnSites[I]->bCleared) { SpawnSites[I]->bCleared = true; Started.Add(SwampWaveOfSite.IsValidIndex(I) ? SwampWaveOfSite[I] : -1); }
        if (Started.Num() > 1) { Finish(false, TEXT("two waves started at once")); return; }
        Waves += Started.Num();
    }
    const int32 Expected = SwampStage == 1 ? UE_ARRAY_COUNT(StageOne) : SwampStage == 3 ? UE_ARRAY_COUNT(StageThree) : SwampWaveCount;
    const bool WantLight = SwampStage == 2 && SwampRoomNumber < RunRooms;
    const TArray<int32> WantPortals = SwampStage == 1 ? TArray<int32>{2, 6, 9} : TArray<int32>{};
    UE_LOG(LogTemp, Display, TEXT("Swamp stage check: stage %d room %d: %d waves, %d blue portals, light %d, portal %d"), SwampStage, SwampRoomNumber, Waves,
        PortalsAfter.Num(), LightOpen, ExitGate && ExitGate->IsOpen());
    if (Waves != Expected || (SwampStage == 2 && (Waves < 3 || Waves > 5 || (King && Waves != 5))))
    { Finish(false, FString::Printf(TEXT("%d waves, expected %d"), Waves, Expected)); return; }
    if (PortalsAfter != WantPortals) { Finish(false, FString::Printf(TEXT("%d blue portals, not after the right waves"), PortalsAfter.Num())); return; }
    if (WantLight != LightOpen || (!WantLight && !(ExitGate && ExitGate->IsOpen()))) { Finish(false, TEXT("the way out did not open")); return; }
    Finish(true, FString::Printf(TEXT("stage %d room %d: %d waves of rats and frogs from their points%s%s, then %s"), SwampStage, SwampRoomNumber, Waves,
        King ? TEXT(" with the Frog King") : TEXT(""), PortalsAfter.Num() ? TEXT(", blue portals after waves 2, 6 and 9") : TEXT(""),
        WantLight ? TEXT("the light to the next room") : TEXT("the purple portal")));
#endif
}
