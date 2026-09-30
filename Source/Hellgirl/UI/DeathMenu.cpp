#include "UI/HellgirlPlayerController.h"
#include "Levels/ArenaGameMode.h"
#include "Fighter/ArenaFighter.h"
#include "Progress/CampaignProgress.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/CoreStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Containers/Ticker.h"
#include "UI/GothicUIKit.h"

// Shown a moment after Hellgirl falls: TRY AGAIN (the same level, or a fresh run for Stage II of the swamp and
// endless) or RETURN TO CAMP. R still tries again straight away.
class SDeathMenu : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SDeathMenu) {} SLATE_ARGUMENT(TWeakObjectPtr<AHellgirlPlayerController>,Owner) SLATE_ARGUMENT(FString,Title) SLATE_ARGUMENT(FString,Detail) SLATE_END_ARGS()
    TSharedPtr<SButton> FirstButton;
    void Construct(const FArguments& Args)
    {
        Owner=Args._Owner;
        // The placeholder art "Death Retry Return to camp.png" (724 x 543): the frame with its two bars, the title above
        // them and the detail below the frame.
        using namespace GothicUI;
        TSharedPtr<SButton> Camp;
        const TArray<FPlace> Places = {
            {FVector2D(196,30), FVector2D(352,44), Label(Args._Title,22,GothicUI::Blood())},
            {FVector2D(228,168), FVector2D(290,66), ArtButton(FirstButton,TEXT("DeathDark"),TEXT("DeathRed"),Label(TEXT("TRY AGAIN"),15,Ink()),[this]() { Choose(false); })},
            {FVector2D(230,243), FVector2D(282,60), ArtButton(Camp,TEXT("DeathDark"),TEXT("DeathRed"),Label(TEXT("RETURN TO CAMP"),15,Ink()),[this]() { Choose(true); })},
            {FVector2D(170,350), FVector2D(384,40), Label(Args._Detail,11,Faint())}};
        ChildSlot[Screen(TEXT("DeathMenu"),FVector2D(724,543),Places)];
    }
    virtual bool SupportsKeyboardFocus() const override { return true; }
    virtual FReply OnPreviewKeyDown(const FGeometry&,const FKeyEvent& Event) override
    {
        if (!Event.IsRepeat() && Event.GetKey()==EKeys::R) { Choose(false); return FReply::Handled(); }
        return FReply::Unhandled();
    }
private:
    void Choose(bool bCamp)
    {
        if (!Owner.IsValid()) return;
        auto* GM=Cast<AArenaGameMode>(UGameplayStatics::GetGameMode(Owner.Get()));
        Owner->ResumeGame();
        if (!GM) return;
        if (bCamp) GM->TravelToHub(); else GM->TryAgain();
    }
    TWeakObjectPtr<AHellgirlPlayerController> Owner;
};

void AHellgirlPlayerController::OpenDeathMenu(const FString& Title,const FString& Detail)
{
    if (bMenuOpen || !GetWorld() || !GetWorld()->GetGameViewport() || !SetPause(true)) return;
    FlushPressedKeys(); bMenuOpen=true;
    auto Menu=SNew(SDeathMenu).Owner(this).Title(Title).Detail(Detail);
    PauseWidget=Menu;
    GetWorld()->GetGameViewport()->AddViewportWidgetContent(Menu,100);
    KeepMenuFocus();
    const TSharedPtr<SButton> Focus=Menu->FirstButton;
    bShowMouseCursor=true;
    FInputModeUIOnly Mode; Mode.SetWidgetToFocus(Focus); Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock); SetInputMode(Mode);
    // Focus again once the menu has been laid out (as the other menus do).
    TWeakObjectPtr<AHellgirlPlayerController> Weak(this);
    TWeakPtr<SWidget> WeakMenu=PauseWidget;
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Weak,WeakMenu,Focus](float)
    {
        const TSharedPtr<SWidget> Open=WeakMenu.Pin();
        if (Weak.IsValid() && Weak->IsPauseMenuOpen() && Open.IsValid() && Open==Weak->PauseWidget && Focus.IsValid())
        {
            FSlateApplication::Get().SetKeyboardFocus(Focus,EFocusCause::SetDirectly);
            FSlateApplication::Get().SetAllUserFocus(Focus,EFocusCause::SetDirectly);
        }
        return false;
    }),.1f);
}
