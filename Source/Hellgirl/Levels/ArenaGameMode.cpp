#include "Levels/ArenaGameMode.h"
#include "Progress/HellgirlWallet.h"
#include "Fighter/ArenaFighter.h"
#include "UI/ArenaHUD.h"
#include "UI/HellgirlPlayerController.h"
#include "Progress/CoinPickup.h"
#include "Rules/CoinDropRules.h"
#include "Enemies/EnemySpawnPoint.h"
#include "Levels/StageOneTerrain.h"
#include "Levels/CastleTerrainLayout.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "InputCoreTypes.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

AArenaGameMode::AArenaGameMode()
{
    PrimaryActorTick.bCanEverTick = true;
    DefaultPawnClass = AArenaFighter::StaticClass();
    HUDClass = AArenaHUD::StaticClass();
    PlayerControllerClass = AHellgirlPlayerController::StaticClass();
}
void AArenaGameMode::BeginPlay()
{
    Super::BeginPlay();
    MapNumber = FMath::Clamp(UGameplayStatics::GetIntOption(OptionsString, TEXT("StageMap"), 1), 1, 3);
    bLegacyMap = MapNumber > 1;
    CampaignLevel = FMath::Clamp(UGameplayStatics::GetIntOption(OptionsString,TEXT("CampaignLevel"),1),1,4);
    bForestHub = UGameplayStatics::GetIntOption(OptionsString,TEXT("ForestHub"),0)==1
        || (GetUnlockedLevel()>=2 && !UGameplayStatics::HasOption(OptionsString,TEXT("StageMap")) && !UGameplayStatics::HasOption(OptionsString,TEXT("CampaignLevel")));
    bStoryEnabled=!bForestHub && !bLegacyMap && (CampaignLevel==1 || CampaignLevel==3)
        && (!FString(FCommandLine::Get()).Contains(TEXT("-Hellgirl")) || FParse::Param(FCommandLine::Get(),TEXT("HellgirlStoryCheck")));
    if (bForestHub) BuildForestHub(); else BuildArena();
    LastSafePosition = bForestHub ? FVector(-550.f,0.f,110.f) : ((CampaignLevel==2 && !bLegacyMap) || IsImpArena() ? FVector(0.f,0.f,115.f) : FVector(-5000.f,0.f,115.f));
    if (APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0))
    {
        Player->SetActorLocation(LastSafePosition, false, nullptr, ETeleportType::TeleportPhysics);
        if (APlayerController* PC = Cast<APlayerController>(Player->GetController()))
            PC->SetControlRotation(FRotator(-20.f, 0.f, 0.f));
    }
    const bool ExplicitDestination=UGameplayStatics::HasOption(OptionsString,TEXT("StageMap"))
        || UGameplayStatics::HasOption(OptionsString,TEXT("CampaignLevel")) || UGameplayStatics::HasOption(OptionsString,TEXT("ForestHub"));
    bShowStartupMenu=!ExplicitDestination && (!FString(FCommandLine::Get()).Contains(TEXT("-Hellgirl")) || FParse::Param(FCommandLine::Get(),TEXT("HellgirlMainMenuCheck")));
}
AStaticMeshActor* AArenaGameMode::Prop(FVector Position, FVector Scale, FLinearColor Color, bool Sphere)
{
    AStaticMeshActor* Actor = GetWorld()->SpawnActor<AStaticMeshActor>(Position, FRotator::ZeroRotator);
    if (!Actor) return nullptr;
    Actor->SetMobility(EComponentMobility::Movable);
    auto* Mesh = Actor->GetStaticMeshComponent();
    Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, Sphere ? TEXT("/Engine/BasicShapes/Sphere.Sphere") : TEXT("/Engine/BasicShapes/Cube.Cube")));
    Mesh->SetCollisionProfileName(TEXT("BlockAll"));
    Actor->SetActorScale3D(Scale);
    auto* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Environment/Materials/M_EnvironmentSurface.M_EnvironmentSurface"));
    if (!Base) Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    if (auto* Mat = UMaterialInstanceDynamic::Create(Base, Actor))
    {
        Mat->SetVectorParameterValue(TEXT("Color"), Color);
        Mat->SetScalarParameterValue(TEXT("Roughness"), .92f);
        Mesh->SetMaterial(0, Mat);
    }
    return Actor;
}
void AArenaGameMode::Platform(FVector Center, FVector Size, FLinearColor Color)
{
    if (MapNumber > 1) RockPlatform(Center, Size, Color);
    else Prop(Center - FVector(0, 0, Size.Z * .5f), Size / 100.f, Color);
    MapPlatforms.Add(FVector4(Center.X, Center.Y, Size.X, Size.Y));
}
void AArenaGameMode::Bridge(FVector From, FVector To, bool Gaps)
{
    const float Distance = FVector::Dist2D(From, To);
    const int32 Steps = FMath::CeilToInt(Distance / (Gaps ? 550.f : 350.f));
    for (int32 I = 1; I < Steps; ++I)
    {
        const FVector P = FMath::Lerp(From, To, static_cast<float>(I) / Steps);
        // Slightly recess bridge caps where they overlap the large islands,
        // avoiding coplanar flicker while retaining the full landing footprint.
        Platform(P - FVector(0,0,3.f), FVector(Gaps ? 390.f : 500.f, Gaps ? 390.f : 500.f, 250.f), FLinearColor(.19f, .16f, .23f));
    }
}
AEnemySpawnPoint* AArenaGameMode::Site(FVector P, FString Name, int32 Count, int32 Flyers, bool Enabled, bool Boss)
{
    auto* S = GetWorld()->SpawnActor<AEnemySpawnPoint>(P, FRotator::ZeroRotator);
    if (!S) return nullptr;
    S->SiteName = Name; S->EnemyCount = Count; S->FlyingCount = Flyers;
    S->Difficulty = MapNumber; S->bEnabled = Enabled; S->bBoss = Boss;
    S->GroundType = MapNumber == 1 ? (Boss ? EHellgirlEnemyType::ImpCommander : EHellgirlEnemyType::Imps)
        : MapNumber == 2 ? (Boss ? EHellgirlEnemyType::GulpBoss : EHellgirlEnemyType::Gulps)
        : (Boss ? EHellgirlEnemyType::SuccubusBoss : EHellgirlEnemyType::Succubus);
    S->FlyingType = MapNumber == 1 ? EHellgirlEnemyType::FlyingImps : S->GroundType;
    if (!bLegacyMap)
    {
        S->Difficulty = CampaignLevel<=3 ? 1 : 2;
        S->GroundType = CampaignLevel <= 3 ? (Boss ? EHellgirlEnemyType::GoblinQueen : EHellgirlEnemyType::Goblins)
            : (Boss ? EHellgirlEnemyType::ImpCommander : EHellgirlEnemyType::Imps);
        if (CampaignLevel <= 3) S->FlyingCount = 0;
    }
    S->ActivationRadius = 1250.f;
    S->bInstantGroup = MapNumber == 2;
    if (MapNumber == 2 && !Boss) S->bActivated = true;
    SpawnSites.Add(S);
    return S;
}
void AArenaGameMode::BuildArena()
{
    const FLinearColor Stone(.24f, .23f, .20f), Ash(.20f, .12f, .10f), Space(.22f, .12f, .30f);
    if (IsImpArena()) BuildImpArena();
    else if (MapNumber == 1)
    {
        MapTitle = CampaignLevel<=3 ? FString::Printf(TEXT("STAGE 1 / LEVEL %d / GOBLINS"),CampaignLevel) : TEXT("STAGE 2 / LEVEL 1 / IMPS");
        GetWorld()->SpawnActor<AStageOneTerrain>();
        MapPlatforms.Add(FVector4(0.f, 0.f, 12500.f, 9500.f));
        for (float X : {-3400.f, 3300.f})
        {
            // Individual courses and broken heights turn the two walls into
            // readable ruins while keeping their original open gateways.
            for (int32 Sign : {-1, 1})
                for (int32 I = 0; I < 7; ++I)
                {
                    const float Y = Sign * (970.f + I * 560.f);
                    const int32 Courses = 2 + ((I * 7 + Sign + (X > 0 ? 2 : 0)) % 4 + 4) % 4;
                    const float BaseZ = CastleTerrain::Height(X, Y) - 25.f;
                    for (int32 Row = 0; Row < Courses; ++Row)
                    {
                        auto* Block = Prop(FVector(X + (Row % 2 ? 12.f : -10.f), Y, BaseZ + 95.f + Row * 185.f),
                            FVector(2.55f, 5.4f, 1.8f), Stone * (1.f + .065f * ((I + Row) % 3)));
                        if (Block && Row == Courses - 1) Block->SetActorRotation(FRotator(0.f, (I % 3 - 1) * 4.f, (I % 2 ? 3.f : -2.f)));
                    }
                }
        }
        if (CampaignLevel==2)
        {
            for (int32 I=0;I<5;++I)
                Site(FVector(I%2 ? 1100 : -1100, I%2 ? 800 : -800,10),FString::Printf(TEXT("SURVIVAL / WAVE %d"),I+1),4+I*2,0,false);
        }
        else
        {
            Site(FVector(-4500,0,10),TEXT("SECTION 1 / WAVE 1"),4,0,false);
            if (CampaignLevel==4) Site(FVector(-4400,100,10),TEXT("SECTION 1 / WAVE 2"),6,1,false);
            Site(FVector(-1400,0,10),TEXT("SECTION 2 / WAVE 1"),5,1,false);
            if (CampaignLevel==4) Site(FVector(0,-500,10),TEXT("SECTION 2 / WAVE 2"),7,2,false);
            Site(FVector(1000,0,10),TEXT("SECTION 2 / FINAL WAVE"),8,3,false);
            if (CampaignLevel==1)
            {
                Site(FVector(4100,-500,10),TEXT("SECTION 3 / WAVE 1"),10,0,false);
                Site(FVector(4500,500,10),TEXT("SECTION 3 / WAVE 2"),12,0,false);
            }
            else Site(FVector(4100,0,10),CampaignLevel==3?TEXT("SECTION 3 / GOBLIN QUEEN"):TEXT("SECTION 3 / IMP COMMANDER"),1,0,false,true);
        }
        for (float X : {-3400.f, 3300.f})
        {
            SectionGates.Add(Prop(FVector(X,0,500), FVector(2.6f,14.f,10.f), FLinearColor(.32f,.06f,.42f)));
            auto* Barrier = Prop(FVector(X,0,5000), FVector(2.6f,300.f,100.f), Stone);
            Barrier->SetActorHiddenInGame(true);
            SectionBarriers.Add(Barrier);
        }
        ExitPosition = CampaignLevel==2 ? FVector(1800,0,0) : FVector(4800, 0, 0);
        // Keep the open-air castle graybox bounded.
        for (int32 Sign : {-1, 1})
        {
            Prop(FVector(Sign * 6250, 0, 500), FVector(1, 96, 15), Stone);
            Prop(FVector(0, Sign * 4750, 500), FVector(126, 1, 15), Stone);
        }
    }
    else if (MapNumber == 2)
    {
        MapTitle = TEXT("STAGE 01 / LAVA ISLANDS");
        // Keep the visible lava below a standing character's damage threshold.
        auto* Lava = Prop(FVector(0, 0, -200), FVector(145, 115, .4f), FLinearColor(1.f, .055f, .005f));
        if (Lava)
        {
            auto* GlowBase = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Environment/Materials/M_EnvironmentGlow.M_EnvironmentGlow"));
            if (auto* Glow = UMaterialInstanceDynamic::Create(GlowBase, Lava))
            {
                Glow->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.f,.16f,.008f));
                Glow->SetScalarParameterValue(TEXT("EmissiveStrength"), 2.f);
                Glow->SetScalarParameterValue(TEXT("TextureScale"), .0003f);
                Glow->SetScalarParameterValue(TEXT("PatternStrength"), 1.f);
                Lava->GetStaticMeshComponent()->SetMaterial(0, Glow);
            }
        }
        const FVector P[] = {FVector(-5000,0,0), FVector(-2400,-1900,0), FVector(-2400,1900,0), FVector(800,-1900,0), FVector(800,1900,0), FVector(4200,0,0)};
        for (auto C : P) Platform(C, FVector(2100,2100,500), Ash);
        Bridge(P[0], P[1], true); Bridge(P[0], P[2], true);
        Bridge(P[1], P[3], true); Bridge(P[2], P[4], true);
        Bridge(P[3], P[4], true); Bridge(P[3], P[5], true); Bridge(P[4], P[5], true);
        for (int32 I = 1; I <= 4; ++I) Site(P[I] + FVector(0,0,10), FString::Printf(TEXT("GULPS / ISLAND %d"), I), 4, 0, true);
        Site(P[5] + FVector(0,0,10), TEXT("GULP BOSS"), 1, 0, false, true);
        ExitPosition = FVector(4700,0,0);
    }
    else
    {
        MapTitle = TEXT("STAGE 01 / ASTRAL PATHS");
        const FVector P[] = {FVector(-5000,0,0), FVector(-2900,-2000,0), FVector(-500,1600,0), FVector(1700,-1100,0), FVector(4300,1700,0)};
        for (auto C : P) Platform(C, FVector(1900,1900,450), Space);
        for (int32 I = 1; I < 5; ++I) Bridge(P[I-1], P[I], I == 2 || I == 4);
        Site(P[1] + FVector(0,0,10), TEXT("SUCCUBUS 1"), 1, 1, true);
        Site(P[3] + FVector(0,0,10), TEXT("SUCCUBUS 2"), 1, 1, false);
        Site(P[4] + FVector(0,0,10), TEXT("SUCCUBUS QUEEN"), 1, 1, false, true);
        ExitPosition = FVector(4700,1700,0);
        FRandomStream Stars(301);
        for (int32 I = 0; I < 90; ++I)
            Prop(FVector(Stars.FRandRange(-8500,8500), Stars.FRandRange(-8500,8500), Stars.FRandRange(2000,6500)), FVector(.12f), FLinearColor(.8f,.6f,1.f), true)->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }
    ExitPortal = Prop(ExitPosition + FVector(0,0,200), FVector(.4f,2.8f,4.f), FLinearColor(.55f,.025f,.9f), true);
    ExitPortal->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    ExitPortal->SetActorHiddenInGame(true);
    TotalSites = SpawnSites.Num();
    if (MapNumber == 2 && !IsImpArena()) GetWorld()->SpawnActor<ASkyAtmosphere>();
    if (auto* Sun = GetWorld()->SpawnActor<ADirectionalLight>(FVector(0,0,4000), FRotator(-50,-35,0)))
    {
        auto* Light = CastChecked<UDirectionalLightComponent>(Sun->GetLightComponent());
        Light->SetMobility(EComponentMobility::Movable);
        Light->bAtmosphereSunLight = MapNumber == 2 && !IsImpArena();
        Light->SetIntensity(IsImpArena() ? 2.4f : MapNumber == 1 ? 1.8f : MapNumber == 3 ? 2.8f : 4.f);
        Light->SetLightColor(IsImpArena() ? FLinearColor(1.f,.23f,.12f) : MapNumber == 3 ? FLinearColor(.7f,.65f,1.f) : MapNumber == 2 ? FLinearColor(1.f,.66f,.44f) : FLinearColor(.48f,.62f,1.f));
    }
    if (auto* Sky = GetWorld()->SpawnActor<ASkyLight>())
    {
        Sky->GetLightComponent()->SetMobility(EComponentMobility::Movable);
        Sky->GetLightComponent()->SetIntensity(IsImpArena() ? .3f : MapNumber == 1 ? .35f : .85f);
        Sky->GetLightComponent()->SetRealTimeCaptureEnabled(!IsImpArena());
    }
    // The astral void has no sky atmosphere for the skylight to capture.
    // A soft opposing fill keeps the player and rock undersides readable.
    if (MapNumber == 1 || MapNumber == 3)
        if (auto* Fill = GetWorld()->SpawnActor<ADirectionalLight>(FVector(0,0,3000), FRotator(-30,145,0)))
        {
            auto* Light = CastChecked<UDirectionalLightComponent>(Fill->GetLightComponent());
            Light->SetMobility(EComponentMobility::Movable);
            Light->SetIntensity(IsImpArena() ? .8f : MapNumber == 1 ? .55f : 1.3f);
            Light->SetLightColor(IsImpArena() ? FLinearColor(.9f,.19f,.09f) : FLinearColor(.55f,.65f,1.f));
            Light->SetCastShadows(false);
            Light->bAtmosphereSunLight = false;
        }
    BuildEnvironmentDetails();
}
void AArenaGameMode::Travel(int32 Map)
{
    FString Options = FString::Printf(TEXT("StageMap=%d"), Map);
    if (Map > MapNumber)
        if (const auto* Player = Cast<AArenaFighter>(UGameplayStatics::GetPlayerPawn(this, 0)); Player && Player->IsAlive())
            Options += FString::Printf(TEXT("?CombatEnergy=%.3f"), FMath::Clamp(Player->Energy, 0.f, Player->MaxEnergy));
    UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this)), true, Options);
}
void AArenaGameMode::RestartMap() { if (bForestHub) { TravelToHub(); return; } if (bLegacyMap) Travel(MapNumber); else TravelToCampaign(CampaignLevel); }
int32 AArenaGameMode::GetUnlockedLevel() const
{
    if (const auto* Wallet=Cast<UHellgirlWallet>(GetGameInstance()); Wallet && Wallet->PendingLoad) return Wallet->PendingLoad->Unlocked;
    if (FParse::Param(FCommandLine::Get(),TEXT("HellgirlHubCheck")) || FParse::Param(FCommandLine::Get(),TEXT("HellgirlHubPreview"))
        || FParse::Param(FCommandLine::Get(),TEXT("HellgirlLevelSelectPreview"))) return 4;
    if (FParse::Param(FCommandLine::Get(),TEXT("HellgirlLevelSelectStagePreview"))) return 2;
    if (FParse::Param(FCommandLine::Get(),TEXT("HellgirlDialogueCheck"))) return 2;
    int32 Unlocked=1; GConfig->GetInt(TEXT("HellgirlCampaign"),TEXT("UnlockedLevel"),Unlocked,GGameUserSettingsIni);
    int32 Format=0; GConfig->GetInt(TEXT("HellgirlCampaign"),TEXT("ProgressVersion"),Format,GGameUserSettingsIni);
    if (Format<2) { Unlocked=Unlocked>=2 ? Unlocked+2 : 1; GConfig->SetInt(TEXT("HellgirlCampaign"),TEXT("UnlockedLevel"),Unlocked,GGameUserSettingsIni); GConfig->SetInt(TEXT("HellgirlCampaign"),TEXT("ProgressVersion"),2,GGameUserSettingsIni); GConfig->Flush(false,GGameUserSettingsIni); }
    return FMath::Clamp(Unlocked,1,5);
}
void AArenaGameMode::TravelToCampaign(int32 Level)
{
    if (Level < 1 || Level > 4 || (bForestHub && Level>GetUnlockedLevel())) return;
    FString Options=FString::Printf(TEXT("StageMap=1?CampaignLevel=%d"),Level);
    if (!bForestHub && Level > CampaignLevel)
        if (auto* Player=Cast<AArenaFighter>(UGameplayStatics::GetPlayerPawn(this,0)))
            Options+=FString::Printf(TEXT("?CombatEnergy=%.3f"),Player->Energy);
    UGameplayStatics::OpenLevel(this,FName(*UGameplayStatics::GetCurrentLevelName(this)),true,Options);
}
void AArenaGameMode::AnswerPrompt(bool Yes)
{
    if (!PromptAction || bWon) return;
    const int32 Action = PromptAction;
    PromptAction = 0;
    Prompt.Empty();
    if (!Yes) { bDeclined = true; return; }
    if (Action == 1) { SpawnSites.Last()->bEnabled = true; SpawnSites.Last()->bActivated = true; }
    else if (Action == 2) { if (bLegacyMap) Travel(MapNumber + 1); else TravelToCampaign(CampaignLevel + 1); }
    else if (Action == 4) TravelToHub();
    else bWon = true;
}
void AArenaGameMode::EnemyDefeated(const FVector& Location)
{
    const int32 Coins = HellgirlCoins::DropAmount(FMath::RandRange(0,99), FMath::RandRange(0,2));
    FHitResult Hit;
    FCollisionObjectQueryParams Types; Types.AddObjectTypesToQuery(ECC_WorldStatic); Types.AddObjectTypesToQuery(ECC_WorldDynamic);
    FVector Drop = Location;
    if (GetWorld()->LineTraceSingleByObjectType(Hit, Location + FVector(0,0,150), FVector(Location.X,Location.Y,-1000), Types))
        Drop = Hit.ImpactPoint + FVector(0,0,30);
    if (auto* Pickup = GetWorld()->SpawnActor<ACoinPickup>(Drop, FRotator::ZeroRotator)) Pickup->SetAmount(Coins);
    ++Kills;
}
void AArenaGameMode::Tick(float Dt)
{
    Super::Tick(Dt);
    // Wait until pawn startup has finished setting its game input mode.
    if (bShowStartupMenu)
    {
        if (auto* PC=Cast<AHellgirlPlayerController>(UGameplayStatics::GetPlayerController(this,0)))
        { PC->ShowMainMenu(); bShowStartupMenu=!PC->IsPauseMenuOpen(); }
        return;
    }
    if (auto* Wallet=Cast<UHellgirlWallet>(GetGameInstance()); Wallet && Wallet->PendingLoad) Wallet->RestorePending();
    if (auto* Wallet=Cast<UHellgirlWallet>(GetGameInstance()); Wallet && Wallet->RunSaveCheck()) return;
    if (bStoryEnabled && TickStory(Dt)) return;
    RunForestHubCheck(Dt);
    if (bForestHub) { TickForestHub(Dt); return; }
    RunGoblinStageCheck();
    if (!bLegacyMap && CampaignLevel<=2) { TickGoblinPrelude(Dt); return; }
    RunCampaignCheck(Dt);
    if (IsImpArena()) { RunImpArenaCheck(); TickImpArena(Dt); return; }
    RunMapVisualCheck(Dt);
    RunTerrainCheck(Dt);
    auto* Player = Cast<AArenaFighter>(UGameplayStatics::GetPlayerPawn(this,0));
    if (!Player || !Player->IsAlive() || SpawnSites.IsEmpty()) return;
    auto* PC = Cast<APlayerController>(Player->GetController());
    ActivatedSites = ClearedSites = EnemiesRemaining = 0;
    for (auto S : SpawnSites)
    {
        ActivatedSites += S->bActivated ? 1 : 0;
        ClearedSites += S->bCleared ? 1 : 0;
        EnemiesRemaining += S->LivingEnemies();
    }
    const int32 BossIndex = SpawnSites.Num()-1;
    bool GroupsDone = true;
    for (int32 I = 0; I < BossIndex; ++I) GroupsDone &= SpawnSites[I]->bCleared;
    if (MapNumber == 3)
        for (int32 I = 1; I < BossIndex; ++I)
            if (SpawnSites[I-1]->bCleared) { SpawnSites[I]->bEnabled = true; SpawnSites[I]->bActivated = true; }
    if (GroupsDone && MapNumber != 1)
    { SpawnSites[BossIndex]->bEnabled = true; SpawnSites[BossIndex]->bActivated = true; }
    if (MapNumber == 1)
    {
        const int32 FirstSectionWaves=CampaignLevel==3 ? 1 : 2;
        const bool OpenFirst = SpawnSites[0]->bCleared && (FirstSectionWaves==1 || SpawnSites[1]->bCleared);
        for (int32 I = 0; I < 2; ++I)
        {
            const bool Open = I == 0 ? OpenFirst : GroupsDone;
            SectionGates[I]->SetActorHiddenInGame(Open);
            SectionGates[I]->SetActorEnableCollision(!Open);
            SectionBarriers[I]->SetActorEnableCollision(!Open);
        }
        int32 Next = 0;
        while (Next < SpawnSites.Num() && SpawnSites[Next]->bCleared) ++Next;
        if (Next != ArenaWaveIndex) { ArenaWaveIndex = Next; WaveCountdown = Next == 0 ? 1.f : 3.f; }
        if (Next < SpawnSites.Num())
        {
            const float X = Player->GetActorLocation().X;
            const bool Entered = Next < FirstSectionWaves || (Next < BossIndex ? X > -3200.f : X > 3500.f);
            if (Entered && !SpawnSites[Next]->bActivated)
            {
                WaveCountdown -= Dt;
                if (WaveCountdown <= 0.f) { SpawnSites[Next]->bEnabled = true; SpawnSites[Next]->bActivated = true; }
            }
        }
    }
    const bool BossDone = SpawnSites.Last()->bCleared;
    if (BossDone && !bLegacyMap && GetUnlockedLevel() < CampaignLevel+1 && !FParse::Param(FCommandLine::Get(),TEXT("HellgirlCampaignCheck")) && !FParse::Param(FCommandLine::Get(),TEXT("HellgirlStoryCheck")) && !FParse::Param(FCommandLine::Get(),TEXT("HellgirlGoblinStageCheck")))
    {
        GConfig->SetInt(TEXT("HellgirlCampaign"),TEXT("UnlockedLevel"),CampaignLevel+1,GGameUserSettingsIni);
        GConfig->Flush(false,GGameUserSettingsIni);
    }
    ExitPortal->SetActorHiddenInGame(!BossDone);
    if (BossOrb) BossOrb->SetActorHiddenInGame(!GroupsDone || SpawnSites.Last()->bActivated);
    Objective = BossDone ? TEXT("Enter the purple portal") : GroupsDone ? TEXT("Defeat the boss") : MapNumber == 2 ? TEXT("Clear all four gulp islands in any order") : TEXT("Follow the path and defeat each encounter");
    if (MapNumber == 1 && CampaignLevel == 4 && GroupsDone && !BossDone && SpawnSites.Last()->bActivated)
    {
        bool CommanderAlive = false;
        for (TActorIterator<AArenaFighter> It(GetWorld()); It; ++It)
            CommanderAlive |= It->bEnemy && It->IsAlive() && It->EnemyType == EHellgirlEnemyType::ImpCommander;
        if (!CommanderAlive && SpawnSites.Last()->LivingEnemies() > 0)
            Objective = TEXT("Defeat the Commander's remaining Imps to open the portal");
    }
    if (MapNumber == 1 && !BossDone)
    {
        const int32 FirstSectionWaves=CampaignLevel==3 ? 1 : 2;
        if (ArenaWaveIndex < BossIndex)
        {
            const int32 Section = ArenaWaveIndex < FirstSectionWaves ? 1 : 2;
            const int32 Wave = ArenaWaveIndex < FirstSectionWaves ? ArenaWaveIndex + 1 : ArenaWaveIndex - FirstSectionWaves + 1;
            Objective = ArenaWaveIndex == FirstSectionWaves && Player->GetActorLocation().X <= -3200.f
                ? TEXT("Gate opened! Enter section 2")
                : FString::Printf(TEXT("SECTION %d / WAVE %d OF %d - %s"), Section, Wave, Section == 1 ? FirstSectionWaves : BossIndex-FirstSectionWaves,
                    SpawnSites[ArenaWaveIndex]->bActivated ? TEXT("Defeat all enemies") : TEXT("Incoming wave"));
        }
        else if (!SpawnSites.Last()->bActivated)
            Objective = Player->GetActorLocation().X <= 3500.f ? TEXT("Gate opened! Enter section 3 for the boss") : (CampaignLevel == 3 ? TEXT("SECTION 3 / Goblin Queen incoming") : TEXT("SECTION 3 / Imp Commander incoming"));
    }
    Prompt.Empty();
    PromptAction = 0;
    const bool NearOrb = false; // Castle boss now starts upon entering section 3.
    const bool NearExit = BossDone && FVector::Dist2D(Player->GetActorLocation(), ExitPosition) < 350;
    if (!NearOrb && !NearExit) bDeclined = false;
    if ((NearOrb || NearExit) && !bDeclined && !bWon)
    {
        PromptAction = NearOrb ? 1 : (bLegacyMap ? MapNumber < 3 : CampaignLevel < 2) ? 2 : 3;
        Prompt = bLegacyMap ? TEXT("Complete legacy map?") : CampaignLevel == 1 ? TEXT("GOBLIN QUEEN DEFEATED / Proceed to Level 2: Imps?") : TEXT("LEVEL 2 COMPLETE / Level 3 unlocked (coming later). Finish?");
        Prompt += TEXT("  E / D-pad Up: YES    N / D-pad Down: NO");
        if (!bLegacyMap && !FParse::Param(FCommandLine::Get(),TEXT("HellgirlCampaignCheck")) && !FParse::Param(FCommandLine::Get(),TEXT("HellgirlStoryCheck")) && !FParse::Param(FCommandLine::Get(),TEXT("HellgirlGoblinStageCheck")))
        {
            PromptAction=4;
            Prompt=TEXT("LEVEL COMPLETE / Return to the forest camp?  YES: Y/Up   NO: N/Down");
        }
        if (PC && (PC->WasInputKeyJustPressed(EKeys::N) || PC->WasInputKeyJustPressed(EKeys::Gamepad_DPad_Down))) AnswerPrompt(false);
        else if (PC && (PC->WasInputKeyJustPressed(EKeys::E) || PC->WasInputKeyJustPressed(EKeys::Gamepad_DPad_Up)))
        {
            AnswerPrompt(true);
            if (NearExit && (bLegacyMap ? MapNumber < 3 : CampaignLevel < 2)) return;
        }
    }
    // Record a grounded landing as a recovery point; lava and void cannot become checkpoints.
    if (Player->GetCharacterMovement()->IsMovingOnGround() && Player->GetActorLocation().Z > 60)
        LastSafePosition = Player->GetActorLocation() + FVector(0,0,10);
    HazardClock -= Dt;
    const float Z = Player->GetActorLocation().Z;
    if (MapNumber > 1 && Z < (MapNumber == 2 ? -30.f : -120.f) && HazardClock <= 0)
    {
        // Environmental damage bypasses block and dodge; lava deals repeated high damage.
        Player->ApplyPhysicsDamage(MapNumber == 2 ? 25.f : 20.f, Player->GetVelocity());
        HazardClock = .5f;
        if (MapNumber == 3 || Z < -800)
        {
            Player->SetActorLocation(LastSafePosition, false, nullptr, ETeleportType::TeleportPhysics);
            Player->ResetAfterRecovery();
        }
        Player->MoveLabel = MapNumber == 2 ? TEXT("LAVA! JUMP BACK TO THE ROCKS") : TEXT("VOID FALL / RETURNED TO LAST LANDING");
    }
    for (TActorIterator<AArenaFighter> It(GetWorld()); It; ++It)
        if (It->bEnemy && It->IsAlive() && It->GetActorLocation().Z < (MapNumber == 2 ? -30.f : -120.f))
        {
            It->SetActorLocation(It->HomePosition + FVector(0,0,115), false, nullptr, ETeleportType::TeleportPhysics);
            It->ResetAfterRecovery();
        }
    RunMapCheck(Dt);
}

