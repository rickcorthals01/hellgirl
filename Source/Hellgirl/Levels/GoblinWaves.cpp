#include "Rules/EnemyTuning.h"
#include "Levels/ArenaGameMode.h"
#include "Levels/WavePortal.h"
#include "Fighter/ArenaFighter.h"
#include "Enemies/EnemySpawnPoint.h"
#include "Progress/HellgirlWallet.h"
#include "Progress/CampaignProgress.h"
#include "Progress/Achievements.h"
#include "Rules/PortalUpgrades.h"
#include "Rules/SoulRewards.h"
#include "UI/HellgirlPlayerController.h"
#include "Engine/StaticMeshActor.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "EngineUtils.h"
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
    {FVector(-2400,0,10),9}, {FVector(2300,0,10),9}};
// Stage 3 crosses the ruins: waves 1-4 in the first section, 5-14 in the second, the Queen in the third.
const FWaveSite StageThreeSites[] = {
    {FVector(-4700,-800,10),3}, {FVector(-4300,900,10),4}, {FVector(-5300,700,10),4}, {FVector(-4500,-300,10),5},
    {FVector(-2400,-900,10),5}, {FVector(-2000,1000,10),6}, {FVector(-900,-1200,10),6}, {FVector(-500,900,10),7},
    {FVector(400,-700,10),7}, {FVector(900,1100,10),8}, {FVector(1500,-1000,10),8}, {FVector(1900,800,10),9},
    {FVector(2300,-400,10),9}, {FVector(2350,700,10),10}};
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
    // Winning Stage 2 unlocks endless goblins; winning Stage 3 brings the goblin (and his shop) to camp.
    // World II: each swamp stage won unlocks the next (Stage I itself opens once World I is won).
    if (bSwamp && SwampStage > 0) HellgirlProgress::SetFlag(*FString::Printf(TEXT("SwampStage%dWon"), SwampStage));
    else if (!bLegacyMap && !bForestRun && !bEndless && (CampaignLevel == 2 || CampaignLevel == 3))
        HellgirlProgress::SetFlag(CampaignLevel == 2 ? TEXT("Stage2Won") : TEXT("Stage3Won"));
    // Won: every Soul earned in the level plus the speed, combo and energy bonuses become Soul Coins. Automated
    // checks work the reward out but never touch the player's real wallet or progress.
    const bool bAutomated = FString(FCommandLine::Get()).Contains(TEXT("-Hellgirl"));
    if (auto* Wallet = Cast<UHellgirlWallet>(GetGameInstance())) Wallet->FinishLevel(!bAutomated);
    if (bAutomated) return;
    if (!bForestRun && !bEndless && !bLegacyMap && !bSwamp && GetUnlockedLevel() < CampaignLevel + 1)
    {
        GConfig->SetInt(TEXT("HellgirlCampaign"), TEXT("UnlockedLevel"), CampaignLevel + 1, GGameUserSettingsIni);
        GConfig->Flush(false, GGameUserSettingsIni);
    }
}

