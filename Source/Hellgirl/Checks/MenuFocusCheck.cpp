#include "Levels/ArenaGameMode.h"
#include "UI/HellgirlPlayerController.h"
#include "Progress/HellgirlWallet.h"
#include "Kismet/GameplayStatics.h"
#include "Framework/Application/SlateApplication.h"
#include "Containers/Ticker.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

// -HellgirlMenuFocusCheck (at camp): a gamepad player must never be left without a focused button. The goblin's shop
// opens with its first item focused; buying it disables that button, and focus must move to another usable button.
// Then focus is dropped on the game view (as happens when a menu page changes), and it must come back to a button.
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
    Wallet->bShopInChecks = true;
    Wallet->ShopLevels.Reset();
    Wallet->Coins = 100000;
    PC->OpenHubMenu(2);
    // The game is paused while the menu is open, so the steps run on the core ticker.
    struct FState { int32 Step = 0; float Time = 0.f; TSharedPtr<SWidget> First; };
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
            if (!WeakPC->IsPauseMenuOpen()) return Finish(false, TEXT("the shop did not open"));
            State->First = FocusedButton();
            if (!State->First.IsValid()) return Finish(false, TEXT("the shop opened without a focused button"));
            // Buy the focused first item (Charge): its button is disabled from now on.
            if (!WeakWallet->BuyShopItem(0)) return Finish(false, TEXT("could not buy the first item"));
            State->Step = 1; State->Time = 0.f;
            return true;
        case 1:
            if (State->Time < .5f) return true;
            if (State->First->IsEnabled()) return Finish(false, TEXT("the bought item's button stayed enabled"));
            if (!FocusedButton().IsValid() || FocusedButton() == State->First) return Finish(false, TEXT("focus stayed on the disabled button"));
            FSlateApplication::Get().SetAllUserFocusToGameViewport();
            State->Step = 2; State->Time = 0.f;
            return true;
        default:
            if (State->Time < .5f) return true;
            if (!FocusedButton().IsValid()) return Finish(false, TEXT("focus left on the game view"));
            return Finish(true, TEXT("focus moved off a bought item's button and back from the game view"));
        }
    }));
#endif
}
