#include "Levels/ArenaGameMode.h"
#include "UI/HellgirlPlayerController.h"
#include "Progress/HellgirlWallet.h"
#include "Kismet/GameplayStatics.h"
#include "Framework/Application/SlateApplication.h"
#include "Containers/Ticker.h"
#include "Input/Events.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

// -HellgirlMenuFocusCheck (at camp): a gamepad player must never be left without a focused button. The blue portal's
// menu opens; moving down focuses its stock button, and stocking the Souls disables it: focus must move to another usable button.
// Then focus is dropped on the game view (as happens when a menu page changes), and it must come back to a button.
// Needs a real window (Slate only refreshes a button's enabled state when it draws), so it is not in run-checks.ps1:
//   UnrealEditor.exe Hellgirl.uproject /Engine/Maps/Entry?ForestHub=1 -game -windowed -HellgirlMenuFocusCheck
void AArenaGameMode::RunMenuFocusCheck(float Dt)
{
#if WITH_DEV_AUTOMATION_TESTS
    if (!FParse::Param(FCommandLine::Get(), TEXT("HellgirlMenuFocusCheck"))) return;
    static bool Started = false;
    static float Clock = 0.f;
    Clock += Dt;
    auto* PC = Cast<AHellgirlPlayerController>(UGameplayStatics::GetPlayerController(this, 0));
    auto* Wallet = Cast<UHellgirlWallet>(GetGameInstance());
    if (Started || Clock < 1.f || !PC || !Wallet) return;
    Started = true;
    // The blue portal's menu with Souls to stock: its second button (stock) disables once they are stocked.
    Wallet->LevelSouls.Souls = 40;
    if (FParse::Param(FCommandLine::Get(), TEXT("MenuFocusPause"))) PC->TogglePauseMenu(); else PC->OpenPortalMenu(false);
    // The game is paused while the menu is open, so the steps run on the core ticker.
    struct FState { int32 Step = 0; float Time = 0.f; TSharedPtr<SWidget> First, Opening; };
    TSharedRef<FState> State = MakeShared<FState>();
    TWeakObjectPtr<AHellgirlPlayerController> WeakPC(PC);
    TWeakObjectPtr<UHellgirlWallet> WeakWallet(Wallet);
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([State, WeakPC, WeakWallet](float Delta)
    {
        auto Finish = [](bool Passed, const TCHAR* Why)
        {
            if (Passed) { UE_LOG(LogTemp, Display, TEXT("MENU FOCUS CHECK PASSED: %s"), Why); }
            else { UE_LOG(LogTemp, Error, TEXT("MENU FOCUS CHECK FAILED: %s"), Why); }
            FPlatformMisc::RequestExitWithStatus(false, Passed ? 0 : 1);
            return false;
        };
        State->Time += Delta;
        if (!WeakPC.IsValid() || !WeakWallet.IsValid()) return Finish(false, TEXT("controller or wallet gone"));
        auto FocusedButton = []() -> TSharedPtr<SWidget>
        {
            const TSharedPtr<SWidget> W = FSlateApplication::Get().GetUserFocusedWidget(0);
            return W.IsValid() && W->GetType() == FName(TEXT("SButton")) && W->IsEnabled() ? W : nullptr;
        };
        if (State->Time > 12.f) return Finish(false, TEXT("timed out"));
        switch (State->Step)
        {
        case 0:
            if (State->Time < .5f) return true;
            if (!WeakPC->IsPauseMenuOpen()) return Finish(false, TEXT("the portal menu did not open"));
            if (!FocusedButton().IsValid()) return Finish(false, TEXT("the menu opened without a focused button"));
            State->Opening = FocusedButton();
            // Down to the stock button, as a gamepad would.
            FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(EKeys::Down, FModifierKeysState(), 0, false, 0, 0));
            FSlateApplication::Get().ProcessKeyUpEvent(FKeyEvent(EKeys::Down, FModifierKeysState(), 0, false, 0, 0));
            State->Step = 3; State->Time = 0.f;
            return true;
        case 3:
            if (State->Time < .3f) return true;
            State->First = FocusedButton();
            if (!State->First.IsValid()) return Finish(false, TEXT("no button focused after moving down"));
            if (State->First == State->Opening) return Finish(false, TEXT("moving down did not move the focus"));
            // Stock the Souls: the focused stock button disables.
            if (!WeakWallet->StockSouls(false)) return Finish(false, TEXT("could not stock the Souls"));
            State->Step = 1; State->Time = 0.f;
            return true;
        case 1:
            if (State->Time < .5f) return true;
            if (State->First->IsEnabled()) return Finish(false, TEXT("the stock button stayed enabled (focus was not on it)"));
            if (!FocusedButton().IsValid() || FocusedButton() == State->First) return Finish(false, TEXT("focus stayed on the disabled button"));
            FSlateApplication::Get().SetAllUserFocusToGameViewport();
            State->Step = 2; State->Time = 0.f;
            return true;
        default:
            if (State->Time < .5f) return true;
            if (!FocusedButton().IsValid()) return Finish(false, TEXT("focus left on the game view"));
            return Finish(true, TEXT("focus moved off a button that disabled, and back from the game view"));
        }
    }));
#endif
}
