#include "UI/ArenaHUD.h"
#include "Fighter/ArenaFighter.h"
#include "Bosses/BossBehavior.h"
#include "Levels/ArenaGameMode.h"
#include "Rules/CombatEnergyRules.h"
#include "Progress/HellgirlWallet.h"
#include "Rules/StageOneLayout.h"
#include "Enemies/EnemySpawnPoint.h"
#include "HAL/PlatformTime.h"
#include "Engine/Canvas.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"

namespace
{
// Gothic palette: parchment text, old gold, blood red, ember orange, cold teal.
const FLinearColor Parchment(.93f, .87f, .76f), Gold(.95f, .72f, .32f), Blood(.78f, .1f, .09f), Ember(1.f, .62f, .22f),
    Teal(.25f, .85f, .78f), Ash(.5f, .48f, .46f), Shade(.01f, .008f, .012f, .6f), Groove(.07f, .05f, .06f, .85f);
}

void AArenaHUD::DrawHUD()
{
    Super::DrawHUD();
    if (!Canvas) return;
    const AArenaFighter* Player = Cast<AArenaFighter>(UGameplayStatics::GetPlayerPawn(this, 0));
    const AArenaGameMode* GM = Cast<AArenaGameMode>(UGameplayStatics::GetGameMode(this));
    if (!Player || !GM) return;
    const float W = Canvas->ClipX, H = Canvas->ClipY;
    const double Now = GetWorld()->GetRealTimeSeconds();
    const auto* Wallet = Cast<UHellgirlWallet>(GetWorld()->GetGameInstance());

    // Text with a soft drop shadow, so it reads on any background without a box behind it.
    auto Say = [&](const FString& Text, FLinearColor Color, float X, float Y, float Scale = 1.f, float Alpha = 1.f)
    {
        DrawText(Text, FLinearColor(0.f, 0.f, 0.f, .75f * Alpha), X + 1.5f, Y + 1.5f, nullptr, Scale);
        Color.A = Alpha;
        DrawText(Text, Color, X, Y, nullptr, Scale);
    };
    auto Centered = [&](const FString& Text, FLinearColor Color, float Y, float Scale = 1.f, float Alpha = 1.f)
    {
        float TW, TH;
        GetTextSize(Text, TW, TH, nullptr, Scale);
        Say(Text, Color, (W - TW) * .5f, Y, Scale, Alpha);
    };
    // A framed bar: dark groove, fill, thin light edge on top.
    auto Bar = [&](float X, float Y, float BW, float BH, float Fill, FLinearColor Color)
    {
        DrawRect(FLinearColor(0.f, 0.f, 0.f, .7f), X - 2.f, Y - 2.f, BW + 4.f, BH + 4.f);
        DrawRect(Groove, X, Y, BW, BH);
        const float F = FMath::Clamp(Fill, 0.f, 1.f);
        DrawRect(Color, X, Y, BW * F, BH);
        DrawRect(FLinearColor(1.f, 1.f, 1.f, .18f), X, Y, BW * F, FMath::Max(1.f, BH * .25f));
    };
    // A soft dark wash behind a corner, fading out to the right.
    auto Wash = [&](float X, float Y, float WW, float WH)
    {
        for (int32 I = 0; I < 6; ++I)
            DrawRect(FLinearColor(Shade.R, Shade.G, Shade.B, Shade.A * (1.f - I / 6.f)), X + WW * I / 6.f, Y, WW / 6.f, WH);
    };

    // ---- Camp: title, coins, and the prompt of whatever you stand next to. ----
    if (GM->bForestHub)
    {
        Wash(0.f, 16.f, 360.f, 66.f);
        Say(TEXT("FOREST CAMP"), Gold, 28.f, 22.f, 1.5f);
        if (Wallet) Say(FString::Printf(TEXT("%lld coins"), Wallet->Coins), Parchment, 28.f, 54.f);
        const FString Prompt = GM->Prompt.IsEmpty() ? TEXT("Campfire: outfits  ·  Goblin: shop  ·  Forest road: levels") : GM->Prompt;
        Centered(Prompt, GM->Prompt.IsEmpty() ? Ash : Gold, H - 92.f, GM->Prompt.IsEmpty() ? 1.f : 1.25f);
        Centered(TEXT("E / Y  interact     Esc / Start  pause"), Ash, H - 58.f);
        return;
    }

    // ---- Top left: where you are and what to do. ----
    Wash(0.f, 14.f, 520.f, GM->Prompt.IsEmpty() ? 70.f : 94.f);
    Say(GM->MapTitle, Gold, 28.f, 20.f, 1.35f);
    Say(GM->Objective, Parchment, 28.f, 50.f);
    if (!GM->Prompt.IsEmpty()) Say(GM->Prompt, Ember, 28.f, 74.f);

    // ---- Top right: a small minimap, coins underneath. ----
    const float MapSize = 150.f, MapX = W - MapSize - 24.f, MapY = 20.f;
    DrawRect(FLinearColor(0.f, 0.f, 0.f, .55f), MapX - 3.f, MapY - 3.f, MapSize + 6.f, MapSize + 6.f);
    DrawRect(FLinearColor(.03f, .03f, .035f, .6f), MapX, MapY, MapSize, MapSize);
    auto OnMap = [&](float X, float Y)
    {
        return FVector2D(MapX + (Y + StageOne::HalfExtent) / (2.f * StageOne::HalfExtent) * MapSize,
            MapY + (StageOne::HalfExtent - X) / (2.f * StageOne::HalfExtent) * MapSize);
    };
    for (const FVector4& Platform : GM->MapPlatforms)
    {
        const FVector2D P = OnMap(Platform.X, Platform.Y);
        const float PW = Platform.W / (2.f * StageOne::HalfExtent) * MapSize, PH = Platform.Z / (2.f * StageOne::HalfExtent) * MapSize;
        DrawRect(FLinearColor(.2f, .18f, .2f, .8f), P.X - PW * .5f, P.Y - PH * .5f, PW, PH);
    }
    for (const TObjectPtr<AEnemySpawnPoint>& Site : GM->GetSpawnSites())
    {
        if (!IsValid(Site) || !Site->bEnabled || Site->bCleared) continue;
        const FVector2D P = OnMap(static_cast<float>(Site->GetActorLocation().X), static_cast<float>(Site->GetActorLocation().Y));
        const float Pulse = Site->bActivated ? 5.f + 1.5f * FMath::Sin(Now * 6.0) : 4.f;
        DrawRect(Site->bActivated ? Blood : Ember, P.X - Pulse * .5f, P.Y - Pulse * .5f, Pulse, Pulse);
    }
    if (!GM->bSuccubusCourt)
    {
        const FVector2D ExitDot = OnMap(GM->ExitPosition.X, GM->ExitPosition.Y);
        DrawRect(FLinearColor(.7f, .3f, 1.f), ExitDot.X - 3.f, ExitDot.Y - 3.f, 6.f, 6.f);
    }
    const FVector2D PlayerDot = OnMap(static_cast<float>(Player->GetActorLocation().X), static_cast<float>(Player->GetActorLocation().Y));
    DrawRect(FLinearColor::Black, PlayerDot.X - 4.f, PlayerDot.Y - 4.f, 8.f, 8.f);
    DrawRect(Teal, PlayerDot.X - 3.f, PlayerDot.Y - 3.f, 6.f, 6.f);
    if (Wallet)
    {
        FString Coins = FString::Printf(TEXT("%lld coins"), static_cast<long long>(Wallet->Coins));
        if (Wallet->bLoadFailed) Coins = TEXT("wallet not loaded");
        else if (Wallet->bSaveFailed) Coins += TEXT("  (not saved)");
        float TW, TH;
        GetTextSize(Coins, TW, TH);
        Say(Coins, Gold, MapX + MapSize - TW, MapY + MapSize + 8.f);
        if (FPlatformTime::Seconds() - Wallet->PickupTime < 2.5)
        {
            const FString Gain = FString::Printf(TEXT("+%d"), Wallet->LastPickup);
            GetTextSize(Gain, TW, TH);
            Say(Gain, Gold, MapX + MapSize - TW, MapY + MapSize + 28.f, 1.f, 1.f - static_cast<float>((FPlatformTime::Seconds() - Wallet->PickupTime) / 2.5));
        }
    }

    // ---- Top centre: the boss. ----
    for (TActorIterator<AArenaFighter> It(GetWorld()); It; ++It)
        if (const UBossBehavior* Boss = It->GetBossBehavior(); Boss && It->bEnemy && It->bBossEncounter && It->IsAlive())
        {
            const float BW = FMath::Min(620.f, W - 420.f), BX = (W - BW) * .5f, BY = 34.f;
            Centered(Boss->GetDisplayName() + (It->IsBossAttackArmored() ? TEXT("  ·  ARMORED") : TEXT("")), Gold, BY - 22.f, 1.1f);
            Bar(BX, BY + 4.f, BW, 12.f, It->Health / FMath::Max(1.f, It->MaxHealth), Blood);
            if (!Boss->GetHudStatus().IsEmpty()) Centered(Boss->GetHudStatus(), Ash, BY + 22.f);
            break;
        }

    // ---- Bottom left: health, energy tubes, stamina, and what the energy can buy. ----
    const float VX = 28.f, VW = FMath::Min(360.f, W - 56.f), VY = H - 112.f;
    Wash(0.f, VY - 12.f, VW + 140.f, 108.f);
    const float Life = Player->Health / FMath::Max(Player->MaxHealth, 1.f);
    const FLinearColor LifeColor = Life < .3f ? FLinearColor(1.f, .2f, .12f) * (.8f + .2f * FMath::Sin(Now * 9.0)) : Blood;
    Bar(VX, VY, VW, 18.f, Life, LifeColor);
    Say(FString::Printf(TEXT("%d"), FMath::CeilToInt(FMath::Max(0.f, Player->Health))), Parchment, VX + 8.f, VY + 1.f, .9f);
    const float EnergyCapacity = FMath::Max(1.f, Player->MaxEnergy), EnergyAmount = FMath::Clamp(Player->Energy, 0.f, EnergyCapacity);
    constexpr float Gap = 6.f;
    const float TubeW = (VW - 3.f * Gap) / 4.f;
    for (int32 Tube = 0; Tube < 4; ++Tube)
        Bar(VX + Tube * (TubeW + Gap), VY + 28.f, TubeW, 9.f, EnergyAmount / (EnergyCapacity / 4.f) - Tube, Ember);
    Bar(VX, VY + 45.f, VW, 4.f, Player->Stamina / 100.f, Teal);
    auto Ability = [&](const TCHAR* Name, float Cost, float X)
    {
        const bool Ready = EnergyAmount >= Cost;
        Say(Name, Ready ? Ember : Ash, X, VY + 58.f, .85f, Ready ? 1.f : .7f);
    };
    Ability(TEXT("CHARGE"), HellgirlEnergy::ChargeCost, VX);
    Ability(TEXT("SLAM"), HellgirlEnergy::SlamCost, VX + 80.f);
    Say(Player->GetWeapon() == 1 ? TEXT("SWORD") : TEXT("FISTS"), Ash, VX + 140.f, VY + 58.f, .85f);
    if (Player->GetUltimateTime() > 0.f)
        Say(FString::Printf(TEXT("ULTIMATE  %.1f"), Player->GetUltimateTime()), FLinearColor(.85f, .6f, 1.f), VX, VY - 28.f, 1.15f);
    else if (Player->HasUltimate() && EnergyAmount >= EnergyCapacity)
        Say(TEXT("ULTIMATE READY  ·  Q / RIGHT STICK"), FLinearColor(.85f, .6f, 1.f), VX, VY - 28.f, 1.05f, .75f + .25f * FMath::Sin(Now * 5.0));
    if (Player->GetWeaponMenuTime() > 0.f) Centered(TEXT("UP / 1  FISTS          RIGHT / 2  SWORD"), Ember, H * .3f, 1.1f);

    // ---- Centre: move callouts pop up and fade. ----
    if (Player->MoveLabel != ShownLabel) { ShownLabel = Player->MoveLabel; LabelShownAt = Now; }
    const float LabelAge = static_cast<float>(Now - LabelShownAt);
    if (!ShownLabel.IsEmpty() && LabelAge < 1.4f)
    {
        const float Fade = LabelAge < 1.f ? 1.f : 1.f - (LabelAge - 1.f) / .4f;
        const bool Loud = ShownLabel.Contains(TEXT("CRITICAL")) || ShownLabel.Contains(TEXT("THORNS")) || ShownLabel.Contains(TEXT("BLOCKED"));
        Centered(ShownLabel, Loud ? Ember : Parchment, H * .68f - FMath::Min(LabelAge, .3f) * 30.f, Loud ? 1.3f : 1.05f, Fade);
    }

    // ---- Controls: shown at the start of a level, then they fade away. ----
    const float Age = GetWorld()->GetTimeSeconds();
    if (Age < 12.f)
    {
        const float Fade = Age < 10.f ? .85f : .85f * (1.f - (Age - 10.f) / 2.f);
        Centered(TEXT("WASD move  ·  LMB light  ·  RMB heavy (hold to charge)  ·  Shift dodge  ·  Space jump  ·  Q ultimate"), Parchment, H - 58.f, .95f, Fade);
        Centered(TEXT("Pad: X light  ·  Y heavy  ·  B dodge  ·  A jump  ·  Right stick ultimate  ·  Start pause"), Ash, H - 36.f, .9f, Fade);
    }

    // ---- Death and victory. ----
    FString Message;
    if (!Player->IsAlive()) Message = GM->bForestRun ? TEXT("YOU FELL  ·  the run is over  ·  press R") : TEXT("YOU FELL  ·  press R to try again");
    else if (GM->bWon) Message = TEXT("LEVEL COMPLETE");
    if (!Message.IsEmpty())
    {
        DrawRect(FLinearColor(0.f, 0.f, 0.f, .45f), 0.f, H * .38f, W, 70.f);
        Centered(Message, Player->IsAlive() ? Gold : FLinearColor(.9f, .2f, .15f), H * .38f + 18.f, 1.7f);
    }
}
