#include "UI/HellgirlPlayerController.h"
#include "Levels/ArenaGameMode.h"
#include "Fighter/ArenaFighter.h"
#include "Fighter/HellgirlOutfits.h"
#include "Progress/HellgirlWallet.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/CoreStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Containers/Ticker.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Progress/CampaignProgress.h"

class SLevelSelectMenu : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SLevelSelectMenu) {} SLATE_ARGUMENT(TWeakObjectPtr<AHellgirlPlayerController>,Owner)
        SLATE_ARGUMENT(UTexture2D*,Frame) SLATE_END_ARGS()
    TSharedPtr<SButton> FirstButton;

    TSharedPtr<SButton> OpenFirstWorldForPreview()
    {
        SelectedWorld=1; bStageOpen=true;
        return GoblinFirstButton;
    }

    void Construct(const FArguments& Args)
    {
        Owner=Args._Owner;
        const auto* GM=Owner.IsValid()?Cast<AArenaGameMode>(UGameplayStatics::GetGameMode(Owner.Get())):nullptr;
        Unlocked=GM?GM->GetUnlockedLevel():1;
        Frame.SetResourceObject(Args._Frame);
        Frame.ImageSize=FVector2D(400,640); Frame.DrawAs=ESlateBrushDrawType::Image;
        Worlds=SNew(SHorizontalBox);
        GoblinStages=SNew(SVerticalBox);
        ImpStages=SNew(SVerticalBox);
        CourtStages=SNew(SVerticalBox);
        AddWorld(1,TEXT("WORLD I  /  GOBLIN RUINS"),TEXT("Three stages · Goblin Queen"),true);
        AddWorld(2,TEXT("WORLD II  /  IMP TORTURE ARENA"),TEXT("Imp waves · Imp Commander"),Unlocked>=4);
        if (Unlocked>=4)
            AddWorld(3,TEXT("WORLD III  /  SUCCUBUS COURT"),TEXT("Map preview · no enemies yet"),true);
        AddStage(GoblinStages,1,TEXT("STAGE I  /  FIRST RAID"),TEXT("Survive the first Goblin waves"),Unlocked>=1);
        if (Unlocked>=1) AddStage(GoblinStages,2,TEXT("STAGE II  /  THE GOBLIN ARMY"),TEXT("Seven waves · soul portals"),Unlocked>=2);
        if (Unlocked>=2) AddStage(GoblinStages,3,TEXT("STAGE III  /  THE QUEEN"),TEXT("Fourteen waves across the ruins, then the Goblin Queen"),Unlocked>=3);
        // Unlocked by beating Stage 2 (02.5).
        if (HellgirlProgress::Flag(TEXT("Stage2Won"))) AddStage(GoblinStages,Endless,TEXT("ENDLESS  /  GOBLIN WAVES"),
            *FString::Printf(TEXT("Waves that never stop · best wave %d"),HellgirlProgress::EndlessBest()),true);
        AddStage(GoblinStages,ForestRun,TEXT("FOREST RUN  /  RANDOM ROOMS"),TEXT("Three random clearings, then the Queen · dying ends the run"),true);
        AddStage(ImpStages,4,TEXT("STAGE I  /  TORTURE ARENA"),TEXT("Five Imp waves · Imp Commander"),Unlocked>=4);
        AddStage(ImpStages,5,TEXT("STAGE II  /  COMING LATER"),TEXT("Next stage preview"),false);
        AddStage(CourtStages,CourtPreview,TEXT("THE COURT  /  MAP PREVIEW"),TEXT("Walk the court · no enemies yet"),true);

        const FLinearColor Ivory(.94f,.88f,.77f);
        ChildSlot[SNew(SOverlay)
            + SOverlay::Slot()[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
                .BorderBackgroundColor(FLinearColor(.012f,.008f,.02f,.72f))]
            + SOverlay::Slot().Padding(14)[SNew(SImage).Image(&Frame).ColorAndOpacity(FLinearColor(.92f,.85f,.74f,.78f)).Visibility(EVisibility::HitTestInvisible)]
            + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(44)
              [SNew(SScaleBox).Stretch(EStretch::ScaleToFit).StretchDirection(EStretchDirection::DownOnly)
                [SNew(SBox).WidthOverride(820).HeightOverride(700)
                  [SNew(SBorder).Padding(FMargin(38,30)).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
                    .BorderBackgroundColor(FLinearColor(.025f,.017f,.032f,.92f))
                    [SNew(SVerticalBox)
                      + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,8)
                        [SNew(STextBlock).Text_Lambda([this]() { return FText::FromString(bStageOpen?TEXT("CHOOSE A STAGE"):TEXT("CHOOSE A WORLD")); })
                            .Font(FCoreStyle::GetDefaultFontStyle("Regular",29)).ColorAndOpacity(Ivory).Justification(ETextJustify::Center)]
                      + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,22)
                        [SNew(STextBlock).Text(FText::FromString(TEXT("—  †  —"))).Font(FCoreStyle::GetDefaultFontStyle("Regular",18))
                            .ColorAndOpacity(FLinearColor(.64f,.49f,.35f)).Justification(ETextJustify::Center)]
                      + SVerticalBox::Slot().FillHeight(1)
                        [SNew(SOverlay)
                          + SOverlay::Slot().HAlign(HAlign_Center)[SNew(SBox).Visibility_Lambda([this]() { return bStageOpen?EVisibility::Collapsed:EVisibility::Visible; })[Worlds.ToSharedRef()]]
                          + SOverlay::Slot().HAlign(HAlign_Center)[SNew(SBox).WidthOverride(520).Visibility_Lambda([this]() { return bStageOpen && SelectedWorld==1?EVisibility::Visible:EVisibility::Collapsed; })[GoblinStages.ToSharedRef()]]
                          + SOverlay::Slot().HAlign(HAlign_Center)[SNew(SBox).WidthOverride(520).Visibility_Lambda([this]() { return bStageOpen && SelectedWorld==2?EVisibility::Visible:EVisibility::Collapsed; })[ImpStages.ToSharedRef()]]
                          + SOverlay::Slot().HAlign(HAlign_Center)[SNew(SBox).WidthOverride(520).Visibility_Lambda([this]() { return bStageOpen && SelectedWorld==3?EVisibility::Visible:EVisibility::Collapsed; })[CourtStages.ToSharedRef()]]]
                      + SVerticalBox::Slot().AutoHeight().Padding(0,18,0,6)
                        [SAssignNew(BackButton,SButton).HAlign(HAlign_Center).ContentPadding(FMargin(22,10))
                            .ButtonColorAndOpacity(FLinearColor(.12f,.065f,.085f,.9f))
                            .OnClicked_Lambda([this]() { GoBack(); return FReply::Handled(); })
                            [SNew(STextBlock).Text_Lambda([this]() { return FText::FromString(bStageOpen?TEXT("BACK TO WORLDS"):TEXT("BACK TO CAMP")); })
                                .Font(FCoreStyle::GetDefaultFontStyle("Regular",17)).ColorAndOpacity(Ivory)]]
                      + SVerticalBox::Slot().AutoHeight()
                        [SNew(STextBlock).Text(FText::FromString(TEXT("ESC / B  ·  BACK"))).Font(FCoreStyle::GetDefaultFontStyle("Regular",11))
                            .ColorAndOpacity(FLinearColor(.58f,.52f,.48f)).Justification(ETextJustify::Center)]
                    ]]]]];
    }

    virtual FReply OnPreviewKeyDown(const FGeometry&,const FKeyEvent& Event) override
    {
        // Held-button repeats (e.g. B still held from a dodge) must not close the menu.
        if (!Event.IsRepeat() && (Event.GetKey()==EKeys::Escape || Event.GetKey()==EKeys::Gamepad_FaceButton_Right || Event.GetKey()==EKeys::Gamepad_Special_Right))
        { GoBack(); return FReply::Handled(); }
        return FReply::Unhandled();
    }
