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
        const FLinearColor Ink(.94f,.88f,.77f), Faint(.58f,.54f,.52f), Blood(.9f,.2f,.15f);
        auto Text=[](const FString& Value,int32 Size,FLinearColor Color)
        { return SNew(STextBlock).Text(FText::FromString(Value)).AutoWrapText(true).Justification(ETextJustify::Center).Font(FCoreStyle::GetDefaultFontStyle("Regular",Size)).ColorAndOpacity(Color); };
        auto Choice=[this,Ink](TSharedPtr<SButton>& Out,const TCHAR* Label,bool bCamp)
        {
            return SAssignNew(Out,SButton).HAlign(HAlign_Center).ContentPadding(FMargin(18,12))
                .ButtonColorAndOpacity(FLinearColor(.12f,.05f,.06f,.95f))
                .OnClicked_Lambda([this,bCamp]() { Choose(bCamp); return FReply::Handled(); })
                [SNew(STextBlock).Text(FText::FromString(Label)).Font(FCoreStyle::GetDefaultFontStyle("Regular",18)).ColorAndOpacity(Ink)];
        };
        TSharedPtr<SButton> Camp;
        TSharedRef<SVerticalBox> Items=SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,8)[Text(Args._Title,28,Blood)]
            + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,22)[Text(Args._Detail,14,Faint)]
            + SVerticalBox::Slot().AutoHeight().Padding(0,5)[Choice(FirstButton,TEXT("TRY AGAIN"),false)]
            + SVerticalBox::Slot().AutoHeight().Padding(0,5)[Choice(Camp,TEXT("RETURN TO CAMP"),true)];
        ChildSlot.HAlign(HAlign_Center).VAlign(VAlign_Center)
        [SNew(SBox).WidthOverride(460)
          [SNew(SBorder).Padding(32).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(.03f,.015f,.02f,.95f))[Items]]];
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
