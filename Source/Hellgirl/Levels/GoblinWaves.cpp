#include "Rules/EnemyTuning.h"
#include "Levels/ArenaGameMode.h"
#include "Levels/WavePortal.h"
#include "Fighter/ArenaFighter.h"
#include "Enemies/EnemySpawnPoint.h"
#include "Progress/HellgirlWallet.h"
#include "Progress/CampaignProgress.h"
#include "UI/HellgirlPlayerController.h"
#include "Engine/StaticMeshActor.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

// World I Stages 2 and 3, from "02 Dialog - Goblins Stage 2 WAVES.txt" and "03 Dialog - Goblins Stage 3.txt".
// Each stage is a script of steps taken in order: waves, conversations, soul portals, the Queen, the exit.
// A step is finished when its fact holds (its sites are cleared, its conversation or portal id is in PlayedStory),
// so a loaded save picks the script up where it was.
namespace
{
enum class EScriptStep : uint8 { Wave, Story, Portal, Boss, Exit };
struct FScriptStep
{
    EScriptStep Kind = EScriptStep::Wave;
    TArray<int32> Sites;   // Wave / Boss: spawn sites (a wave can come from several at once)
    FName Id;              // Story: conversation; Portal: the id recorded when the player continues
    FName Intro;           // Portal: a conversation shown when it first opens
    FVector Spot = FVector::ZeroVector;
    int32 Gate = -1;       // Portal: the castle gate that opens once the player continues
};
FScriptStep StepWave(TArray<int32> Sites) { FScriptStep S; S.Kind = EScriptStep::Wave; S.Sites = MoveTemp(Sites); return S; }
FScriptStep StepStory(const TCHAR* Id) { FScriptStep S; S.Kind = EScriptStep::Story; S.Id = Id; return S; }
FScriptStep StepPortal(const TCHAR* Id, FVector Spot, int32 Gate = -1, const TCHAR* Intro = nullptr)
{ FScriptStep S; S.Kind = EScriptStep::Portal; S.Id = Id; S.Spot = Spot; S.Gate = Gate; if (Intro) S.Intro = Intro; return S; }
FScriptStep StepBoss(int32 Site) { FScriptStep S; S.Kind = EScriptStep::Boss; S.Sites = {Site}; return S; }
FScriptStep StepExit() { FScriptStep S; S.Kind = EScriptStep::Exit; return S; }

struct FWaveSite { FVector Position; int32 Count; };
// Stage 2 stays in the walled middle of the ruins. Waves 1-6 come from around the centre;
// wave 7 is the goblin army, two huge packs charging in from both ends.
const FWaveSite StageTwoSites[] = {
    {FVector(-900,-700,10),3}, {FVector(1000,600,10),4}, {FVector(-400,1200,10),5},
    {FVector(1100,-900,10),5}, {FVector(-1200,300,10),6}, {FVector(600,1300,10),7},
    {FVector(-2850,0,10),9}, {FVector(2750,0,10),9}};
// Stage 3 crosses the ruins: waves 1-4 in the first section, 5-14 in the second, the Queen in the third.
const FWaveSite StageThreeSites[] = {
    {FVector(-4700,-800,10),3}, {FVector(-4300,900,10),4}, {FVector(-5300,700,10),4}, {FVector(-4500,-300,10),5},
    {FVector(-2400,-900,10),5}, {FVector(-2000,1000,10),6}, {FVector(-900,-1200,10),6}, {FVector(-500,900,10),7},
    {FVector(400,-700,10),7}, {FVector(900,1100,10),8}, {FVector(1500,-1000,10),8}, {FVector(1900,800,10),9},
    {FVector(2300,-400,10),9}, {FVector(2600,700,10),10}};
const FVector QueenThrone(4100,0,10);

const TArray<FScriptStep>& ScriptFor(int32 Level)
{
    static const TArray<FScriptStep> Two = {
        StepStory(TEXT("L2_Start")),
        StepWave({0}), StepWave({1}), StepWave({2}),
        StepPortal(TEXT("L2_Portal1"), FVector(0,-650,0), -1, TEXT("L2_PortalHelp")),
        StepWave({3}), StepWave({4}), StepWave({5}),
        StepPortal(TEXT("L2_Portal2"), FVector(0,650,0)),
        StepStory(TEXT("L2_Army")),
        StepWave({6,7}),
        StepExit()};
    static const TArray<FScriptStep> Three = {
        StepWave({0}), StepWave({1}),
        StepStory(TEXT("L3_Goblins")),
        StepWave({2}), StepWave({3}),
        StepPortal(TEXT("L3_Portal1"), FVector(-3950,0,0), 0),
        StepWave({4}), StepWave({5}),
        StepStory(TEXT("L3_TalkToMe")),
        StepWave({6}), StepWave({7}), StepWave({8}), StepWave({9}),
        StepStory(TEXT("L3_Subjects")),
        StepPortal(TEXT("L3_Portal2"), FVector(-200,0,0)),
        StepStory(TEXT("L3_SubjectsReply")),
        StepWave({10}), StepWave({11}), StepWave({12}), StepWave({13}),
        StepPortal(TEXT("L3_Portal3"), FVector(2850,0,0), 1),
        StepBoss(14),
        StepExit()};
    return Level == 3 ? Three : Two;
}
// The castle's three sections, split by the two gated walls.
int32 SectionOf(float X) { return X < -3400.f ? 0 : X < 3300.f ? 1 : 2; }
int32 HeroSection(float X) { return X > 3500.f ? 2 : X > -3200.f ? 1 : 0; }
}

