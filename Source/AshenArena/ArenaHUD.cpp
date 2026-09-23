#include "ArenaHUD.h"
#include "ArenaFighter.h"
#include "ArenaGameMode.h"
#include "CombatEnergyRules.h"
#include "HellgirlWallet.h"
#include "StageOneLayout.h"
#include "EnemySpawnPoint.h"
#include "HAL/PlatformTime.h"
#include "Engine/Canvas.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"

void AArenaHUD::DrawHUD()
{
    Super::DrawHUD();
    if (!Canvas) return;
    const AArenaFighter* Player = Cast<AArenaFighter>(UGameplayStatics::GetPlayerPawn(this, 0));
    const AArenaGameMode* GM = Cast<AArenaGameMode>(UGameplayStatics::GetGameMode(this));
    if (!Player || !GM) return;
    if (GM->bForestHub)
    {
        DrawRect(FLinearColor(.015f,.025f,.025f,.8f),20,20,330,76);
        DrawText(TEXT("FOREST CAMP"),FLinearColor(.95f,.79f,.48f),36,32,nullptr,1.5f);
        if (auto* Wallet=Cast<UHellgirlWallet>(GetWorld()->GetGameInstance()))
            DrawText(FString::Printf(TEXT("%lld coins"),Wallet->Coins),FLinearColor::White,36,67);
        const float Width=FMath::Min(700.f,Canvas->ClipX-40.f),X=(Canvas->ClipX-Width)*.5f,Y=Canvas->ClipY-110.f;
        DrawRect(FLinearColor(.01f,.02f,.025f,.85f),X,Y,Width,88.f);
        DrawText(GM->Prompt.IsEmpty()?TEXT("Campfire: outfits   /   Goblin: shop   /   Forest road: levels"):GM->Prompt,FLinearColor(1.f,.85f,.55f),X+18,Y+18);
        DrawText(TEXT("E / Y: interact      Esc / Start: pause"),FLinearColor::White,X+18,Y+50);
        return;
    }
    const float MapX = FMath::Max(20.f, Canvas->ClipX - 210.f), MapY = 155.f, MapSize = 180.f;
    DrawRect(FLinearColor(.015f, .02f, .015f, .88f), MapX - 10.f, MapY - 25.f, MapSize + 20.f, MapSize + 55.f);
    DrawText(FString::Printf(TEXT("N / STAGE %d / LEVEL %d"), GM->CampaignLevel<=3?1:2, GM->CampaignLevel<=3?GM->CampaignLevel:1), FLinearColor::White, MapX, MapY - 20.f);
    auto OnMap = [&](float X, float Y)
    {
        return FVector2D(MapX + (Y + StageOne::HalfExtent) / (2.f * StageOne::HalfExtent) * MapSize,
            MapY + (StageOne::HalfExtent - X) / (2.f * StageOne::HalfExtent) * MapSize);
    };
    for (const FVector4& Platform : GM->MapPlatforms)
    {
        const FVector2D P = OnMap(Platform.X, Platform.Y);
        const float Width = Platform.W / (2.f * StageOne::HalfExtent) * MapSize;
        const float Height = Platform.Z / (2.f * StageOne::HalfExtent) * MapSize;
        DrawRect(FLinearColor(.25f,.23f,.28f), P.X-Width*.5f, P.Y-Height*.5f, Width, Height);
    }
    for (const TObjectPtr<AEnemySpawnPoint>& Site : GM->GetSpawnSites())
    {
        if (!IsValid(Site) || !Site->bEnabled) continue;
        const FVector2D P = OnMap(static_cast<float>(Site->GetActorLocation().X), static_cast<float>(Site->GetActorLocation().Y));
        DrawRect(Site->bCleared ? FLinearColor::Green : (Site->bActivated ? FLinearColor::Red : FLinearColor(1.f, .6f, .1f)), P.X - 4.f, P.Y - 4.f, 8.f, 8.f);
    }
    const FVector2D PlayerDot = OnMap(static_cast<float>(Player->GetActorLocation().X), static_cast<float>(Player->GetActorLocation().Y));
    const FVector2D ExitDot = OnMap(GM->ExitPosition.X, GM->ExitPosition.Y);
    DrawRect(FLinearColor(.7f,.1f,1.f), ExitDot.X-3.f, ExitDot.Y-3.f, 6.f, 6.f);
    DrawRect(FLinearColor(.2f, 1.f, 1.f), PlayerDot.X - 3.f, PlayerDot.Y - 3.f, 6.f, 6.f);
    DrawText(TEXT("YOU: cyan   RIFTS: orange"), FLinearColor::White, MapX, MapY + MapSize + 8.f);
    if (const UHellgirlWallet* Wallet = Cast<UHellgirlWallet>(GetWorld()->GetGameInstance()))
    {
        const float WalletX = FMath::Max(20.f, Canvas->ClipX - 275.f);
        DrawRect(FLinearColor(.035f, .025f, .015f, .92f), WalletX, 20.f, 255.f, 100.f);
        DrawText(TEXT("WALLET"), FLinearColor(1.f, .8f, .2f), WalletX + 15.f, 30.f);
        DrawText(FString::Printf(TEXT("%lld coins"), static_cast<long long>(Wallet->Coins)), FLinearColor(1.f, .8f, .2f), WalletX + 15.f, 51.f, nullptr, 1.6f);
        FString Status = TEXT("Coins drift to you; touch to collect");
        if (Wallet->bLoadFailed) Status = TEXT("Wallet save could not be loaded");
        else if (Wallet->bSaveFailed) Status = TEXT("Save failed - balance kept in memory");
        else if (FPlatformTime::Seconds() - Wallet->PickupTime < 2.5)
            Status = FString::Printf(TEXT("+%d coins collected"), Wallet->LastPickup);
        DrawText(Status, FLinearColor::White, WalletX + 15.f, 91.f);
    }
    const FLinearColor MeterBackground(.13f, .13f, .15f);
    const FLinearColor EnergyColor(1.f, .72f, .3f);
    const FLinearColor UnavailableColor(.55f, .58f, .63f);
    const float EnergyCapacity = FMath::Max(0.f, Player->MaxEnergy);
    const float EnergyAmount = FMath::Clamp(Player->Energy, 0.f, EnergyCapacity);
    constexpr float PanelBottom = 242.f;
    DrawRect(FLinearColor(0.015f, 0.02f, 0.035f, 0.9f), 20.f, 20.f, 510.f, PanelBottom - 20.f);
    DrawText(GM->MapTitle, FLinearColor(0.95f, 0.7f, 0.3f), 36.f, 30.f, nullptr, 1.5f);
    DrawText(FString::Printf(TEXT("Rifts cleared %d / %d    Enemies %d    Kills %d"), GM->ClearedSites, GM->TotalSites, GM->EnemiesRemaining, GM->Kills), FLinearColor::White, 36.f, 65.f);
    DrawText(TEXT("DODGE STAMINA"), FLinearColor::White, 36.f, 92.f);
    DrawRect(MeterBackground, 200.f, 96.f, 300.f, 10.f);
    DrawRect(FLinearColor(.1f, .8f, .7f), 200.f, 96.f, 300.f * FMath::Clamp(Player->Stamina / 100.f, 0.f, 1.f), 10.f);
    const FString UltimateStatus = Player->GetUltimateTime() > 0.f ? FString::Printf(TEXT("ULTIMATE ACTIVE / %.1fs"), Player->GetUltimateTime())
        : !Player->HasUltimate() ? TEXT("OUTFIT ULTIMATE / COMING LATER")
        : EnergyAmount >= EnergyCapacity ? TEXT("ULTIMATE READY / SPECIAL") : TEXT("ULTIMATE / FILL ALL FOUR TUBES");
    DrawText(UltimateStatus, FLinearColor(.8f,.65f,1.f),36.f,122.f);
    DrawText(Player->GetWeapon() == 1 ? TEXT("SWORD EQUIPPED") : TEXT("FISTS EQUIPPED"), FLinearColor::White,36.f,144.f);
    DrawText(TEXT("Weapons: 1 Fists / 2 Sword / D-pad"), FLinearColor::White,36.f,166.f);
    if (Player->GetWeaponMenuTime() > 0.f)
    {
        const float WX = Canvas->ClipX * .5f - 160.f, WY = Canvas->ClipY * .3f;
        DrawRect(FLinearColor(.03f,.015f,.045f,.9f),WX,WY,320.f,100.f);
        DrawText(TEXT("UP / 1: FISTS    RIGHT / 2: SWORD"),EnergyColor,WX+12.f,WY+15.f);
        DrawText(TEXT("DOWN / 3: GUN - unavailable"),UnavailableColor,WX+12.f,WY+42.f);
        DrawText(TEXT("LEFT / 4: CAR - unavailable"),UnavailableColor,WX+12.f,WY+68.f);
    }
    DrawText(Player->MoveLabel, FLinearColor(.4f, .95f, .85f), 36.f, 192.f);
    DrawText(TEXT("Light x4 / Heavy x4 / Hold Heavy to charge"), FLinearColor::White, 36.f, 216.f);

    // Anchor the two main resources to the bottom edge, clear of Hellgirl and
    // the control hints. They stay in place as the window height changes.
    const float VitalsX = 20.f;
    const float VitalsY = Canvas->ClipY - 146.f;
    const float VitalsWidth = FMath::Min(390.f, Canvas->ClipX - 40.f);
    const float BarX = VitalsX + 16.f, BarWidth = FMath::Max(1.f, VitalsWidth - 32.f);
    DrawRect(FLinearColor(.015f, .02f, .035f, .88f), VitalsX, VitalsY, VitalsWidth, 126.f);
    DrawText(FString::Printf(TEXT("HP %d / %d"), FMath::CeilToInt(FMath::Max(0.f, Player->Health)), FMath::RoundToInt(Player->MaxHealth)), FLinearColor(1.f, .75f, .7f), BarX, VitalsY + 12.f);
    DrawRect(MeterBackground, BarX, VitalsY + 32.f, BarWidth, 16.f);
    DrawRect(FLinearColor(.85f, .15f, .12f), BarX, VitalsY + 32.f, BarWidth * FMath::Clamp(Player->Health / FMath::Max(Player->MaxHealth, 1.f), 0.f, 1.f), 16.f);
    DrawText(FString::Printf(TEXT("ENERGY %d / %d"), FMath::FloorToInt(EnergyAmount), FMath::RoundToInt(EnergyCapacity)), EnergyColor, BarX, VitalsY + 58.f);
    constexpr float TubeGap = 6.f;
    const float TubeWidth = (BarWidth - 3.f * TubeGap) / 4.f;
    for (int32 Tube = 0; Tube < 4; ++Tube)
    {
        const float X = BarX + Tube * (TubeWidth + TubeGap);
        const float Fill = FMath::Clamp(EnergyAmount / FMath::Max(EnergyCapacity / 4.f, 1.f) - Tube, 0.f, 1.f);
        DrawRect(MeterBackground,X,VitalsY+78.f,TubeWidth,14.f);
        DrawRect(EnergyColor,X,VitalsY+78.f,TubeWidth*Fill,14.f);
        DrawRect(FLinearColor(.2f,.15f,.1f),X+TubeWidth*.5f,VitalsY+78.f,1.f,14.f);
    }
    auto DrawEnergyCost = [&](const TCHAR* Label, float Cost, float X)
    {
        const bool Ready = EnergyAmount >= Cost;
        const FString Status = Ready ? TEXT("READY") : FString::Printf(TEXT("NEED %d"), FMath::CeilToInt(Cost - EnergyAmount));
        DrawText(FString::Printf(TEXT("%s 1/2 TUBE / %s"), Label, *Status),
            Ready ? EnergyColor : UnavailableColor, X, VitalsY + 104.f);
    };
    DrawEnergyCost(TEXT("CHARGE"), HellgirlEnergy::ChargeCost, BarX);
    DrawEnergyCost(TEXT("SLAM"), HellgirlEnergy::SlamCost, BarX + 195.f);
    const bool ControlsBesideMeters = Canvas->ClipX >= 1000.f;
    const float ControlsX = ControlsBesideMeters ? FMath::Max(VitalsX + VitalsWidth + 20.f, Canvas->ClipX - 710.f) : 20.f;
    const float ControlsY = Canvas->ClipY - (ControlsBesideMeters ? 95.f : 230.f);
    DrawRect(FLinearColor(.015f, .02f, .035f, .85f), ControlsX, ControlsY, Canvas->ClipX - ControlsX - 20.f, 75.f);
    DrawText(TEXT("WASD Move | Mouse Look | LMB Normal | RMB Heavy | Shift Dodge | Space Jump"), FLinearColor::White, ControlsX + 16.f, ControlsY + 13.f);
    DrawText(TEXT("Xbox: sticks Move/Look | X Normal | Y Heavy | B Dodge | A Jump"), FLinearColor::White, ControlsX + 16.f, ControlsY + 35.f);
    DrawText(TEXT("Q / right stick: Ultimate | F / left stick: Sprint | Esc / Start: Pause"), FLinearColor::White, ControlsX + 16.f, ControlsY + 57.f);
    FString Message;
    DrawText(GM->Objective, FLinearColor(1.f,.85f,.45f), 36.f, PanelBottom + 15.f);
    if (!GM->Prompt.IsEmpty()) DrawText(GM->Prompt, FLinearColor::White, 36.f, PanelBottom + 39.f);
    if (GM->MapNumber == 1)
        for (TActorIterator<AArenaFighter> It(GetWorld()); It; ++It)
            if (It->bEnemy && (It->EnemyType == EHellgirlEnemyType::ImpCommander || It->EnemyType == EHellgirlEnemyType::GoblinQueen) && It->bBossEncounter && It->IsAlive())
            {
                const float X = 20.f, Y = 300.f, Width = FMath::Min(510.f,Canvas->ClipX-40.f);
                DrawRect(FLinearColor(.04f,.015f,.025f,.88f),X,Y,Width,78.f);
                DrawText(FString::Printf(TEXT("%s%s"),It->EnemyType == EHellgirlEnemyType::GoblinQueen ? TEXT("GOBLIN QUEEN") : TEXT("IMP COMMANDER"),It->IsBossAttackArmored() ? TEXT(" / ARMORED") : TEXT("")),
                    FLinearColor(1.f,.65f,.3f),X+16.f,Y+8.f);
                DrawRect(MeterBackground,X+16.f,Y+30.f,Width-32.f,10.f);
                DrawRect(FLinearColor(.8f,.15f,.12f),X+16.f,Y+30.f,(Width-32.f)*FMath::Clamp(It->Health/FMath::Max(1.f,It->MaxHealth),0.f,1.f),10.f);
                const FString Hint = It->EnemyType == EHellgirlEnemyType::GoblinQueen
                    ? (It->IsQueenHidden() ? TEXT("DEFEAT THE GOBLINS TO BRING HER BACK") : FString::Printf(TEXT("SHADOW PHASE %d"),It->GetGoblinPhase()+1))
                    : FString::Printf(TEXT("PROTECTIVE IMPS: %d / 3 / %s"),It->GetSummonedImpCount(),It->GetSummonedImpCount()>0 ? TEXT("90% DAMAGE REDUCTION") : TEXT("VULNERABLE"));
                DrawText(Hint,FLinearColor(1.f,.85f,.65f),X+16.f,Y+50.f);
                break;
            }
    if (!Player->IsAlive()) Message = TEXT("YOU FELL  -  Press R to try again");
    else if (GM->bWon) Message = TEXT("LEVEL 2 COMPLETE! MORE LEVELS COMING");
    if (!Message.IsEmpty())
    {
        float Width, Height;
        GetTextSize(Message, Width, Height, nullptr, 1.6f);
        DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.8f), (Canvas->ClipX - Width) * 0.5f - 20.f, Canvas->ClipY * 0.4f - 10.f, Width + 40.f, Height + 20.f);
        DrawText(Message, FLinearColor::White, (Canvas->ClipX - Width) * 0.5f, Canvas->ClipY * 0.4f, nullptr, 1.6f);
    }
}