private:
    void FocusLater(TSharedPtr<SButton> Button)
    {
        TWeakPtr<SLevelSelectMenu> Weak=StaticCastSharedRef<SLevelSelectMenu>(AsShared());
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Weak,Button](float)
        {
            if (Weak.IsValid() && Button.IsValid())
            {
                FSlateApplication::Get().SetKeyboardFocus(Button,EFocusCause::SetDirectly);
                FSlateApplication::Get().SetAllUserFocus(Button,EFocusCause::SetDirectly);
            }
            return false;
        }),.01f);
    }
    void GoBack()
    {
        if (bStageOpen)
        {
            bStageOpen=false;
            FocusLater(SelectedWorld==2?ImpWorldButton:SelectedWorld==3?CourtWorldButton:FirstButton);
        }
        else if (Owner.IsValid()) Owner->ResumeGame();
    }
    void AddWorld(int32 World,const TCHAR* Title,const TCHAR* Detail,bool Available)
    {
        const FLinearColor Ink=Available?FLinearColor(.94f,.88f,.77f):FLinearColor(.43f,.43f,.45f);
        auto Card=SNew(SVerticalBox)
            + SVerticalBox::Slot().FillHeight(1).VAlign(VAlign_Center)[SNew(STextBlock).Text(FText::FromString(Title))
                .Font(FCoreStyle::GetDefaultFontStyle("Regular",19)).ColorAndOpacity(Ink)
                .AutoWrapText(true).Justification(ETextJustify::Center)]
            + SVerticalBox::Slot().AutoHeight().Padding(0,12,0,0)[SNew(STextBlock)
                .Text(FText::FromString(FString(Detail)+(Available?TEXT(""):TEXT("  /  LOCKED"))))
                .Font(FCoreStyle::GetDefaultFontStyle("Regular",12)).ColorAndOpacity(Ink)
                .AutoWrapText(true).Justification(ETextJustify::Center)];
        if (Available)
        {
            TSharedPtr<SButton> Button;
            Worlds->AddSlot().AutoWidth().Padding(6,0)[SNew(SBox).WidthOverride(225).HeightOverride(196)
                [SAssignNew(Button,SButton).ContentPadding(FMargin(16,18))
                .ButtonColorAndOpacity(FLinearColor(.16f,.08f,.11f,.95f))
                .OnClicked_Lambda([this,World]() {
                    if (World==2 && Unlocked<4) return FReply::Handled();
                    SelectedWorld=World; bStageOpen=true;
                    FocusLater(World==1?GoblinFirstButton:World==2?ImpFirstButton:CourtFirstButton);
                    return FReply::Handled(); })[Card]]];
            if (World==1) FirstButton=Button;
            if (World==2) ImpWorldButton=Button;
            if (World==3) CourtWorldButton=Button;
        }
        else Worlds->AddSlot().AutoWidth().Padding(6,0)[SNew(SBox).WidthOverride(225).HeightOverride(196)
            [SNew(SBorder).Padding(FMargin(16,18))
                .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(.07f,.07f,.08f,.8f))[Card]]];
    }
    void AddStage(TSharedPtr<SVerticalBox> Panel,int32 Level,const TCHAR* Title,const TCHAR* Detail,bool Available)
    {
        const FLinearColor Ink=Available?FLinearColor(.94f,.88f,.77f):FLinearColor(.43f,.43f,.45f);
        auto Card=SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(Title))
                .Font(FCoreStyle::GetDefaultFontStyle("Regular",20)).ColorAndOpacity(Ink)]
            + SVerticalBox::Slot().AutoHeight().Padding(0,5,0,0)[SNew(STextBlock)
                .Text(FText::FromString(FString(Detail)+(Available?TEXT(""):Level==5?TEXT("  /  UNAVAILABLE"):TEXT("  /  LOCKED"))))
                .Font(FCoreStyle::GetDefaultFontStyle("Regular",13)).ColorAndOpacity(Ink)];
        if (Available)
        {
            TSharedPtr<SButton> Button;
            Panel->AddSlot().AutoHeight().Padding(0,4)[SAssignNew(Button,SButton).ContentPadding(FMargin(18,9))
                .ButtonColorAndOpacity(FLinearColor(.16f,.08f,.11f,.95f))
                .OnClicked_Lambda([this,Level]() {
                    if (Owner.IsValid() && Level==CourtPreview)
                    {
                        auto* GM=Cast<AArenaGameMode>(UGameplayStatics::GetGameMode(Owner.Get()));
                        Owner->ResumeGame(); if (GM) GM->TravelToSuccubusCourt();
                    }
                    else if (Owner.IsValid() && Level==ForestRun)
                    {
                        auto* GM=Cast<AArenaGameMode>(UGameplayStatics::GetGameMode(Owner.Get()));
                        Owner->ResumeGame(); if (GM) GM->StartForestRun();
                    }
                    else if (Owner.IsValid() && Level==Endless)
                    {
                        auto* GM=Cast<AArenaGameMode>(UGameplayStatics::GetGameMode(Owner.Get()));
                        Owner->ResumeGame(); if (GM) GM->StartEndless();
                    }
                    else if (Owner.IsValid() && Level<=Unlocked && Level<=4)
                    {
                        auto* GM=Cast<AArenaGameMode>(UGameplayStatics::GetGameMode(Owner.Get()));
                        Owner->ResumeGame(); if (GM) GM->TravelToCampaign(Level);
                    }
                    return FReply::Handled(); })[Card]];
            if (Level==1) GoblinFirstButton=Button;
            if (Level==4) ImpFirstButton=Button;
            if (Level==CourtPreview) CourtFirstButton=Button;
        }
        else Panel->AddSlot().AutoHeight().Padding(0,4)[SNew(SBorder).Padding(FMargin(18,9))
            .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(.07f,.07f,.08f,.8f))[Card]];
    }
    TWeakObjectPtr<AHellgirlPlayerController> Owner;
    FSlateBrush Frame;
    int32 Unlocked=1,SelectedWorld=0;
    bool bStageOpen=false;
    TSharedPtr<SHorizontalBox> Worlds;
    TSharedPtr<SVerticalBox> GoblinStages,ImpStages,CourtStages;
    // World III is a map preview reached by its own option, not a campaign level number.
    static constexpr int32 CourtPreview=100;
    // The forest run is a chain of random rooms, also outside the campaign level numbers.
    static constexpr int32 ForestRun=101;
    // Endless goblin waves in the Stage 2 arena.
    static constexpr int32 Endless=102;
    TSharedPtr<SButton> BackButton,GoblinFirstButton,ImpFirstButton,ImpWorldButton,CourtWorldButton,CourtFirstButton;
};