void AArenaGameMode::BuildGoblinWaves()
{
    if (!bEndless)
    {
        if (CampaignLevel == 2)
            for (int32 I = 0; I < UE_ARRAY_COUNT(StageTwoSites); ++I)
            {
                auto* S = Site(StageTwoSites[I].Position, I < 6 ? FString::Printf(TEXT("WAVE %d"), I + 1) : TEXT("WAVE 7 / THE GOBLIN ARMY"), StageTwoSites[I].Count, 0, false);
                // The army charges from its end of the map instead of waiting there.
                if (I >= 6) { S->bInstantGroup = true; S->bGroupGuardsHome = false; }
            }
        else
        {
            for (int32 I = 0; I < UE_ARRAY_COUNT(StageThreeSites); ++I)
                Site(StageThreeSites[I].Position, FString::Printf(TEXT("WAVE %d"), I + 1), StageThreeSites[I].Count, 0, false);
            Site(QueenThrone, TEXT("THE GOBLIN QUEEN"), 1, 0, false, true);
        }
    }
    SoulPortal = GetWorld()->SpawnActor<AWavePortal>();
}

bool AArenaGameMode::IsSoulPortalOpen() const { return SoulPortal && SoulPortal->IsOpen(); }
bool AArenaGameMode::IsExitOpen() const { return ExitGate ? ExitGate->IsOpen() : ExitPortal && !ExitPortal->IsHidden(); }
void AArenaGameMode::ShowExitPortal(bool Open)
{
    if (!ExitGate) { if (ExitPortal) ExitPortal->SetActorHiddenInGame(!Open); return; }
    if (ExitPortal) ExitPortal->SetActorHiddenInGame(true);
    // The exit faces back the way Hellgirl came.
    if (Open && !ExitGate->IsOpen()) ExitGate->Open(ExitPosition, ExitPosition - FVector(600, 0, 0), true);
    else if (!Open && ExitGate->IsOpen()) ExitGate->Close();
}

void AArenaGameMode::CompleteLevel()
{
    if (bLevelCompleted) return;
    bLevelCompleted = true;
    // Automated checks never touch the player's real wallet or progress.
    if (FString(FCommandLine::Get()).Contains(TEXT("-Hellgirl"))) return;
    if (auto* Wallet = Cast<UHellgirlWallet>(GetGameInstance())) Wallet->BankCarried();
    if (!bForestRun && !bEndless && !bLegacyMap && GetUnlockedLevel() < CampaignLevel + 1)
    {
        GConfig->SetInt(TEXT("HellgirlCampaign"), TEXT("UnlockedLevel"), CampaignLevel + 1, GGameUserSettingsIni);
        GConfig->Flush(false, GGameUserSettingsIni);
    }
}

void AArenaGameMode::ChoosePortal(EPortalChoice Choice)
{
    auto* Wallet = Cast<UHellgirlWallet>(GetGameInstance());
    switch (Choice)
    {
    case EPortalChoice::Continue:
        if (SoulPortal && SoulPortal->IsOpen() && !IsPortalIntroPending())
        {
            if (!OpenPortalId.IsNone()) PlayedStory.Add(OpenPortalId);
            SoulPortal->Close();
            OpenPortalId = OpenPortalIntro = NAME_None;
            WaveCountdown = 1.5f;
        }
        break;
    case EPortalChoice::Stock:
        if (Wallet) Wallet->BankCarried();
        break;
    case EPortalChoice::Leave:
        if (bEndless) { if (Wallet) Wallet->BankCarried(); TravelToHub(); }
        else UseExitPortal();
        break;
    case EPortalChoice::Stay:
        bPortalMenuDeclined = true;
        break;
    }
}