void AArenaGameMode::ChoosePortal(EPortalChoice Choice)
{
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
        // Sent to camp now: safe from a fall, but no longer spendable on upgrades.
        if (auto* Wallet = Cast<UHellgirlWallet>(GetGameInstance())) Wallet->StockSouls(!FString(FCommandLine::Get()).Contains(TEXT("-Hellgirl")));
        break;
    case EPortalChoice::Leave:
        // Leaving an endless run at a portal counts as winning it: its Souls become Soul Coins.
        if (bEndless) { CompleteLevel(); TravelToHub(); }
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
    // A portal with help text waits until it has been read. The swamp's portals speak once she steps into them.
    if (Near == SoulPortal && IsPortalIntroPending() && bPortalIntroOnEnter) QueueStory(OpenPortalIntro);
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
    RunNaturalWavesCheck(Dt);
    if (bEndless) { if (!RunEndlessCheck()) TickEndless(Dt); return; }
    auto* Hero = Cast<AArenaFighter>(UGameplayStatics::GetPlayerPawn(this, 0));
    if (!Hero || !Hero->IsAlive()) return;
    const TArray<FScriptStep>& Script = ScriptFor(CampaignLevel);
    ActivatedSites = ClearedSites = EnemiesRemaining = 0;
    for (auto S : SpawnSites) { ActivatedSites += S->bActivated; ClearedSites += S->bCleared; EnemiesRemaining += S->LivingEnemies(); }
    RescueStragglers(Hero, Dt);
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
            RollPortalOffers();
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
    // Story to come: the first time a run reaches wave 50 after the Queen fled in Stage 3, the conversation
    // [E_Wave50] in Content/Dialogue/LevelOne.ini plays. Until it is written nothing happens (and nothing is marked).
    if (EndlessWave >= 50 && !bWave50Tried && HellgirlProgress::Flag(TEXT("Stage3Won")) && !HellgirlProgress::Flag(TEXT("EndlessWave50")))
        if (auto* PC = Cast<AHellgirlPlayerController>(Hero->GetController()); PC && !PC->IsPauseMenuOpen())
        {
            bWave50Tried = true;
            if (PC->ShowConversation(TEXT("E_Wave50"))) HellgirlProgress::SetFlag(TEXT("EndlessWave50"));
        }
    ActivatedSites = ClearedSites = EnemiesRemaining = 0;
    bool WaveDone = true;
    for (int32 I = 0; I < SpawnSites.Num(); ++I)
    {
        auto* S = SpawnSites[I].Get();
        ActivatedSites += S->bActivated; ClearedSites += S->bCleared; EnemiesRemaining += S->LivingEnemies();
        if (I >= EndlessWaveStart) WaveDone &= S->bCleared;
    }
    Prompt.Empty();
    RescueStragglers(Hero, Dt);
    const int32 Best = HellgirlProgress::EndlessBest();
    if (EndlessWave > 0 && !WaveDone)
    {
        Objective = FString::Printf(TEXT("ENDLESS  —  WAVE %d  ·  best %d"), EndlessWave, Best);
        TickPortalMenus(Hero);
        return;
    }
    if (EndlessWave > 0) HellgirlProgress::RecordEndless(EndlessWave);
    // Clearing wave 50 is an achievement (and opens the Goblin Queen outfit in the shop).
    if (EndlessWave >= 50) HellgirlAchievements::Unlock(HellgirlAchievements::EndlessGoblins50);
    if (ScriptStep != EndlessWave) { ScriptStep = EndlessWave; WaveCountdown = EndlessWave == 0 ? 2.5f : EnemyTuning::WaveBreak(3.f); }
    // Every third wave cleared, a soul portal: continue, stock the souls, or leave with them.
    const FName PortalId(*FString::Printf(TEXT("Endless_Portal_%d"), EndlessWave));
    if (EndlessWave > 0 && EndlessWave % 3 == 0 && !PlayedStory.Contains(PortalId))
    {
        if (SoulPortal && OpenPortalId != PortalId)
        {
            SoulPortal->Open(FVector(0, -650, 0), Hero->GetActorLocation(), false);
            OpenPortalId = PortalId; bPortalMenuDeclined = false;
            RollPortalOffers();
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
    // Tougher goblins every four waves; every fifth wave is an army charging from both ends, and every tenth the
    // Goblin Queen joins it (a boss fight with no dialogue in endless).
    const int32 Difficulty = FMath::Min(1 + (EndlessWave - 1) / 4, 6);
    TArray<AEnemySpawnPoint*> Wave;
    if (EndlessWave % 5 == 0)
    {
        const int32 Count = FMath::Min(4 + EndlessWave / 2, 14);
        for (const float End : {-2400.f, 2300.f})
            if (auto* S = Site(FVector(End, Random.FRandRange(-600.f, 600.f), 10), FString::Printf(TEXT("WAVE %d / ARMY"), EndlessWave), Count, 0, false))
            { S->bInstantGroup = true; S->bGroupGuardsHome = false; Wave.Add(S); }
    }
    else
    {
        const float Angle = Random.FRandRange(0.f, 2.f * PI), Radius = Random.FRandRange(900.f, 1700.f);
        if (auto* S = Site(FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 10), FString::Printf(TEXT("WAVE %d"), EndlessWave), FMath::Min(3 + EndlessWave, 16), 0, false))
            Wave.Add(S);
    }
    if (EndlessWave % 10 == 0)
        if (auto* S = Site(FVector(0, 1500, 10), FString::Printf(TEXT("WAVE %d / THE GOBLIN QUEEN"), EndlessWave), 1, 0, false, true)) Wave.Add(S);
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
    bool Passed = SpawnSites.IsEmpty() && Wallet->bInLevel && Wallet->LevelSouls.Earned == 0;
    const int64 CoinsBefore = Wallet->Coins;
    int64 StockedTotal = 0;
    int32 Portals = 0, Armies = 0, Queens = 0;
    for (int32 Guard = 0; Guard < 200 && EndlessWave < 10; ++Guard)
    {
        TickEndless(10.f);
        if (IsSoulPortalOpen())
        {
            ++Portals; Passed &= EndlessWave % 3 == 0;
            // Upgrades: five different offers, a fresh choice each portal, paid from carried souls.
            TSet<int32> Distinct(PortalOffers);
            Passed &= PortalOffers.Num() == HellgirlUpgrades::OffersPerPortal && Distinct.Num() == PortalOffers.Num() && !IsOfferSold(0) && !IsOfferSold(1) && !IsOfferSold(2);
            Wallet->LevelSouls.Souls = 5;
            Passed &= !BuyUpgrade(0) && Wallet->LevelSouls.Souls == 5;
            Wallet->LevelSouls.Souls = 500; Wallet->LevelSouls.Earned += 500;
            const int32 Upgrade = PortalOffers[0], Before = GetUpgradeLevel(Upgrade), Price = GetUpgradeCost(Upgrade);
            // Any offer can be bought (each once): saving up buys several at one portal.
            Passed &= BuyUpgrade(0) && Wallet->LevelSouls.Souls == 500 - Price && !BuyUpgrade(0) && Wallet->LevelSouls.Souls == 500 - Price;
            const int32 SecondPrice = GetUpgradeCost(PortalOffers[1]);
            Passed &= BuyUpgrade(1) && Wallet->LevelSouls.Souls == 500 - Price - SecondPrice && Wallet->LevelSouls.Earned >= 500 && IsOfferSold(0) && IsOfferSold(1) && !IsOfferSold(2);
            Passed &= GetUpgradeLevel(Upgrade) == Before + (HellgirlUpgrades::IsInstant(Upgrade) ? 0 : 1);
            const HellgirlUpgrades::FStats Expected = HellgirlUpgrades::Stats(UpgradeLevels);
            Passed &= Hero->Upgrades.Damage == Expected.Damage && Hero->Upgrades.DamageTaken == Expected.DamageTaken && Hero->Upgrades.Speed == Expected.Speed
                && Hero->Upgrades.BonusSouls == Expected.BonusSouls;
            if (!Passed) UE_LOG(LogTemp, Error, TEXT("Upgrade purchase failed at portal %d (upgrade %d, price %d)"), Portals, Upgrade, Price);
            // Stocking sends what is left to camp at once and leaves nothing to spend.
            const int64 Left = Wallet->LevelSouls.Souls, Coins = Wallet->Coins;
            ChoosePortal(EPortalChoice::Stock);
            Passed &= Wallet->Coins == Coins + Left && Wallet->LevelSouls.Souls == 0 && Wallet->LevelSouls.Stocked == StockedTotal + Left;
            StockedTotal += Left;
            ChoosePortal(EPortalChoice::Continue);
            continue;
        }
        int32 Started = 0, Bosses = 0;
        for (int32 I = EndlessWaveStart; I < SpawnSites.Num(); ++I)
            if (!SpawnSites[I]->bCleared)
            {
                ++Started; SpawnSites[I]->bCleared = true;
                if (SpawnSites[I]->bBoss) { ++Bosses; Passed &= SpawnSites[I]->GroundType == EHellgirlEnemyType::GoblinQueen; }
            }
        if (Bosses) { ++Queens; Passed &= EndlessWave % 10 == 0; }
        if (Started - Bosses == 2) { ++Armies; Passed &= EndlessWave % 5 == 0; }
    }
    TickEndless(0.f);
    Passed &= EndlessWave == 10 && Portals == 3 && Armies == 2 && Queens == 1 && HellgirlProgress::EndlessBest() >= 9
        && SpawnSites.Last()->Difficulty == 3 && SpawnSites[0]->Difficulty == 1;
    // The Goblin Queen outfit costs 20000 Soul Coins and needs endless wave 50 cleared; both are achievements
    // (automated runs keep all of this in memory).
    {
        const int64 CoinsKept = Wallet->Coins;
        const bool bOwnedKept = Wallet->bGoblinQueenOwned;
        HellgirlAchievements::Session().Remove(HellgirlAchievements::EndlessGoblins50);
        HellgirlAchievements::Session().Remove(HellgirlAchievements::GoblinQueenOutfit);
        Wallet->bGoblinQueenOwned = false; Wallet->Coins = 25000;
        Passed &= !Wallet->BuyGoblinQueen() && Wallet->Coins == 25000; // not before wave 50
        EndlessWave = 50; EndlessWaveStart = SpawnSites.Num(); ScriptStep = 50; WaveCountdown = 5.f;
        TickEndless(0.f); // wave 50 is cleared
        Passed &= HellgirlAchievements::Has(HellgirlAchievements::EndlessGoblins50);
        Wallet->Coins = 19999;
        Passed &= !Wallet->BuyGoblinQueen();
        Wallet->Coins = 25000;
        Passed &= Wallet->BuyGoblinQueen() && Wallet->Coins == 5000 && Wallet->bGoblinQueenOwned
            && HellgirlAchievements::Has(HellgirlAchievements::GoblinQueenOutfit) && !Wallet->BuyGoblinQueen();
        if (!Passed) UE_LOG(LogTemp, Error, TEXT("Goblin Queen outfit purchase or achievements failed"));
        Wallet->Coins = CoinsKept; Wallet->bGoblinQueenOwned = bOwnedKept;
    }
    // Falling: nothing more goes to camp (the stocked Soul Coins stay).
    Wallet->LevelSouls.Souls = 7;
    const int64 EarnedBefore = Wallet->LevelSouls.Earned, CoinsAtDeath = Wallet->Coins;
    Hero->ApplyPhysicsDamage(1000000.f, FVector::ZeroVector);
    Tick(0.f);
    Passed &= !Hero->IsAlive() && Wallet->LevelSouls.Souls == 0 && Wallet->LevelSouls.Earned == 0 && Wallet->LastLost == EarnedBefore && Wallet->Coins == CoinsAtDeath && CoinsAtDeath == CoinsBefore + StockedTotal;
    // The reward: earned 100 (20 stocked), 100 s against a par of 145 s (40 kills), combo going half the fight,
    // 105 energy spent: 100 - 20 + 30 speed + 15 combo + 10 energy = 135 Soul Coins.
    const HellgirlSouls::FReward R = HellgirlSouls::Compute(100, 20, 100.f, 40, 50.f, 100.f, 105.f);
    Passed &= R.Speed == 30 && R.Combo == 15 && R.Energy == 10 && R.Total == 135 && HellgirlSouls::Compute(100, 0, 290.f, 40, 0.f, 0.f, 0.f).Total == 100;
    // Prices climb 40% per level owned.
    Passed &= HellgirlUpgrades::Cost(0, 0) == 43 && HellgirlUpgrades::Cost(0, 1) == 60 && HellgirlUpgrades::Cost(0, 2) == 77;
    if (Passed) { UE_LOG(LogTemp, Display, TEXT("ENDLESS CHECK PASSED: ten waves, armies on 5 and 10, the Goblin Queen on 10, soul portals after 3/6/9 with five fresh upgrade offers, each buyable once, stocking Souls as Soul Coins, tougher goblins, best wave, a fall depositing nothing more, the Soul Coin reward, the Goblin Queen outfit (20000, after wave 50) and its achievements")); }
    else { UE_LOG(LogTemp, Error, TEXT("ENDLESS CHECK FAILED: wave %d, %d portals, %d armies, best %d, souls %lld"), EndlessWave, Portals, Armies, HellgirlProgress::EndlessBest(), Wallet->LevelSouls.Souls); }
    FPlatformMisc::RequestExitWithStatus(false, Passed ? 0 : 1);
    return true;
#else
    return false;
#endif
}
bool AArenaGameMode::IsPortalIntroPending() const { return bStoryEnabled && !OpenPortalIntro.IsNone() && !PlayedStory.Contains(OpenPortalIntro); }

// -HellgirlNaturalWavesCheck: the waves play out for real. Hellgirl stands in the middle of the section being fought
// over and only goblins that actually reach her die, so a goblin that spawns somewhere unreachable (or a wave that
// cannot finish spawning) stalls the run and is reported. Soul portals are continued; the run passes at the exit
// (Stages 2 and 3) or after endless wave 7.
void AArenaGameMode::RunNaturalWavesCheck(float Dt)
{
#if WITH_DEV_AUTOMATION_TESTS
    if (!FParse::Param(FCommandLine::Get(), TEXT("HellgirlNaturalWavesCheck"))) return;
    auto* Hero = Cast<AArenaFighter>(UGameplayStatics::GetPlayerPawn(this, 0));
    if (!Hero) return;
    static float Clock = 0.f, SinceProgress = 0.f;
    static int32 LastProgress = -1, Portals = 0;
    Clock += Dt; SinceProgress += Dt;
    Hero->Health = Hero->MaxHealth = 100000.f;
    auto Finish = [](bool Passed) { FPlatformMisc::RequestExitWithStatus(false, Passed ? 0 : 1); };
    // Endless runs to wave 11, past the Goblin Queen on wave 10.
    if (bEndless ? EndlessWave >= 11 && Portals >= 3 : IsExitOpen())
    {
        UE_LOG(LogTemp, Display, TEXT("NATURAL WAVES CHECK PASSED: level %d%s, %d soul portals, %.0f s"), CampaignLevel, bEndless ? TEXT(" endless") : TEXT(""), Portals, Clock);
        Finish(true); return;
    }
    if (IsSoulPortalOpen()) { ++Portals; ChoosePortal(EPortalChoice::Continue); }
    // Where the fighting is: the section of the first wave not yet cleared.
    float StandX = 0.f;
    if (CampaignLevel == 3)
        for (auto Site : SpawnSites)
            if (!Site->bCleared) { const int32 Section = SectionOf(static_cast<float>(Site->GetActorLocation().X)); StandX = Section == 0 ? -4800.f : Section == 1 ? 0.f : 4100.f; break; }
    // Off the centre line, so she never stands where a soul portal opens (its menu would pause the run).
    const FVector Stand(StandX, -1100.f, Hero->GetActorLocation().Z);
    if (FVector::Dist2D(Hero->GetActorLocation(), Stand) > 300.f) { Hero->SetActorLocation(Stand + FVector(0, 0, 100)); Hero->ResetAfterRecovery(); }
    // Every goblin must appear on its wave's side of the castle gates.
    if (!bEndless)
        for (TActorIterator<AArenaFighter> It(GetWorld()); It; ++It)
            if (It->bEnemy && It->IsAlive() && It->EncounterSite.IsValid() && !It->bBossEncounter
                && SectionOf(static_cast<float>(It->GetActorLocation().X)) != SectionOf(static_cast<float>(It->EncounterSite->GetActorLocation().X)))
            {
                UE_LOG(LogTemp, Error, TEXT("NATURAL WAVES CHECK FAILED: a goblin of %s is at %s, beyond its section"), *It->EncounterSite->SiteName, *It->GetActorLocation().ToCompactString());
                Finish(false); return;
            }
    for (TActorIterator<AArenaFighter> It(GetWorld()); It; ++It)
        // The Queen keeps her distance and throws claws, so she is fought wherever she stands.
        if (It->bEnemy && It->IsAlive() && FVector::Dist2D(It->GetActorLocation(), Hero->GetActorLocation()) < (It->bBossEncounter ? 3000.f : 450.f))
            It->ApplyPhysicsDamage(1000000.f, FVector::ZeroVector);
    int32 BossHealth = 0;
    for (TActorIterator<AArenaFighter> It(GetWorld()); It; ++It) if (It->bEnemy && It->bBossEncounter) BossHealth += FMath::RoundToInt(It->Health);
    const int32 Progress = ClearedSites * 100 + Portals * 10 + EndlessWave + BossHealth * 10000 + EnemiesRemaining * 1000000;
    if (Progress != LastProgress) { LastProgress = Progress; SinceProgress = 0.f; }
    if (SinceProgress > 45.f || Clock > 900.f)
    {
        UE_LOG(LogTemp, Error, TEXT("NATURAL WAVES CHECK FAILED: level %d%s stalled at wave %d (%d sites cleared, %d portals)"),
            CampaignLevel, bEndless ? TEXT(" endless") : TEXT(""), bEndless ? EndlessWave : ScriptStep, ClearedSites, Portals);
        for (auto Site : SpawnSites)
            if (Site->bActivated && !Site->bCleared)
            {
                UE_LOG(LogTemp, Error, TEXT("  %s at %s: spawned %d / %d, %d alive"), *Site->SiteName, *Site->GetActorLocation().ToCompactString(), Site->GetSpawned(), Site->WaveTotal(), Site->LivingEnemies());
                for (TActorIterator<AArenaFighter> It(GetWorld()); It; ++It)
                    if (It->bEnemy && It->IsAlive() && It->EncounterSite == Site)
                        UE_LOG(LogTemp, Error, TEXT("    alive at %s"), *It->GetActorLocation().ToCompactString());
            }
        Finish(false);
    }
#endif
}

// The last few goblins of a wave can end up where they cannot reach Hellgirl (knocked onto a wall top, stuck on a
// ledge, fallen through the floor). After 12 s without a kill, stragglers far from her are brought back near her,
// so a wave (and the portal after it) can always be finished.
void AArenaGameMode::RescueStragglers(AArenaFighter* Hero, float Dt)
{
    if (EnemiesRemaining != LastLivingEnemies) { LastLivingEnemies = EnemiesRemaining; StragglerClock = 0.f; }
    StragglerClock += Dt;
    const bool Stalled = EnemiesRemaining > 0 && EnemiesRemaining <= 4 && StragglerClock > 12.f;
    FCollisionObjectQueryParams Solid; Solid.AddObjectTypesToQuery(ECC_WorldStatic); Solid.AddObjectTypesToQuery(ECC_WorldDynamic);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(StragglerRescue), false, Hero);
    const FVector Near = Hero->GetActorLocation();
    for (TActorIterator<AArenaFighter> It(GetWorld()); It; ++It)
    {
        AArenaFighter* Enemy = *It;
        if (!Enemy->bEnemy || !Enemy->IsAlive() || Enemy->bBossEncounter || !Enemy->EncounterSite.IsValid() || !SpawnSites.Contains(Enemy->EncounterSite.Get())) continue;
        const bool Fell = Enemy->GetActorLocation().Z < Near.Z - 1500.f;
        if (!Fell && !(Stalled && FVector::Dist2D(Enemy->GetActorLocation(), Near) > 900.f)) continue;
        // A spot on open ground 600-800 units from her, with nothing solid in between.
        for (int32 Try = 0; Try < 12; ++Try)
        {
            const float Angle = Try * 2.39996f + GetWorld()->GetTimeSeconds();
            const FVector P = Near + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * (600.f + 20.f * Try);
            FHitResult Floor, Wall;
            if (!GetWorld()->LineTraceSingleByObjectType(Floor, P + FVector(0, 0, 800), P - FVector(0, 0, 1200), Solid, Query) || Floor.ImpactNormal.Z < .7f) continue;
            if (GetWorld()->LineTraceSingleByObjectType(Wall, Near + FVector(0, 0, 40), Floor.ImpactPoint + FVector(0, 0, 90), Solid, Query)) continue;
            Enemy->SetActorLocation(Floor.ImpactPoint + FVector(0, 0, 110), false, nullptr, ETeleportType::TeleportPhysics);
            Enemy->ResetAfterRecovery();
            Enemy->HomePosition = Floor.ImpactPoint;
            UE_LOG(LogTemp, Display, TEXT("Straggler %s brought back to Hellgirl"), *Enemy->GetName());
            break;
        }
    }
    if (Stalled) StragglerClock = 0.f;
}
