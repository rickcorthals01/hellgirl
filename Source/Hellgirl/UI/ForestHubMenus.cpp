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
#include "Progress/Achievements.h"

// The level select at the camp's forest road: the first five worlds (from "Levels, Enemies, Bosses.txt"), shown for
// overview and dev testing even where nothing is playable yet. A world opens its list of stages.
class SLevelSelectMenu : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SLevelSelectMenu) {} SLATE_ARGUMENT(TWeakObjectPtr<AHellgirlPlayerController>,Owner)
        SLATE_ARGUMENT(UTexture2D*,Frame) SLATE_END_ARGS()
    TSharedPtr<SButton> FirstButton;

    TSharedPtr<SButton> OpenFirstWorldForPreview()
    {
        SelectedWorld=1; bStageOpen=true;
        return FirstStageButtons[1];
    }

    void Construct(const FArguments& Args)
    {
        Owner=Args._Owner;
        const auto* GM=Owner.IsValid()?Cast<AArenaGameMode>(UGameplayStatics::GetGameMode(Owner.Get())):nullptr;
        Unlocked=GM?GM->GetUnlockedLevel():1;
        Frame.SetResourceObject(Args._Frame);
        Frame.ImageSize=FVector2D(400,640); Frame.DrawAs=ESlateBrushDrawType::Image;
        WorldRows=SNew(SVerticalBox);
        for (int32 Row=0;Row<2;++Row)
        {
            TSharedPtr<SHorizontalBox> Line=SNew(SHorizontalBox);
            WorldLines.Add(Line);
            WorldRows->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(0,6)[Line.ToSharedRef()];
        }
        StagePanels.SetNum(WorldCount+1); WorldButtons.SetNum(WorldCount+1); FirstStageButtons.SetNum(WorldCount+1);
        for (int32 World=1;World<=WorldCount;++World) StagePanels[World]=SNew(SVerticalBox);

        AddWorld(1,TEXT("WORLD I"),TEXT("GOBLIN RUINS"),TEXT("Goblins · Goblin Queen"));
        AddWorld(2,TEXT("WORLD II"),TEXT("THE SWAMP"),TEXT("Rats & Frogs · Rat Queen & Frog King"));
        AddWorld(3,TEXT("WORLD III"),TEXT("SUCCUBUS COURT"),TEXT("Succubi · Succubus Queen"));
        AddWorld(4,TEXT("WORLD IV"),TEXT("THE LOWER CIRCLES"),TEXT("Ghosts · Ghost King"));
        AddWorld(5,TEXT("WORLD V"),TEXT("IMP TORTURE ARENA"),TEXT("Imps · Imp Commander"));

        // World I: the Goblin campaign and its endless mode.
        AddStage(1,1,TEXT("STAGE I  /  FIRST RAID"),TEXT("Survive the first Goblin waves"),Unlocked>=1);
        if (Unlocked>=1) AddStage(1,2,TEXT("STAGE II  /  THE GOBLIN ARMY"),TEXT("Seven waves · soul portals"),Unlocked>=2);
        if (Unlocked>=2) AddStage(1,3,TEXT("STAGE III  /  THE QUEEN"),TEXT("Fourteen waves across the ruins, then the Goblin Queen"),Unlocked>=3);
        // Unlocked by beating Stage 2 (02.5).
        if (HellgirlProgress::Flag(TEXT("Stage2Won"))) AddStage(1,Endless,TEXT("ENDLESS  /  GOBLIN WAVES"),
            *FString::Printf(TEXT("Waves that never stop · best wave %d"),HellgirlProgress::EndlessBest()),true);
        // The forest run (random rooms, Levels/ForestRun.cpp) is out of the game for now; its code stays for later:
        // AddStage(1,ForestRun,TEXT("FOREST RUN  /  RANDOM ROOMS"),TEXT("Three random clearings, then the Queen · dying ends the run"),true);
        // World II: the swamp (to be designed).
        AddStage(2,ComingLater,TEXT("THE SWAMP  /  COMING LATER"),TEXT("Rats and frogs · the Rat Queen and the Frog King"),false);
        // World III: the court can be walked; its fight comes later.
        AddStage(3,CourtPreview,TEXT("THE COURT  /  MAP PREVIEW"),TEXT("Walk the court · no enemies yet"),true);
        AddStage(3,ComingLater,TEXT("THE SUCCUBUS QUEEN  /  COMING LATER"),TEXT("Succubi · the Succubus Queen"),false);
        // World IV: the lower circles of hell (to be designed).
        AddStage(4,ComingLater,TEXT("THE LOWER CIRCLES  /  COMING LATER"),TEXT("Ghosts · the Ghost King"),false);
        // World V: the Imp arena (campaign level 4).
        AddStage(5,4,TEXT("STAGE I  /  TORTURE ARENA"),TEXT("Five Imp waves · Imp Commander"),Unlocked>=4);
        AddStage(5,ComingLater,TEXT("STAGE II  /  COMING LATER"),TEXT("Next stage preview"),false);
        FirstButton=WorldButtons[1];

        const FLinearColor Ivory(.94f,.88f,.77f);
        TSharedRef<SOverlay> Pages=SNew(SOverlay)
            + SOverlay::Slot().HAlign(HAlign_Center)[SNew(SBox).Visibility_Lambda([this]() { return bStageOpen?EVisibility::Collapsed:EVisibility::Visible; })[WorldRows.ToSharedRef()]];
        for (int32 World=1;World<=WorldCount;++World)
            Pages->AddSlot().HAlign(HAlign_Center)[SNew(SBox).WidthOverride(560).Visibility_Lambda([this,World]() { return bStageOpen && SelectedWorld==World?EVisibility::Visible:EVisibility::Collapsed; })[StagePanels[World].ToSharedRef()]];
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
                      + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,16)
                        [SNew(STextBlock).Text(FText::FromString(TEXT("—  †  —"))).Font(FCoreStyle::GetDefaultFontStyle("Regular",18))
                            .ColorAndOpacity(FLinearColor(.64f,.49f,.35f)).Justification(ETextJustify::Center)]
                      + SVerticalBox::Slot().FillHeight(1)[Pages]
                      + SVerticalBox::Slot().AutoHeight().Padding(0,14,0,6)
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
            FocusLater(WorldButtons.IsValidIndex(SelectedWorld)?WorldButtons[SelectedWorld]:FirstButton);
        }
        else if (Owner.IsValid()) Owner->ResumeGame();
    }
    // A world card: "WORLD II", its name, and its enemies and bosses. Every card opens its stage list.
    void AddWorld(int32 World,const TCHAR* Number,const TCHAR* Name,const TCHAR* Detail)
    {
        const FLinearColor Ink(.94f,.88f,.77f),Faint(.66f,.6f,.55f);
        auto Card=SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(Number))
                .Font(FCoreStyle::GetDefaultFontStyle("Regular",13)).ColorAndOpacity(FLinearColor(.64f,.49f,.35f)).Justification(ETextJustify::Center)]
            + SVerticalBox::Slot().FillHeight(1).VAlign(VAlign_Center)[SNew(STextBlock).Text(FText::FromString(Name))
                .Font(FCoreStyle::GetDefaultFontStyle("Regular",18)).ColorAndOpacity(Ink).AutoWrapText(true).Justification(ETextJustify::Center)]
            + SVerticalBox::Slot().AutoHeight().Padding(0,8,0,0)[SNew(STextBlock).Text(FText::FromString(Detail))
                .Font(FCoreStyle::GetDefaultFontStyle("Regular",11)).ColorAndOpacity(Faint).AutoWrapText(true).Justification(ETextJustify::Center)];
        WorldLines[World<=3?0:1]->AddSlot().AutoWidth().Padding(6,0)[SNew(SBox).WidthOverride(225).HeightOverride(172)
            [SAssignNew(WorldButtons[World],SButton).ContentPadding(FMargin(14,14))
            .ButtonColorAndOpacity(FLinearColor(.16f,.08f,.11f,.95f))
            .OnClicked_Lambda([this,World]() {
                SelectedWorld=World; bStageOpen=true;
                FocusLater(FirstStageButtons[World].IsValid()?FirstStageButtons[World]:BackButton);
                return FReply::Handled(); })[Card]]];
    }
    void AddStage(int32 World,int32 Level,const TCHAR* Title,const TCHAR* Detail,bool Available)
    {
        const TSharedPtr<SVerticalBox> Panel=StagePanels[World];
        const FLinearColor Ink=Available?FLinearColor(.94f,.88f,.77f):FLinearColor(.43f,.43f,.45f);
        auto Card=SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(Title))
                .Font(FCoreStyle::GetDefaultFontStyle("Regular",20)).ColorAndOpacity(Ink)]
            + SVerticalBox::Slot().AutoHeight().Padding(0,5,0,0)[SNew(STextBlock)
                .Text(FText::FromString(FString(Detail)+(Available||Level==ComingLater?TEXT(""):TEXT("  /  LOCKED"))))
                .Font(FCoreStyle::GetDefaultFontStyle("Regular",13)).ColorAndOpacity(Ink)];
        if (Available)
        {
            TSharedPtr<SButton> Button;
            Panel->AddSlot().AutoHeight().Padding(0,4)[SAssignNew(Button,SButton).ContentPadding(FMargin(18,9))
                .ButtonColorAndOpacity(FLinearColor(.16f,.08f,.11f,.95f))
                .OnClicked_Lambda([this,Level]() {
                    auto* GM=Owner.IsValid()?Cast<AArenaGameMode>(UGameplayStatics::GetGameMode(Owner.Get())):nullptr;
                    if (!Owner.IsValid()) return FReply::Handled();
                    if (Level==CourtPreview) { Owner->ResumeGame(); if (GM) GM->TravelToSuccubusCourt(); }
                    else if (Level==ForestRun) { Owner->ResumeGame(); if (GM) GM->StartForestRun(); }
                    else if (Level==Endless) { Owner->ResumeGame(); if (GM) GM->StartEndless(); }
                    else if (Level>=1 && Level<=Unlocked && Level<=4) { Owner->ResumeGame(); if (GM) GM->TravelToCampaign(Level); }
                    return FReply::Handled(); })[Card]];
            if (!FirstStageButtons[World].IsValid()) FirstStageButtons[World]=Button;
        }
        else Panel->AddSlot().AutoHeight().Padding(0,4)[SNew(SBorder).Padding(FMargin(18,9))
            .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(.07f,.07f,.08f,.8f))[Card]];
    }
    TWeakObjectPtr<AHellgirlPlayerController> Owner;
    FSlateBrush Frame;
    int32 Unlocked=1,SelectedWorld=0;
    bool bStageOpen=false;
    static constexpr int32 WorldCount=5;
    TSharedPtr<SVerticalBox> WorldRows;
    TArray<TSharedPtr<SHorizontalBox>> WorldLines;          // two rows of world cards: I-III, IV-V
    TArray<TSharedPtr<SVerticalBox>> StagePanels;           // by world number (1-5)
    TArray<TSharedPtr<SButton>> WorldButtons, FirstStageButtons;
    // Stage ids outside the campaign level numbers (1-4):
    static constexpr int32 CourtPreview=100;  // World III's court, a map preview
    static constexpr int32 ForestRun=101;     // the random-rooms forest run (out of the game for now)
    static constexpr int32 Endless=102;       // endless goblin waves in the Stage 2 arena
    static constexpr int32 ComingLater=-1;    // a stage still to be designed
    TSharedPtr<SButton> BackButton;
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
                return FText::FromString(FString::Printf(TEXT("Your Soul Coins: %lld"),Wallet?Wallet->Coins:0)); })];
            Items->AddSlot().AutoHeight().Padding(0,6)
            [SAssignNew(FirstButton,SButton).HAlign(HAlign_Center).ContentPadding(14)
                .IsEnabled_Lambda([this]() { const auto* Wallet=Owner.IsValid()?Cast<UHellgirlWallet>(Owner->GetGameInstance()):nullptr; return Wallet && Wallet->CanBuyGoblinQueen(); })
                .OnClicked_Lambda([this]() { if (Owner.IsValid()) if (auto* Wallet=Cast<UHellgirlWallet>(Owner->GetGameInstance())) Wallet->BuyGoblinQueen(); return FReply::Handled(); })
                [SNew(STextBlock).Text_Lambda([this]() {
                    const auto* Wallet=Owner.IsValid()?Cast<UHellgirlWallet>(Owner->GetGameInstance()):nullptr;
                    return FText::FromString(Wallet && Wallet->bGoblinQueenOwned ? TEXT("GOBLIN QUEEN / OWNED") : TEXT("GOBLIN QUEEN SKIN / 20000 SOUL COINS")); })]];
            // It is only for sale once endless Goblins wave 50 has been cleared.
            Items->AddSlot().AutoHeight().Padding(0,4)[SNew(STextBlock).AutoWrapText(true).Justification(ETextJustify::Center).Font(FCoreStyle::GetDefaultFontStyle("Regular",13)).ColorAndOpacity(FLinearColor(.75f,.6f,.45f))
                .Text_Lambda([this]() {
                    const auto* Wallet=Owner.IsValid()?Cast<UHellgirlWallet>(Owner->GetGameInstance()):nullptr;
                    return FText::FromString(!Wallet || Wallet->bGoblinQueenOwned ? TEXT("") : !HellgirlAchievements::Has(HellgirlAchievements::EndlessGoblins50)
                        ? TEXT("Requires: beat wave 50 of endless Goblins") : TEXT("")); })];
            Items->AddSlot().AutoHeight().Padding(0,12)[Text(TEXT("Equip purchased outfits at the campfire."),14)];
            Items->AddSlot().AutoHeight()[SNew(STextBlock).Text_Lambda([this]() {
                const auto* Wallet=Owner.IsValid()?Cast<UHellgirlWallet>(Owner->GetGameInstance()):nullptr;
                return FText::FromString(Wallet && Wallet->bSaveFailed ? TEXT("Couldn't save purchase. Your Soul Coins were not spent. Try again.") : TEXT("")); })];
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
    // After the first talk, the goblin just opens his shop (as the campfire and road open theirs).
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