void AArenaGameMode::TickPortalMenus(AArenaFighter* Hero)
{
    auto* PC = Cast<AHellgirlPlayerController>(Hero->GetController());
    const FVector Where = Hero->GetActorLocation();
    AWavePortal* Near = SoulPortal && SoulPortal->IsNear(Where) ? SoulPortal.Get() : ExitGate && ExitGate->IsNear(Where) ? ExitGate.Get() : nullptr;
    // A portal with help text waits until it has been read.
    if (!Near || (Near == SoulPortal && IsPortalIntroPending())) { bPortalMenuDeclined = false; return; }
    Prompt = TEXT("E / Y  open the portal");
    // Walking into the portal opens its menu; after closing it, E / Y opens it again.
    const bool Asked = PC && (PC->WasInputKeyJustPressed(EKeys::E) || PC->WasInputKeyJustPressed(EKeys::Gamepad_FaceButton_Top) || PC->WasInputKeyJustPressed(EKeys::Gamepad_DPad_Up));
    if (PC && !PC->IsPauseMenuOpen() && (!bPortalMenuDeclined || Asked))
    {
        PC->OpenPortalMenu(Near->IsExit());
        bPortalMenuDeclined = true;
    }
}

void AArenaGameMode::TickGoblinWaves(float Dt)
{
    if (bEndless) { if (!RunEndlessCheck()) TickEndless(Dt); return; }
    auto* Hero = Cast<AArenaFighter>(UGameplayStatics::GetPlayerPawn(this, 0));
    if (!Hero || !Hero->IsAlive()) return;
    const TArray<FScriptStep>& Script = ScriptFor(CampaignLevel);
    ActivatedSites = ClearedSites = EnemiesRemaining = 0;
    for (auto S : SpawnSites) { ActivatedSites += S->bActivated; ClearedSites += S->bCleared; EnemiesRemaining += S->LivingEnemies(); }
    auto Done = [&](const FScriptStep& S)
    {
        switch (S.Kind)
        {
        case EScriptStep::Wave: { bool All = true; for (int32 I : S.Sites) All &= SpawnSites.IsValidIndex(I) && SpawnSites[I]->bCleared; return All; }
        case EScriptStep::Story: return !bStoryEnabled || PlayedStory.Contains(S.Id);
        case EScriptStep::Portal: return PlayedStory.Contains(S.Id);
        case EScriptStep::Boss: return SpawnSites.IsValidIndex(S.Sites[0]) && SpawnSites[S.Sites[0]]->bCleared && !SurrenderedQueen.IsValid()
            && (!bStoryEnabled || PlayedStory.Contains(TEXT("L3_Escaped")));
        default: return false;
        }
    };
    int32 Step = 0;
    while (Step < Script.Num() - 1 && Done(Script[Step])) ++Step;
    // A gate opens once the player has continued through its portal.
    for (int32 G = 0; G < SectionGates.Num(); ++G)
    {
        bool Open = false;
        for (const FScriptStep& S : Script) if (S.Kind == EScriptStep::Portal && S.Gate == G) Open |= PlayedStory.Contains(S.Id);
        SectionGates[G]->SetActorHiddenInGame(Open); SectionGates[G]->SetActorEnableCollision(!Open); SectionBarriers[G]->SetActorEnableCollision(!Open);
    }
    int32 WaveNumber = 0, TotalWaves = 0;
    for (int32 I = 0; I < Script.Num(); ++I) if (Script[I].Kind == EScriptStep::Wave) { ++TotalWaves; if (I <= Step) ++WaveNumber; }
    if (Step != ScriptStep) { ScriptStep = Step; WaveCountdown = WaveNumber <= 1 ? 2.f : FMath::Max(WaveCountdown, EnemyTuning::WaveBreak(3.f)); }
    const FScriptStep& S = Script[Step];
    if (S.Kind != EScriptStep::Portal && SoulPortal && SoulPortal->IsOpen()) { SoulPortal->Close(); OpenPortalId = OpenPortalIntro = NAME_None; }
    ShowExitPortal(S.Kind == EScriptStep::Exit);
    Prompt.Empty();
    const float X = static_cast<float>(Hero->GetActorLocation().X);
    switch (S.Kind)
    {
    case EScriptStep::Wave:
    {
        const bool Army = S.Sites.Num() > 1;
        if (HeroSection(X) < SectionOf(static_cast<float>(SpawnSites[S.Sites[0]]->GetActorLocation().X)))
        { Objective = TEXT("The gate is open / Move on"); break; }
        bool Started = true;
        for (int32 I : S.Sites) Started &= SpawnSites[I]->bActivated;
        if (!Started)
        {
            WaveCountdown -= Dt;
            if (WaveCountdown <= 0.f) for (int32 I : S.Sites) { SpawnSites[I]->bEnabled = true; SpawnSites[I]->bActivated = true; }
        }
        Objective = FString::Printf(TEXT("WAVE %d / %d  —  %s"), WaveNumber, TotalWaves,
            !Started ? TEXT("Incoming") : Army ? TEXT("The goblin army charges!") : TEXT("Defeat the goblins"));
        break;
    }
    case EScriptStep::Story:
        QueueStory(S.Id);
        break;
    case EScriptStep::Portal:
        if (SoulPortal && OpenPortalId != S.Id)
        {
            SoulPortal->Open(S.Spot, Hero->GetActorLocation(), false);
            OpenPortalId = S.Id; OpenPortalIntro = S.Intro; bPortalMenuDeclined = false;
        }
        if (!S.Intro.IsNone()) QueueStory(S.Intro);
        Objective = TEXT("A soul portal has opened / Step inside");
        break;
    case EScriptStep::Boss:
    {
        auto* Throne = SpawnSites[S.Sites[0]].Get();
        if (!Throne->bActivated)
        {
            if (X > 3500.f) { Throne->bEnabled = true; Throne->bActivated = true; }
            Objective = TEXT("The gate is open / Enter the Queen's court");
        }
        else
        {
            // She speaks once she stands in front of Hellgirl.
            if (Throne->LivingEnemies() > 0) QueueStory(TEXT("L3_BossStart"));
            Objective = SurrenderedQueen.IsValid() ? TEXT("The Goblin Queen flees") : TEXT("Defeat the Goblin Queen");
        }
        break;
    }
    case EScriptStep::Exit:
        CompleteLevel();
        Objective = TEXT("LEVEL COMPLETE / Enter the portal to return to camp");
        break;
    }
    TickPortalMenus(Hero);
}

