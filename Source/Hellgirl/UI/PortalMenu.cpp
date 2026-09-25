#include "UI/HellgirlPlayerController.h"
#include "Levels/ArenaGameMode.h"
#include "Fighter/ArenaFighter.h"
#include "Progress/HellgirlWallet.h"
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

// The soul portal between waves: continue to the next wave, stock the carried souls (safe from death),
// or buy upgrades (coming soon). In endless mode it can also take Hellgirl back to camp.
// The exit portal only asks whether to leave for camp; NO closes it, and walking back in (or E / Y) reopens it.
class SPortalMenu : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SPortalMenu) {} SLATE_ARGUMENT(TWeakObjectPtr<AHellgirlPlayerController>,Owner) SLATE_ARGUMENT(bool,Exit) SLATE_END_ARGS()
    TSharedPtr<SButton> FirstButton;
    void Construct(const FArguments& Args)
    {
        Owner=Args._Owner;
        const FLinearColor Ink(.94f,.88f,.77f), Faint(.58f,.54f,.52f), Soul(.55f,.82f,1.f);
        auto Text=[](const FString& Value,int32 Size,FLinearColor Color)
        { return SNew(STextBlock).Text(FText::FromString(Value)).AutoWrapText(true).Justification(ETextJustify::Center).Font(FCoreStyle::GetDefaultFontStyle("Regular",Size)).ColorAndOpacity(Color); };
        auto Choice=[this,Ink](TSharedPtr<SButton>& Out,TFunction<FText()> Label,TFunction<bool()> Enabled,AArenaGameMode::EPortalChoice Pick,bool bClose)
        {
            return SAssignNew(Out,SButton).HAlign(HAlign_Center).ContentPadding(FMargin(18,12))
                .ButtonColorAndOpacity(FLinearColor(.1f,.09f,.16f,.95f))
                .IsEnabled_Lambda([Enabled]() { return Enabled(); })
                .OnClicked_Lambda([this,Pick,bClose]()
                {
                    auto* GM=Owner.IsValid()?Cast<AArenaGameMode>(UGameplayStatics::GetGameMode(Owner.Get())):nullptr;
                    if (bClose && Owner.IsValid()) Owner->ResumeGame();
                    if (GM) GM->ChoosePortal(Pick);
                    return FReply::Handled();
                })
                [SNew(STextBlock).Text_Lambda([Label]() { return Label(); }).Font(FCoreStyle::GetDefaultFontStyle("Regular",18)).ColorAndOpacity(Ink)];
        };
        auto Wallet=[this]() { return Owner.IsValid()?Cast<UHellgirlWallet>(Owner->GetGameInstance()):nullptr; };
        const auto* GM=Owner.IsValid()?Cast<AArenaGameMode>(UGameplayStatics::GetGameMode(Owner.Get())):nullptr;
        const bool Endless=GM && GM->bEndless;
        TSharedRef<SVerticalBox> Items=SNew(SVerticalBox);
        TSharedPtr<SButton> Button;
        if (Args._Exit)
        {
            Items->AddSlot().AutoHeight().Padding(0,0,0,10)[Text(TEXT("THE WAY BACK"),26,Ink)];
            Items->AddSlot().AutoHeight().Padding(0,0,0,24)[Text(TEXT("Leave the map and go back to camp?"),16,Faint)];
            Items->AddSlot().AutoHeight().Padding(0,5)[Choice(FirstButton,[]() { return FText::FromString(TEXT("YES")); },[]() { return true; },AArenaGameMode::EPortalChoice::Leave,true)];
            Items->AddSlot().AutoHeight().Padding(0,5)[Choice(Button,[]() { return FText::FromString(TEXT("NO")); },[]() { return true; },AArenaGameMode::EPortalChoice::Stay,true)];
        }
        else
        {
            Items->AddSlot().AutoHeight().Padding(0,0,0,8)[Text(TEXT("SOUL PORTAL"),26,Ink)];
            Items->AddSlot().AutoHeight().Padding(0,0,0,20)[SNew(STextBlock).Justification(ETextJustify::Center).Font(FCoreStyle::GetDefaultFontStyle("Regular",15)).ColorAndOpacity(Soul)
                .Text_Lambda([Wallet]() { const auto* W=Wallet(); return FText::FromString(FString::Printf(TEXT("Carried: %lld souls   ·   Stocked: %lld"),W?W->Carried:0,W?W->Coins:0)); })];
            Items->AddSlot().AutoHeight().Padding(0,5)[Choice(FirstButton,[]() { return FText::FromString(TEXT("CONTINUE  /  NEXT WAVE")); },[]() { return true; },AArenaGameMode::EPortalChoice::Continue,true)];
            Items->AddSlot().AutoHeight().Padding(0,5)[Choice(Button,[Wallet]() {
                    const auto* W=Wallet();
                    return FText::FromString(W && W->Carried>0 ? FString::Printf(TEXT("STOCK SOULS  /  %lld"),W->Carried) : TEXT("SOULS STOCKED")); },
                [Wallet]() { const auto* W=Wallet(); return W && W->Carried>0 && !W->bLoadFailed; },AArenaGameMode::EPortalChoice::Stock,false)];
            Items->AddSlot().AutoHeight().Padding(0,5)[SNew(SButton).HAlign(HAlign_Center).ContentPadding(FMargin(18,12)).IsEnabled(false)
                [SNew(STextBlock).Text(FText::FromString(TEXT("UPGRADES  /  COMING SOON"))).Font(FCoreStyle::GetDefaultFontStyle("Regular",18)).ColorAndOpacity(Faint)]];
            if (Endless)
                Items->AddSlot().AutoHeight().Padding(0,5)[Choice(Button,[]() { return FText::FromString(TEXT("LEAVE FOR CAMP  /  KEEP YOUR SOULS")); },[]() { return true; },AArenaGameMode::EPortalChoice::Leave,true)];
            Items->AddSlot().AutoHeight().Padding(0,18,0,0)[Text(TEXT("Carried souls are lost if you fall. Stocked souls are safe."),12,Faint)];
        }
        Items->AddSlot().AutoHeight().Padding(0,14,0,0)[Text(TEXT("ESC / B  ·  CLOSE"),10,Faint)];
        ChildSlot.HAlign(HAlign_Center).VAlign(VAlign_Center)
        [SNew(SBox).WidthOverride(500)
          [SNew(SBorder).Padding(32).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(.02f,.022f,.045f,.96f))[Items]]];
    }
    virtual FReply OnPreviewKeyDown(const FGeometry&,const FKeyEvent& Event) override
    {
        if (!Event.IsRepeat() && (Event.GetKey()==EKeys::Escape || Event.GetKey()==EKeys::Gamepad_FaceButton_Right || Event.GetKey()==EKeys::Gamepad_Special_Right))
        {
            if (Owner.IsValid()) Owner->ResumeGame();
            return FReply::Handled();
        }
        return FReply::Unhandled();
    }
private:
    TWeakObjectPtr<AHellgirlPlayerController> Owner;
};

void AHellgirlPlayerController::OpenPortalMenu(bool bExit)
{
    if (bMenuOpen || !GetWorld() || !GetWorld()->GetGameViewport() || !SetPause(true)) return;
    if (auto* Fighter=Cast<AArenaFighter>(GetPawn())) Fighter->PrepareForPause();
    FlushPressedKeys(); bMenuOpen=true;
    auto Menu=SNew(SPortalMenu).Owner(this).Exit(bExit);
    PauseWidget=Menu;
    GetWorld()->GetGameViewport()->AddViewportWidgetContent(Menu,100);
    const TSharedPtr<SButton> Focus=Menu->FirstButton;
    bShowMouseCursor=true;
    FInputModeUIOnly Mode; Mode.SetWidgetToFocus(Focus); Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock); SetInputMode(Mode);
    // Focus again once the menu has been laid out (as the camp menus do).
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
