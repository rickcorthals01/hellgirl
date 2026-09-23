#include "UI/HellgirlPlayerController.h"
#include "Fighter/ArenaFighter.h"
#include "Progress/HellgirlWallet.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/CoreStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Containers/Ticker.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Kismet/GameplayStatics.h"
#include "UnrealClient.h"

class SHellgirlMainMenu : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SHellgirlMainMenu) {} SLATE_ARGUMENT(TWeakObjectPtr<AHellgirlPlayerController>,Owner) SLATE_ARGUMENT(UTexture2D*,Frame) SLATE_END_ARGS()
    TSharedPtr<SButton> PlayButton;
    void Construct(const FArguments& Args)
    {
        Owner=Args._Owner;
        Frame.SetResourceObject(Args._Frame); Frame.ImageSize=FVector2D(400,640); Frame.DrawAs=ESlateBrushDrawType::Image;
        const FLinearColor Ivory(.94f,.88f,.79f);
        auto Items=SNew(SVerticalBox);
        Items->AddSlot().AutoHeight().Padding(0,0,0,14)[SNew(STextBlock).Text(FText::FromString(TEXT("H E L L G I R L"))).Font(FCoreStyle::GetDefaultFontStyle("Regular",42)).ColorAndOpacity(Ivory).Justification(ETextJustify::Center)];
        Items->AddSlot().AutoHeight().Padding(0,0,0,50)[SNew(STextBlock).Text(FText::FromString(TEXT("—  †  —"))).Font(FCoreStyle::GetDefaultFontStyle("Regular",20)).ColorAndOpacity(FLinearColor(.56f,.43f,.31f)).Justification(ETextJustify::Center)];
        const TCHAR* Labels[]={TEXT("PLAY"),TEXT("NEW GAME"),TEXT("LOAD GAME"),TEXT("OPTIONS"),TEXT("QUIT")};
        for (int32 I=0;I<5;++I)
        {
            TSharedPtr<SButton> Button;
            Items->AddSlot().AutoHeight().Padding(38,8)
            [SAssignNew(Button,SButton).HAlign(HAlign_Center).ContentPadding(FMargin(24,14)).ButtonColorAndOpacity(FLinearColor(.08f,.055f,.065f,1.f))
                .OnClicked_Lambda([this,I]() {
                    if (Owner.IsValid())
                    {
                        if (I!=1) bConfirmNew=false;
                        if (I==0) Owner->PlayFromMainMenu();
                        else if (I==1)
                        {
                            if (!bConfirmNew) { bConfirmNew=true; Status=TEXT("Reset current progress? Manual saves are kept.\nPress CONFIRM NEW GAME to start over."); }
                            else
                            {
                                auto* Wallet=Cast<UHellgirlWallet>(Owner->GetGameInstance());
                                if (!Wallet || !Wallet->StartNewGame()) { bConfirmNew=false; Status=TEXT("Couldn't save the reset. Your progress is unchanged."); }
                            }
                        }
                        else if (I==2) Owner->OpenSaveMenu();
                        else if (I==3) Owner->OpenMainOptions();
                        else UKismetSystemLibrary::QuitGame(Owner.Get(),Owner.Get(),EQuitPreference::Quit,false);
                    }
                    return FReply::Handled(); })
                [SNew(STextBlock).Text_Lambda([this,I,Label=FString(Labels[I])]() { return FText::FromString(I==1 && bConfirmNew?TEXT("CONFIRM NEW GAME"):Label); }).Font(FCoreStyle::GetDefaultFontStyle("Regular",23)).ColorAndOpacity(Ivory)]];
            if (I==0) PlayButton=Button;
        }
        Items->AddSlot().AutoHeight().Padding(20,12)[SNew(STextBlock).Text_Lambda([this]() { return FText::FromString(Status); }).AutoWrapText(true).Justification(ETextJustify::Center).ColorAndOpacity(Ivory)];
        ChildSlot[SNew(SOverlay)
            + SOverlay::Slot()[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor::Black)]
            + SOverlay::Slot().Padding(18)[SNew(SImage).Image(&Frame).ColorAndOpacity(FLinearColor(.85f,.78f,.68f,.8f)).Visibility(EVisibility::HitTestInvisible)]
            + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(70)
              [SNew(SScaleBox).Stretch(EStretch::ScaleToFit).StretchDirection(EStretchDirection::DownOnly)[SNew(SBox).WidthOverride(480)[Items]]]];
    }