void AArenaGameMode::StartEndless()
{
    UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this)), true, TEXT("StageMap=1?CampaignLevel=2?Endless=1"));
}

void AArenaGameMode::TickEndless(float Dt)
{
    auto* Hero = Cast<AArenaFighter>(UGameplayStatics::GetPlayerPawn(this, 0));
    if (!Hero || !Hero->IsAlive()) return;
    ActivatedSites = ClearedSites = EnemiesRemaining = 0;
    bool WaveDone = true;
    for (int32 I = 0; I < SpawnSites.Num(); ++I)
    {
        auto* S = SpawnSites[I].Get();
        ActivatedSites += S->bActivated; ClearedSites += S->bCleared; EnemiesRemaining += S->LivingEnemies();
        if (I >= EndlessWaveStart) WaveDone &= S->bCleared;
    }
    Prompt.Empty();
    const int32 Best = HellgirlProgress::EndlessBest();
    if (EndlessWave > 0 && !WaveDone)
    {
        Objective = FString::Printf(TEXT("ENDLESS  —  WAVE %d  ·  best %d"), EndlessWave, Best);
        TickPortalMenus(Hero);
        return;
    }
    if (EndlessWave > 0) HellgirlProgress::RecordEndless(EndlessWave);
    if (ScriptStep != EndlessWave) { ScriptStep = EndlessWave; WaveCountdown = EndlessWave == 0 ? 2.5f : EnemyTuning::WaveBreak(3.f); }
    // Every third wave cleared, a soul portal: continue, stock the souls, or leave with them.
    const FName PortalId(*FString::Printf(TEXT("Endless_Portal_%d"), EndlessWave));
    if (EndlessWave > 0 && EndlessWave % 3 == 0 && !PlayedStory.Contains(PortalId))
    {
        if (SoulPortal && OpenPortalId != PortalId)
        {
            SoulPortal->Open(FVector(0, -650, 0), Hero->GetActorLocation(), false);
            OpenPortalId = PortalId; bPortalMenuDeclined = false;
        }
        Objective = FString::Printf(TEXT("WAVE %d CLEARED  /  Step into the soul portal"), EndlessWave);
        TickPortalMenus(Hero);
        return;
    }
    Objective = EndlessWave == 0 ? TEXT("ENDLESS  —  The goblins are coming") : FString::Printf(TEXT("WAVE %d CLEARED  /  More are coming"), EndlessWave);
    WaveCountdown -= Dt;
    if (WaveCountdown > 0.f) return;
    ++EndlessWave;
    EndlessWaveStart = SpawnSites.Num();
    FRandomStream Random(EndlessWave * 7919);
    // Tougher goblins every four waves; every fifth wave is an army charging from both ends.
    const int32 Difficulty = FMath::Min(1 + (EndlessWave - 1) / 4, 6);
    TArray<AEnemySpawnPoint*> Wave;
    if (EndlessWave % 5 == 0)
    {
        const int32 Count = FMath::Min(4 + EndlessWave / 2, 14);
        for (const float End : {-2850.f, 2750.f})
            if (auto* S = Site(FVector(End, Random.FRandRange(-600.f, 600.f), 10), FString::Printf(TEXT("WAVE %d / ARMY"), EndlessWave), Count, 0, false))
            { S->bInstantGroup = true; S->bGroupGuardsHome = false; Wave.Add(S); }
    }
    else
    {
        const float Angle = Random.FRandRange(0.f, 2.f * PI), Radius = Random.FRandRange(900.f, 1700.f);
        if (auto* S = Site(FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 10), FString::Printf(TEXT("WAVE %d"), EndlessWave), FMath::Min(3 + EndlessWave, 16), 0, false))
            Wave.Add(S);
    }
    for (auto* S : Wave) { S->Difficulty = Difficulty; S->bEnabled = true; S->bActivated = true; }
}