class SForestHubMenu : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SForestHubMenu) {} SLATE_ARGUMENT(TWeakObjectPtr<AHellgirlPlayerController>,Owner) SLATE_ARGUMENT(int32,Kind) SLATE_END_ARGS()
    TSharedPtr<SButton> FirstButton;
    void Construct(const FArguments& Args)
    {
        Owner=Args._Owner;
        auto Text=[](FString Value,int32 Size=18) { return SNew(STextBlock).Text(FText::FromString(Value)).AutoWrapText(true).Justification(ETextJustify::Center).Font(FCoreStyle::GetDefaultFontStyle("Regular",Size)).ColorAndOpacity(FLinearColor(.92f,.85f,.69f)); };
        TSharedRef<SVerticalBox> Items=SNew(SVerticalBox);
        const FString Title=Args._Kind==0 ? TEXT("CAMPFIRE") : Args._Kind==1 ? TEXT("INTO THE FOREST") : TEXT("GOBLIN'S SHOP");
        Items->AddSlot().AutoHeight().Padding(0,0,0,24)[Text(Title,26)];
        if (Args._Kind==0)
        {
            Items->AddSlot().AutoHeight().Padding(0,0,0,16)[Text(TEXT("Rested / Health restored\nChoose your outfit"),15)];
            for (const int32 Outfit : {0,4})
            {
                TSharedPtr<SButton> Button;
                Items->AddSlot().AutoHeight().Padding(0,6)
                [SAssignNew(Button,SButton).HAlign(HAlign_Center).ContentPadding(14).IsEnabled_Lambda([this,Outfit]() { const auto* Wallet=Owner.IsValid()?Cast<UHellgirlWallet>(Owner->GetGameInstance()):nullptr; return Outfit==0 || (Wallet && Wallet->bGoblinQueenOwned); })
                    .OnClicked_Lambda([this,Outfit]() { if (Owner.IsValid()) Owner->SelectOutfit(Outfit); return FReply::Handled(); })
                    [SNew(STextBlock).Text_Lambda([this,Outfit]() {
                        const auto* Wallet=Owner.IsValid()?Cast<UHellgirlWallet>(Owner->GetGameInstance()):nullptr;
                        return FText::FromString(FString(HellgirlOutfits::Label(Outfit))+(Outfit==4 && (!Wallet || !Wallet->bGoblinQueenOwned) ? TEXT(" / BUY IN SHOP") : Owner.IsValid() && Owner->GetSelectedOutfit()==Outfit ? TEXT(" / EQUIPPED") : TEXT(""))); })]];
                if (!FirstButton) FirstButton=Button;
            }
        }
        else
        {
            Items->AddSlot().AutoHeight().Padding(0,0,0,18)[SNew(STextBlock).Text_Lambda([this]() {
                const auto* Wallet=Owner.IsValid()?Cast<UHellgirlWallet>(Owner->GetGameInstance()):nullptr;
                return FText::FromString(FString::Printf(TEXT("Your souls: %lld"),Wallet?Wallet->Coins:0)); })];
            Items->AddSlot().AutoHeight().Padding(0,6)
            [SAssignNew(FirstButton,SButton).HAlign(HAlign_Center).ContentPadding(14)
                .IsEnabled_Lambda([this]() { const auto* Wallet=Owner.IsValid()?Cast<UHellgirlWallet>(Owner->GetGameInstance()):nullptr; return Wallet && !Wallet->bLoadFailed && !Wallet->bGoblinQueenOwned && Wallet->Coins>=200; })
                .OnClicked_Lambda([this]() { if (Owner.IsValid()) if (auto* Wallet=Cast<UHellgirlWallet>(Owner->GetGameInstance())) Wallet->BuyGoblinQueen(); return FReply::Handled(); })
                [SNew(STextBlock).Text_Lambda([this]() {
                    const auto* Wallet=Owner.IsValid()?Cast<UHellgirlWallet>(Owner->GetGameInstance()):nullptr;
                    return FText::FromString(Wallet && Wallet->bGoblinQueenOwned ? TEXT("GOBLIN QUEEN / OWNED") : TEXT("GOBLIN QUEEN SKIN / 200 SOULS")); })]];
            Items->AddSlot().AutoHeight().Padding(0,12)[Text(TEXT("Equip purchased outfits at the campfire."),14)];
            Items->AddSlot().AutoHeight()[SNew(STextBlock).Text_Lambda([this]() {
                const auto* Wallet=Owner.IsValid()?Cast<UHellgirlWallet>(Owner->GetGameInstance()):nullptr;
                return FText::FromString(Wallet && Wallet->bSaveFailed ? TEXT("Couldn't save purchase. Your souls were not spent. Try again.") : TEXT("")); })];
        }
        TSharedPtr<SButton> Close;
        Items->AddSlot().AutoHeight().Padding(0,24,0,0)
        [SAssignNew(Close,SButton).HAlign(HAlign_Center).ContentPadding(14).OnClicked_Lambda([this]() { if (Owner.IsValid()) Owner->ResumeGame(); return FReply::Handled(); })[Text(TEXT("BACK TO CAMP"),18)]];
        if (!FirstButton) FirstButton=Close;
        ChildSlot.HAlign(HAlign_Center).VAlign(VAlign_Center)
        [SNew(SBox).WidthOverride(470)
          [SNew(SBorder).Padding(32).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(.022f,.035f,.03f,.97f))[Items]]];
    }
    virtual FReply OnPreviewKeyDown(const FGeometry&,const FKeyEvent& Event) override
    {
        // Held-button repeats (e.g. B still held from a dodge) must not close the menu.
        if (!Event.IsRepeat() && (Event.GetKey()==EKeys::Escape || Event.GetKey()==EKeys::Gamepad_FaceButton_Right || Event.GetKey()==EKeys::Gamepad_Special_Right))
        { if (Owner.IsValid()) Owner->ResumeGame(); return FReply::Handled(); }
        return FReply::Unhandled();
    }