private:
    TWeakObjectPtr<AHellgirlPlayerController> Owner;
    FSlateBrush Frame;
    bool bConfirmNew=false;
    FString Status;
};

void AHellgirlPlayerController::ShowMainMenu()
{
    if (!GetWorld() || !GetWorld()->GetGameViewport()) return;
    ResumeGame(); bMainMenuActive=true;
    if (!SetPause(true)) return;
    EnsureGothicFrame();
    if (auto* Hero=Cast<AArenaFighter>(GetPawn())) Hero->PrepareForPause();
    FlushPressedKeys(); bMenuOpen=true; bShowMouseCursor=true;
    auto Menu=SNew(SHellgirlMainMenu).Owner(this).Frame(FrameTexture); PauseWidget=Menu;
    GetWorld()->GetGameViewport()->AddViewportWidgetContent(Menu,100);
    FInputModeUIOnly Mode; Mode.SetWidgetToFocus(Menu->PlayButton); Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock); SetInputMode(Mode);
    // Focus needs a registered Slate widget path; that is only guaranteed after layout.
    TWeakObjectPtr<AHellgirlPlayerController> Weak(this);
    TWeakPtr<SHellgirlMainMenu> WeakMenu(Menu);
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Weak,WeakMenu](float)
    {
        const auto Screen=WeakMenu.Pin();
        if (Weak.IsValid() && Screen.IsValid() && Weak->PauseWidget==Screen)
        {
            FInputModeUIOnly Input; Input.SetWidgetToFocus(Screen->PlayButton); Input.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
            Weak->SetInputMode(Input); Weak->bShowMouseCursor=true;
            FSlateApplication::Get().SetKeyboardFocus(Screen->PlayButton,EFocusCause::SetDirectly);
            FSlateApplication::Get().SetAllUserFocus(Screen->PlayButton,EFocusCause::SetDirectly);
        }
        return false;
    }),.1f);
}

void AHellgirlPlayerController::PlayFromMainMenu()
{
    bMainMenuActive=false; ResumeGame();
}

void AHellgirlPlayerController::RunMainMenuCheck()
{
#if WITH_DEV_AUTOMATION_TESTS
    if (!FParse::Param(FCommandLine::Get(),TEXT("HellgirlMainMenuCheck"))) return;
    TWeakObjectPtr<AHellgirlPlayerController> Weak(this);
    auto Step=MakeShared<int32>(0);
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Weak,Step](float)
    {
        if (!Weak.IsValid()) return false;
        auto* PC=Weak.Get();
        if (*Step==0)
        {
            if (!PC->IsAtMainMenu() || !PC->IsPauseMenuOpen() || !UGameplayStatics::IsGamePaused(PC))
            { UE_LOG(LogTemp,Error,TEXT("MAIN MENU CHECK FAILED: startup")); FPlatformMisc::RequestExitWithStatus(false,1); return false; }
            FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/MainMenu.png"),true,false);
        }
        else if (*Step==1) PC->OpenMainOptions();
        else if (*Step==2) { PC->ShowMainMenu(); PC->OpenSaveMenu(); }
        else if (*Step==3) PC->ShowMainMenu();
        else if (*Step==4)
        {
            const bool StayedInMenus=PC->IsAtMainMenu() && PC->IsPauseMenuOpen() && UGameplayStatics::IsGamePaused(PC);
            // Exercise Slate focus/activation instead of calling the Play handler directly.
            FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(EKeys::Enter,FModifierKeysState(),0,false,0,0));
            FSlateApplication::Get().ProcessKeyUpEvent(FKeyEvent(EKeys::Enter,FModifierKeysState(),0,false,0,0));
            const bool Passed=StayedInMenus && !PC->IsAtMainMenu() && !PC->IsPauseMenuOpen() && !UGameplayStatics::IsGamePaused(PC);
            if (Passed) { UE_LOG(LogTemp,Display,TEXT("MAIN MENU CHECK PASSED: startup, options, load screen, return and play")); }
            else { UE_LOG(LogTemp,Error,TEXT("MAIN MENU CHECK FAILED: navigation")); }
            FPlatformMisc::RequestExitWithStatus(false,Passed?0:1); return false;
        }
        ++*Step; return true;
    }),1.f);
#endif
}