FVector AArenaGameMode::GetSoulPortalLocation() const { return SoulPortal ? SoulPortal->GetActorLocation() : FVector::ZeroVector; }

// -HellgirlEndlessCheck: ten endless waves (armies on 5 and 10, soul portals after 3, 6 and 9, tougher goblins),
// the best wave recorded, then Hellgirl falls and every carried soul is lost.
bool AArenaGameMode::RunEndlessCheck()
{
#if WITH_DEV_AUTOMATION_TESTS
    if (!bEndless || !FParse::Param(FCommandLine::Get(), TEXT("HellgirlEndlessCheck"))) return false;
    static bool bRunning = false; // the Tick below comes back through here
    if (bRunning) return true;
    auto* Hero = Cast<AArenaFighter>(UGameplayStatics::GetPlayerPawn(this, 0));
    auto* Wallet = Cast<UHellgirlWallet>(GetGameInstance());
    if (!Hero || !Wallet || GetWorld()->GetTimeSeconds() < 1.f) return true;
    bRunning = true;
    bool Passed = SpawnSites.IsEmpty() && Wallet->bCarrying;
    int32 Portals = 0, Armies = 0;
    for (int32 Guard = 0; Guard < 200 && EndlessWave < 10; ++Guard)
    {
        TickEndless(10.f);
        if (IsSoulPortalOpen()) { ++Portals; Passed &= EndlessWave % 3 == 0; ChoosePortal(EPortalChoice::Continue); continue; }
        int32 Started = 0;
        for (int32 I = EndlessWaveStart; I < SpawnSites.Num(); ++I) if (!SpawnSites[I]->bCleared) { ++Started; SpawnSites[I]->bCleared = true; }
        if (Started == 2) { ++Armies; Passed &= EndlessWave % 5 == 0; }
    }
    TickEndless(0.f);
    Passed &= EndlessWave == 10 && Portals == 3 && Armies == 2 && HellgirlProgress::EndlessBest() >= 9
        && SpawnSites.Last()->Difficulty == 3 && SpawnSites[0]->Difficulty == 1;
    Wallet->Carried = 7;
    Hero->ApplyPhysicsDamage(1000000.f, FVector::ZeroVector);
    Tick(0.f);
    Passed &= !Hero->IsAlive() && Wallet->Carried == 0 && Wallet->LastLost == 7;
    if (Passed) { UE_LOG(LogTemp, Display, TEXT("ENDLESS CHECK PASSED: ten waves, armies on 5 and 10, soul portals after 3/6/9, tougher goblins, best wave, carried souls lost on death")); }
    else { UE_LOG(LogTemp, Error, TEXT("ENDLESS CHECK FAILED: wave %d, %d portals, %d armies, best %d, carried %lld"), EndlessWave, Portals, Armies, HellgirlProgress::EndlessBest(), Wallet->Carried); }
    FPlatformMisc::RequestExitWithStatus(false, Passed ? 0 : 1);
    return true;
#else
    return false;
#endif
}
bool AArenaGameMode::IsPortalIntroPending() const { return bStoryEnabled && !OpenPortalIntro.IsNone() && !PlayedStory.Contains(OpenPortalIntro); }