private:
    TWeakObjectPtr<AHellgirlPlayerController> Owner;
};
void AHellgirlPlayerController::InteractWithHub()
{
    auto* GM=Cast<AArenaGameMode>(UGameplayStatics::GetGameMode(this));
    if (!GM || !GM->bForestHub || bMenuOpen) return;
    const int32 Kind=GM->GetHubInteraction();
    // 03.5: the first talk with the goblin who followed her unlocks his shop.
    if (Kind==2 && !HellgirlProgress::Flag(TEXT("ShopUnlocked")) && !HellgirlProgress::IsCheckRun() && ShowConversation(TEXT("C_MeetGoblin")))
    {
        HellgirlProgress::SetFlag(TEXT("ShopUnlocked"));
        DialogueNextHubMenu=2;
    }
    else if (Kind==2)
    {
        ShowDialogue(FText::FromString(TEXT("Goblin Merchant")),FText::FromString(TEXT("Take a look. The Goblin Queen outfit is yours for 200 souls.")));
        if (bDialogueOpen) DialogueNextHubMenu=2;
    }
    else if (Kind>=0) OpenHubMenu(Kind);
}
void AHellgirlPlayerController::OpenHubMenu(int32 Kind)
{
    auto* GM=Cast<AArenaGameMode>(UGameplayStatics::GetGameMode(this));
    if (!GM || !GM->bForestHub || Kind<0 || Kind>2 || bMenuOpen || !GetWorld()->GetGameViewport() || !SetPause(true)) return;
    if (auto* Fighter=Cast<AArenaFighter>(GetPawn()))
    {
        Fighter->PrepareForPause();
        if (Kind==0) Fighter->Health=Fighter->MaxHealth;
    }
    FlushPressedKeys(); bMenuOpen=true;
    TSharedPtr<SButton> Focus;
    if (Kind==1)
    {
        EnsureGothicFrame();
        auto Menu=SNew(SLevelSelectMenu).Owner(this).Frame(FrameTexture);
        PauseWidget=Menu; Focus=FParse::Param(FCommandLine::Get(),TEXT("HellgirlLevelSelectStagePreview"))
            ? Menu->OpenFirstWorldForPreview() : Menu->FirstButton;
        GetWorld()->GetGameViewport()->AddViewportWidgetContent(Menu,100);
    }
    else
    {
        auto Menu=SNew(SForestHubMenu).Owner(this).Kind(Kind);
        PauseWidget=Menu; Focus=Menu->FirstButton;
        GetWorld()->GetGameViewport()->AddViewportWidgetContent(Menu,100);
    }
    bShowMouseCursor=true;
    FInputModeUIOnly Mode; Mode.SetWidgetToFocus(Focus); Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock); SetInputMode(Mode);
    TWeakObjectPtr<AHellgirlPlayerController> Weak(this);
    TWeakPtr<SWidget> WeakMenu=PauseWidget;
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Weak,WeakMenu,Focus](float)
    {
        // Only if this menu is still the open one: a menu closed within 0.1 s leaves both pointers empty,
        // and re-applying UI-only input then would lock the player out of the game.
        const TSharedPtr<SWidget> Menu=WeakMenu.Pin();
        if (Weak.IsValid() && Weak->IsPauseMenuOpen() && Menu.IsValid() && Menu==Weak->PauseWidget && Focus.IsValid())
        {
            FInputModeUIOnly Input; Input.SetWidgetToFocus(Focus); Input.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
            Weak->SetInputMode(Input); Weak->bShowMouseCursor=true;
            FSlateApplication::Get().SetKeyboardFocus(Focus,EFocusCause::SetDirectly);
            FSlateApplication::Get().SetAllUserFocus(Focus,EFocusCause::SetDirectly);
        }
        return false;
    }),.1f);
}
