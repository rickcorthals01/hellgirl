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
#include "UI/GothicUIKit.h"
#include "Widgets/Layout/SConstraintCanvas.h"

// The level select at the camp's forest road: the first five worlds (from "Levels, Enemies, Bosses.txt"), shown for
// overview and dev testing even where nothing is playable yet. Drawn as the placeholder art (GothicUI):
//   "World Select.png" (724 x 543): the five worlds down the left, the highlighted one's preview and description, ENTER.
//   "Stage Select.png" (724 x 543): three stage cards (a numeral in the circle; name, detail and state on the lines),
//   paged with arrows when a world has more; PLAY plays the highlighted card, BACK returns to the worlds.
class SLevelSelectMenu : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SLevelSelectMenu) {} SLATE_ARGUMENT(TWeakObjectPtr<AHellgirlPlayerController>,Owner)
        SLATE_ARGUMENT(UTexture2D*,Frame) SLATE_END_ARGS()
    TSharedPtr<SButton> FirstButton;

    TSharedPtr<SButton> OpenFirstWorldForPreview()
    {
        SelectedWorld=1; StageOffset=0; bStageOpen=true;
        return CardButtons[0];
    }

    void Construct(const FArguments& Args)
    {
        Owner=Args._Owner;
        const auto* GM=Owner.IsValid()?Cast<AArenaGameMode>(UGameplayStatics::GetGameMode(Owner.Get())):nullptr;
        Unlocked=HellgirlProgress::PlaytestUnlockAll() ? 4 : GM?GM->GetUnlockedLevel():1;
        auto Won=[](const TCHAR* Flag) { return HellgirlProgress::PlaytestUnlockAll() || HellgirlProgress::Flag(Flag); };

        AddWorld(1,TEXT("WORLD I"),TEXT("GOBLIN RUINS"),TEXT("Goblins · Goblin Queen"),TEXT("The ruined castle where Hellgirl woke. Goblin raiders, their army, and the Goblin Queen on her throne."));
        AddWorld(2,TEXT("WORLD II"),TEXT("THE SWAMP"),TEXT("Rats & Frogs · Rat Queen & Frog King"),TEXT("The swamp of souls: glowing water, drowned hands, and the queens of rats and frogs."));
        AddWorld(3,TEXT("WORLD III"),TEXT("SUCCUBUS COURT"),TEXT("Succubi · Succubus Queen"),TEXT("The succubi's training halls. The court can be walked; its fight comes later."));
        AddWorld(4,TEXT("WORLD IV"),TEXT("THE LOWER CIRCLES"),TEXT("Ghosts · Deprived · Ghost King"),TEXT("A graveyard under the full moon, and below it the Deprived's frozen maze. Both can be walked; their enemies come later."));
        AddWorld(5,TEXT("WORLD V"),TEXT("IMP TORTURE ARENA"),TEXT("Imps · Imp Commander"),TEXT("The imps' arena of lava and chains, and their commander."));

        // World I: the Goblin campaign and its endless mode.
        AddStage(1,1,TEXT("I"),TEXT("FIRST RAID"),TEXT("The first waves"),Unlocked>=1);
        AddStage(1,2,TEXT("II"),TEXT("THE GOBLIN ARMY"),TEXT("Seven waves"),Unlocked>=2);
        AddStage(1,3,TEXT("III"),TEXT("THE QUEEN"),TEXT("Fourteen waves, the Queen"),Unlocked>=3);
        // Unlocked by beating Stage 2 (02.5).
        if (Won(TEXT("Stage2Won"))) AddStage(1,Endless,TEXT("∞"),TEXT("ENDLESS"),*FString::Printf(TEXT("Best wave %d"),HellgirlProgress::EndlessBest()),true);
        // The forest run (random rooms, Levels/ForestRun.cpp) is out of the game for now; its code stays for later:
        // AddStage(1,ForestRun,TEXT("F"),TEXT("FOREST RUN"),TEXT("Random rooms"),true);
        // World II: the swamp (Levels/SwampStages.cpp). Stage I opens once World I is won; each stage won opens the next.
        AddStage(2,SwampStageOne,TEXT("I"),TEXT("SWAMP OF SOULS"),TEXT("Ten waves"),Won(TEXT("Stage3Won")));
        AddStage(2,SwampStageTwo,TEXT("II"),TEXT("DEEPER IN"),TEXT("Ten rooms, Frog King"),Won(TEXT("SwampStage1Won")));
        AddStage(2,SwampStageThree,TEXT("III"),TEXT("THE DOORWAY"),TEXT("The Rat Queen"),Won(TEXT("SwampStage2Won")));
        // World III: the court can be walked; its fight comes later.
        AddStage(3,CourtPreview,TEXT("I"),TEXT("THE COURT"),TEXT("Map preview"),true);
        AddStage(3,ComingLater,TEXT("II"),TEXT("SUCCUBUS QUEEN"),TEXT("Coming later"),false);
        // World IV: the ghosts' graveyard can be walked (random rooms); the ghosts come later.
        AddStage(4,GraveyardPreview,TEXT("I"),TEXT("THE GRAVEYARD"),TEXT("Map preview"),true);
        // The Deprived's frozen maze can be walked too (a random spawn room each run); the Deprived come later.
        AddStage(4,MazePreview,TEXT("II"),TEXT("THE FROZEN MAZE"),TEXT("Map preview"),true);
        AddStage(4,ComingLater,TEXT("III"),TEXT("LOWER CIRCLES"),TEXT("Coming later"),false);
        // World V: the Imp arena (campaign level 4).
        AddStage(5,4,TEXT("I"),TEXT("TORTURE ARENA"),TEXT("Imp Commander"),Unlocked>=4);
        AddStage(5,ComingLater,TEXT("II"),TEXT("NEXT STAGE"),TEXT("Coming later"),false);

        using namespace GothicUI;
        // The worlds.
        TArray<FPlace> WorldPlaces;
        static const float ItemY[WorldCount]={72.f,152.f,232.f,312.f,391.f};
        WorldButtons.SetNum(WorldCount+1);
        for (int32 World=1; World<=WorldCount; ++World)
        {
            const FWorldInfo& Info=Worlds[World];
            TSharedRef<SWidget> Content=SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()[Label(Info.Number,8,FLinearColor(.64f,.49f,.35f))]
                + SVerticalBox::Slot().AutoHeight().Padding(10,2,6,0)[Label(Info.Name,11,Ink())];
            WorldPlaces.Add({FVector2D(80,ItemY[World-1]), FVector2D(178,78), ArtButton(WorldButtons[World],TEXT("WorldItemDark"),TEXT("WorldItemRed"),Content,
                [this,World]() { OpenWorld(World); },[this,World]() { return ShownWorld()==World; })});
        }
        FirstButton=WorldButtons[1];
        WorldPlaces.Add({FVector2D(290,95), FVector2D(366,30), Label([this]() { return Worlds[ShownWorld()].Number; },12,FLinearColor(.64f,.49f,.35f))});
        WorldPlaces.Add({FVector2D(290,130), FVector2D(366,50), Label([this]() { return Worlds[ShownWorld()].Name; },24,Ink())});
        WorldPlaces.Add({FVector2D(290,195), FVector2D(366,40), Label([this]() { return Worlds[ShownWorld()].Enemies; },13,FLinearColor(.95f,.72f,.32f))});
        WorldPlaces.Add({FVector2D(290,270), FVector2D(366,40), Label([this]() {
            int32 Open=0; for (const FStage& S : Stages[ShownWorld()]) Open+=S.bAvailable;
            return FString::Printf(TEXT("%d STAGE%s  ·  %d OPEN"),Stages[ShownWorld()].Num(),Stages[ShownWorld()].Num()==1?TEXT(""):TEXT("S"),Open); },11,Faint())});
        WorldPlaces.Add({FVector2D(292,364), FVector2D(366,62), Label([this]() { return Worlds[ShownWorld()].Description; },10,Faint())});
        TSharedPtr<SButton> Enter;
        WorldPlaces.Add({FVector2D(326,432), FVector2D(300,60), ArtButton(Enter,TEXT("WorldButton"),nullptr,Label(TEXT("ENTER"),16,Ink()),
            [this]() { OpenWorld(SelectedWorld); })});
        WorldPlaces.Add({FVector2D(262,500), FVector2D(200,24), Label(TEXT("ESC / B  ·  BACK TO CAMP"),8,Faint())});

        // The stages of the chosen world.
        TArray<FPlace> StagePlaces;
        static const float CardX[3]={103.f,303.f,503.f};
        CardButtons.SetNum(3);
        for (int32 Card=0; Card<3; ++Card)
        {
            auto Stage=[this,Card]() -> const FStage* { return Stages[SelectedWorld].IsValidIndex(StageOffset+Card) ? &Stages[SelectedWorld][StageOffset+Card] : nullptr; };
            auto Line=[Stage](int32 Which) { return [Stage,Which]() {
                const FStage* S=Stage(); if (!S) return FString();
                return Which==0 ? S->Name : Which==1 ? S->Detail : S->Level==ComingLater ? FString(TEXT("COMING LATER")) : S->bAvailable ? FString(TEXT("PLAY")) : FString(TEXT("LOCKED")); }; };
            TSharedRef<SConstraintCanvas> Face=SNew(SConstraintCanvas);
            Face->AddSlot().Anchors(FAnchors(0.f,0.f)).Alignment(FVector2D::ZeroVector).Offset(FMargin(25,48,100,50))[Label([Stage]() { const FStage* S=Stage(); return S?S->Numeral:FString(); },20,Ink())];
            for (int32 L=0; L<3; ++L)
                Face->AddSlot().Anchors(FAnchors(0.f,0.f)).Alignment(FVector2D::ZeroVector).Offset(FMargin(38,146+L*37,92,24))[Label(Line(L),L==0?7:6,L==2?FLinearColor(.95f,.72f,.32f):Ink())];
            StagePlaces.Add({FVector2D(CardX[Card],58), FVector2D(150,275), SNew(SBox).Visibility_Lambda([Stage]() { return Stage()?EVisibility::Visible:EVisibility::Hidden; })
                [ArtButton(CardButtons[Card],TEXT("StageCardDark"),TEXT("StageCardRed"),SNew(SBox).WidthOverride(150).HeightOverride(275)[Face],
                    [this,Card]() { PlayCard(Card); },[this,Card]() { return LitCard()==Card; },[Stage]() { const FStage* S=Stage(); return S && S->bAvailable; })]});
        }
        TSharedPtr<SButton> Earlier, Later;
        StagePlaces.Add({FVector2D(40,165), FVector2D(50,60), SNew(SBox).Visibility_Lambda([this]() { return StageOffset>0?EVisibility::Visible:EVisibility::Hidden; })
            [ArtButton(Earlier,TEXT("None"),nullptr,Label(TEXT("‹"),30,Ink()),[this]() { StageOffset=FMath::Max(0,StageOffset-1); })]});
        StagePlaces.Add({FVector2D(655,165), FVector2D(50,60), SNew(SBox).Visibility_Lambda([this]() { return StageOffset+3<Stages[SelectedWorld].Num()?EVisibility::Visible:EVisibility::Hidden; })
            [ArtButton(Later,TEXT("None"),nullptr,Label(TEXT("›"),30,Ink()),[this]() { StageOffset=FMath::Min(StageOffset+1,FMath::Max(0,Stages[SelectedWorld].Num()-3)); })]});
        TSharedPtr<SButton> Play;
        StagePlaces.Add({FVector2D(150,356), FVector2D(230,58), ArtButton(Play,TEXT("StageButtonDark"),TEXT("StageButtonRed"),Label(TEXT("PLAY"),15,Ink()),
            [this]() { PlayCard(LastCard); },nullptr,[this]() { return Stages[SelectedWorld].IsValidIndex(StageOffset+LastCard) && Stages[SelectedWorld][StageOffset+LastCard].bAvailable; })});
        StagePlaces.Add({FVector2D(390,358), FVector2D(200,50), ArtButton(BackButton,TEXT("StageButtonDark"),TEXT("StageButtonRed"),Label(TEXT("BACK"),14,Ink()),[this]() { GoBack(); })});
        StagePlaces.Add({FVector2D(212,22), FVector2D(300,26), Label([this]() { return Worlds[SelectedWorld].Number+TEXT("  ·  ")+Worlds[SelectedWorld].Name; },10,Ink())});

        ChildSlot[SNew(SOverlay)
            + SOverlay::Slot()[SNew(SBox).Visibility_Lambda([this]() { return bStageOpen?EVisibility::Collapsed:EVisibility::Visible; })
                [Screen(TEXT("WorldSelect"),FVector2D(724,543),WorldPlaces)]]
            + SOverlay::Slot()[SNew(SBox).Visibility_Lambda([this]() { return bStageOpen?EVisibility::Visible:EVisibility::Collapsed; })
                [Screen(TEXT("StageSelect"),FVector2D(724,543),StagePlaces)]]];
    }

    virtual FReply OnPreviewKeyDown(const FGeometry&,const FKeyEvent& Event) override
    {
        // Held-button repeats (e.g. B still held from a dodge) must not close the menu.
        if (!Event.IsRepeat() && (Event.GetKey()==EKeys::Escape || Event.GetKey()==EKeys::Gamepad_FaceButton_Right || Event.GetKey()==EKeys::Gamepad_Special_Right))
        { GoBack(); return FReply::Handled(); }
        return FReply::Unhandled();
    }
private:
    struct FWorldInfo { FString Number, Name, Enemies, Description; };
    struct FStage { int32 Level; FString Numeral, Name, Detail; bool bAvailable; };
    // The world whose preview shows: the one hovered or focused, else the last chosen.
    int32 ShownWorld() const
    {
        for (int32 World=1; World<WorldButtons.Num(); ++World)
            if (GothicUI::IsLit(WorldButtons[World])) { SelectedWorld=World; return World; }
        return SelectedWorld;
    }
    // The card lit (hovered or focused), remembered for PLAY.
    int32 LitCard() const
    {
        for (int32 Card=0; Card<CardButtons.Num(); ++Card) if (GothicUI::IsLit(CardButtons[Card])) { LastCard=Card; return Card; }
        return -1;
    }
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
    void OpenWorld(int32 World)
    {
        SelectedWorld=FMath::Clamp(World,1,WorldCount); StageOffset=0; LastCard=0; bStageOpen=true;
        TSharedPtr<SButton> First=BackButton;
        for (int32 Card=0; Card<3; ++Card)
            if (Stages[SelectedWorld].IsValidIndex(Card) && Stages[SelectedWorld][Card].bAvailable) { First=CardButtons[Card]; break; }
        FocusLater(First);
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
    void AddWorld(int32 World,const TCHAR* Number,const TCHAR* Name,const TCHAR* Enemies,const TCHAR* Description)
    {
        Worlds[World]={Number,Name,Enemies,Description};
    }
    void AddStage(int32 World,int32 Level,const TCHAR* Numeral,const TCHAR* Name,const TCHAR* Detail,bool Available)
    {
        Stages[World].Add({Level,Numeral,Name,Detail,Available});
    }
    void PlayCard(int32 Card)
    {
        if (!Stages[SelectedWorld].IsValidIndex(StageOffset+Card) || !Owner.IsValid()) return;
        const FStage& S=Stages[SelectedWorld][StageOffset+Card];
        if (!S.bAvailable) return;
        const int32 Level=S.Level;
        auto* GM=Cast<AArenaGameMode>(UGameplayStatics::GetGameMode(Owner.Get()));
        if (Level==CourtPreview) { Owner->ResumeGame(); if (GM) GM->TravelToSuccubusCourt(); }
        else if (Level==ForestRun) { Owner->ResumeGame(); if (GM) GM->StartForestRun(); }
        else if (Level==GraveyardPreview) { Owner->ResumeGame(); if (GM) GM->StartGraveyard(); }
        else if (Level==MazePreview) { Owner->ResumeGame(); if (GM) GM->StartFrozenMaze(); }
        else if (Level>=SwampStageOne && Level<=SwampStageThree) { Owner->ResumeGame(); if (GM) GM->StartSwampStage(Level-SwampStageOne+1); }
        else if (Level==Endless) { Owner->ResumeGame(); if (GM) GM->StartEndless(); }
        else if (Level>=1 && Level<=Unlocked && Level<=4) { Owner->ResumeGame(); if (GM) GM->TravelToCampaign(Level); }
    }
    TWeakObjectPtr<AHellgirlPlayerController> Owner;
    int32 Unlocked=1, StageOffset=0;
    mutable int32 SelectedWorld=1, LastCard=0;
    bool bStageOpen=false;
    static constexpr int32 WorldCount=5;
    FWorldInfo Worlds[WorldCount+1];
    TArray<FStage> Stages[WorldCount+1];
    TArray<TSharedPtr<SButton>> WorldButtons, CardButtons;
    // Stage ids outside the campaign level numbers (1-4):
    static constexpr int32 CourtPreview=100;  // World III's court, a map preview
    static constexpr int32 ForestRun=101;     // the random-rooms forest run (out of the game for now)
    static constexpr int32 Endless=102;       // endless goblin waves in the Stage 2 arena
    static constexpr int32 GraveyardPreview=103; // World IV's graveyard, a map preview (random rooms)
    static constexpr int32 SwampStageOne=105, SwampStageTwo=106, SwampStageThree=107; // World II's swamp stages
    static constexpr int32 MazePreview=108;   // World IV's frozen maze, a map preview
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
        // The goblin's shop, drawn as the placeholder art "Shop menu.png" (1448 x 1086): tabs for skins, moves and
        // upgrades, a grid of twelve cells, and the preview panel on the right (the chosen item, its status and BUY).
        if (Args._Kind==2) { BuildShop(); return; }
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
            // Two columns: the energy moves, and the permanent upgrades with the outfits below (Rules/ShopUpgrades.h).
            TSharedRef<SVerticalBox> Moves=SNew(SVerticalBox), Stats=SNew(SVerticalBox);
            auto Heading=[](const TCHAR* Label) { return SNew(STextBlock).Text(FText::FromString(Label)).Font(FCoreStyle::GetDefaultFontStyle("Bold",15)).ColorAndOpacity(FLinearColor(.75f,.6f,.45f)); };
            Moves->AddSlot().AutoHeight().Padding(0,0,0,6)[Heading(TEXT("MOVES"))];
            Stats->AddSlot().AutoHeight().Padding(0,0,0,6)[Heading(TEXT("PERMANENT UPGRADES"))];
            for (int32 Item=0; Item<HellgirlShop::Count; ++Item)
            {
                TSharedPtr<SButton> Button;
                (HellgirlShop::IsMove(Item)?Moves:Stats)->AddSlot().AutoHeight().Padding(0,4)
                [SAssignNew(Button,SButton).HAlign(HAlign_Fill).ContentPadding(FMargin(14,8))
                    .IsEnabled_Lambda([this,Item]() { const auto* Wallet=Owner.IsValid()?Cast<UHellgirlWallet>(Owner->GetGameInstance()):nullptr; return Wallet && Wallet->CanBuyShopItem(Item); })
                    .OnClicked_Lambda([this,Item]() { if (Owner.IsValid()) if (auto* Wallet=Cast<UHellgirlWallet>(Owner->GetGameInstance())) Wallet->BuyShopItem(Item); return FReply::Handled(); })
                    [SNew(SVerticalBox)
                        + SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular",16)).ColorAndOpacity(FLinearColor(.92f,.85f,.69f))
                            .Text_Lambda([this,Item]() {
                                const auto* Wallet=Owner.IsValid()?Cast<UHellgirlWallet>(Owner->GetGameInstance()):nullptr;
                                const HellgirlShop::FInfo& Info=HellgirlShop::Info(Item);
                                const int32 Level=Wallet?Wallet->ShopLevel(Item):0;
                                const FString Price=Level>=Info.MaxLevel ? (HellgirlShop::IsOneOff(Item)?TEXT("OWNED"):TEXT("MAXED")) : FString::Printf(TEXT("%lld"),Info.Price);
                                return FText::FromString(FString::Printf(TEXT("%s  /  %s"),Info.Name,*Price)); })]
                        + SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular",12)).ColorAndOpacity(FLinearColor(.62f,.58f,.52f)).AutoWrapText(true)
                            .Text_Lambda([this,Item]() {
                                const auto* Wallet=Owner.IsValid()?Cast<UHellgirlWallet>(Owner->GetGameInstance()):nullptr;
                                const HellgirlShop::FInfo& Info=HellgirlShop::Info(Item);
                                if (HellgirlShop::IsOneOff(Item)) return FText::FromString(Info.Detail);
                                // Stats show what is owned so far.
                                const int32 Level=Wallet?Wallet->ShopLevel(Item):0;
                                const HellgirlShop::FStats Now=HellgirlShop::Stats(Wallet?Wallet->ShopLevels:TArray<int32>());
                                const FString Total=Item==static_cast<int32>(HellgirlShop::EItem::Health) ? FString::Printf(TEXT("+%d health"),FMath::RoundToInt(Now.BonusHealth))
                                    : Item==static_cast<int32>(HellgirlShop::EItem::Souls) ? FString::Printf(TEXT("+%g%% Souls"),Now.SoulBonus*100.f)
                                    : Item==static_cast<int32>(HellgirlShop::EItem::Damage) ? FString::Printf(TEXT("+%d%% damage"),FMath::RoundToInt((Now.Damage-1.f)*100.f))
                                    : FString::Printf(TEXT("+%d%% speed"),FMath::RoundToInt((Now.Speed-1.f)*100.f));
                                return FText::FromString(FString::Printf(TEXT("%s  ·  owned %d / %d (%s)"),Info.Detail,Level,Info.MaxLevel,*Total)); })]]];
                if (!FirstButton) FirstButton=Button;
            }
            Stats->AddSlot().AutoHeight().Padding(0,16,0,6)[Heading(TEXT("OUTFITS"))];
            Stats->AddSlot().AutoHeight().Padding(0,4)
            [SNew(SButton).HAlign(HAlign_Center).ContentPadding(FMargin(14,8))
                .IsEnabled_Lambda([this]() { const auto* Wallet=Owner.IsValid()?Cast<UHellgirlWallet>(Owner->GetGameInstance()):nullptr; return Wallet && Wallet->CanBuyGoblinQueen(); })
                .OnClicked_Lambda([this]() { if (Owner.IsValid()) if (auto* Wallet=Cast<UHellgirlWallet>(Owner->GetGameInstance())) Wallet->BuyGoblinQueen(); return FReply::Handled(); })
                [SNew(STextBlock).Text_Lambda([this]() {
                    const auto* Wallet=Owner.IsValid()?Cast<UHellgirlWallet>(Owner->GetGameInstance()):nullptr;
                    return FText::FromString(Wallet && Wallet->bGoblinQueenOwned ? TEXT("GOBLIN QUEEN / OWNED") : TEXT("GOBLIN QUEEN SKIN / FREE")); })]];
            // It is only for sale once endless Goblins wave 50 has been cleared.
            Stats->AddSlot().AutoHeight().Padding(0,4)[SNew(STextBlock).AutoWrapText(true).Justification(ETextJustify::Center).Font(FCoreStyle::GetDefaultFontStyle("Regular",13)).ColorAndOpacity(FLinearColor(.75f,.6f,.45f))
                .Text_Lambda([this]() {
                    const auto* Wallet=Owner.IsValid()?Cast<UHellgirlWallet>(Owner->GetGameInstance()):nullptr;
                    return FText::FromString(!Wallet || Wallet->bGoblinQueenOwned ? TEXT("") : !HellgirlAchievements::Has(HellgirlAchievements::EndlessGoblins50)
                        ? TEXT("Requires: beat wave 50 of endless Goblins") : TEXT("")); })];
            Stats->AddSlot().AutoHeight().Padding(0,6)[Text(TEXT("Equip purchased outfits at the campfire."),13)];
            Items->AddSlot().AutoHeight()[SNew(SHorizontalBox)
                + SHorizontalBox::Slot().FillWidth(1).Padding(0,0,14,0)[Moves]
                + SHorizontalBox::Slot().FillWidth(1).Padding(14,0,0,0)[Stats]];
            Items->AddSlot().AutoHeight()[SNew(STextBlock).Text_Lambda([this]() {
                const auto* Wallet=Owner.IsValid()?Cast<UHellgirlWallet>(Owner->GetGameInstance()):nullptr;
                return FText::FromString(Wallet && Wallet->bSaveFailed ? TEXT("Couldn't save purchase. Your Soul Coins were not spent. Try again.") : TEXT("")); })];
        }
        TSharedPtr<SButton> Close;
        Items->AddSlot().AutoHeight().Padding(0,24,0,0)
        [SAssignNew(Close,SButton).HAlign(HAlign_Center).ContentPadding(14).OnClicked_Lambda([this]() { if (Owner.IsValid()) Owner->ResumeGame(); return FReply::Handled(); })[Text(TEXT("BACK TO CAMP"),18)]];
        if (!FirstButton) FirstButton=Close;
        ChildSlot.HAlign(HAlign_Center).VAlign(VAlign_Center)
        [SNew(SBox).WidthOverride(Args._Kind==2 ? 900 : 470)
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
    // The shop's items, per tab: a name, what it does, its state ("500 SOUL COINS", "OWNED"...), and buying it.
    struct FShopEntry { FString Name, Detail; TFunction<FString()> Status; TFunction<bool()> CanBuy; TFunction<void()> Buy; };
    TArray<FShopEntry> ShopTabs[3];
    int32 ShopTab=0, ShopItem=0;
    UHellgirlWallet* ShopWallet() const { return Owner.IsValid()?Cast<UHellgirlWallet>(Owner->GetGameInstance()):nullptr; }
    void BuildShop()
    {
        using namespace GothicUI;
        auto Wallet=[this]() { return ShopWallet(); };
        // Skins: the outfits (equipped at the campfire).
        ShopTabs[0].Add({HellgirlOutfits::Label(0),TEXT("Hellgirl's rags. Always yours."),[]() { return FString(TEXT("OWNED")); },[]() { return false; },nullptr});
        ShopTabs[0].Add({HellgirlOutfits::Label(4),TEXT("The Goblin Queen's outfit. Free once you have beaten wave 50 of endless Goblins."),
            [Wallet]() { const auto* W=Wallet(); return W && W->bGoblinQueenOwned ? FString(TEXT("OWNED"))
                : HellgirlAchievements::Has(HellgirlAchievements::EndlessGoblins50) ? FString(TEXT("FREE")) : FString(TEXT("BEAT ENDLESS WAVE 50")); },
            [Wallet]() { const auto* W=Wallet(); return W && W->CanBuyGoblinQueen(); },
            [Wallet]() { if (auto* W=Wallet()) W->BuyGoblinQueen(); }});
        // Moves and permanent upgrades (Rules/ShopUpgrades.h).
        for (int32 Item=0; Item<HellgirlShop::Count; ++Item)
        {
            const HellgirlShop::FInfo& Info=HellgirlShop::Info(Item);
            ShopTabs[HellgirlShop::IsMove(Item)?1:2].Add({Info.Name,Info.Detail,
                [Wallet,Item]() {
                    const auto* W=Wallet(); const HellgirlShop::FInfo& I=HellgirlShop::Info(Item);
                    const int32 Level=W?W->ShopLevel(Item):0;
                    if (Level>=I.MaxLevel) return FString(HellgirlShop::IsOneOff(Item)?TEXT("OWNED"):TEXT("MAXED"));
                    return HellgirlShop::IsOneOff(Item) ? FString::Printf(TEXT("%lld SOUL COINS"),I.Price)
                        : FString::Printf(TEXT("%lld SOUL COINS  ·  owned %d / %d"),I.Price,Level,I.MaxLevel); },
                [Wallet,Item]() { const auto* W=Wallet(); return W && W->CanBuyShopItem(Item); },
                [Wallet,Item]() { if (auto* W=Wallet()) W->BuyShopItem(Item); }});
        }
        auto Entry=[this](int32 Index) -> const FShopEntry* { return ShopTabs[ShopTab].IsValidIndex(Index) ? &ShopTabs[ShopTab][Index] : nullptr; };
        TArray<FPlace> Places;
        static const TCHAR* TabNames[3]={TEXT("Skins"),TEXT("Moves"),TEXT("Upgrades")};
        static const float TabX[3]={106.f,362.f,614.f};
        for (int32 Tab=0; Tab<3; ++Tab)
        {
            TSharedPtr<SButton> Button;
            Places.Add({FVector2D(TabX[Tab],170), FVector2D(252,100), ArtButton(Button,TEXT("ShopTabDark"),TEXT("ShopTabRed"),Label(TabNames[Tab],28,Ink()),
                [this,Tab]() { ShopTab=Tab; ShopItem=0; },[this,Tab]() { return ShopTab==Tab; })});
            if (Tab==0) FirstButton=Button;
        }
        static const float CellX[4]={108.f,296.f,483.f,668.f}, CellY[3]={303.f,493.f,683.f};
        for (int32 Cell=0; Cell<12; ++Cell)
        {
            TSharedPtr<SButton> Button;
            TSharedRef<SWidget> Content=SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight().Padding(14,0)[Label([Entry,Cell]() { const FShopEntry* E=Entry(Cell); return E?E->Name:FString(); },18,Ink())]
                + SVerticalBox::Slot().AutoHeight().Padding(14,10,14,0)[Label([Entry,Cell]() { const FShopEntry* E=Entry(Cell); return E?E->Status():FString(); },12,FLinearColor(.95f,.72f,.32f))];
            Places.Add({FVector2D(CellX[Cell%4],CellY[Cell/4]), FVector2D(182,182), ArtButton(Button,TEXT("ShopCellDark"),TEXT("ShopCellRed"),Content,
                [this,Cell]() { ShopItem=Cell; },[this,Cell]() { return ShopItem==Cell; },[Entry,Cell]() { return Entry(Cell)!=nullptr; })});
        }
        // The preview panel: the chosen item over the pedestal's light, your Soul Coins, and BUY.
        Places.Add({FVector2D(905,200), FVector2D(420,60), Label([Entry,this]() { const FShopEntry* E=Entry(ShopItem); return E?E->Name:FString(); },30,Ink())});
        Places.Add({FVector2D(915,270), FVector2D(400,150), Label([Entry,this]() { const FShopEntry* E=Entry(ShopItem); return E?E->Detail:FString(); },17,Faint())});
        Places.Add({FVector2D(905,430), FVector2D(420,40), Label([Entry,this]() { const FShopEntry* E=Entry(ShopItem); return E?E->Status():FString(); },18,FLinearColor(.95f,.72f,.32f))});
        Places.Add({FVector2D(905,480), FVector2D(420,36), Label([Wallet]() { const auto* W=Wallet(); return FString::Printf(TEXT("YOUR SOUL COINS  %lld"),W?W->Coins:0); },17,FLinearColor(.55f,.82f,1.f))});
        TSharedPtr<SButton> Buy;
        Places.Add({FVector2D(970,770), FVector2D(290,66), ArtButton(Buy,TEXT("DeathDark"),TEXT("DeathRed"),
            Label([Entry,this]() { const FShopEntry* E=Entry(ShopItem); return E && E->CanBuy() ? FString(E->Status()==TEXT("FREE")?TEXT("CLAIM"):TEXT("BUY")) : FString(TEXT("—")); },20,Ink()),
            [Entry,this]() { if (const FShopEntry* E=Entry(ShopItem); E && E->CanBuy() && E->Buy) E->Buy(); },nullptr,
            [Entry,this]() { const FShopEntry* E=Entry(ShopItem); return E && E->CanBuy(); })});
        Places.Add({FVector2D(420,955), FVector2D(620,36), Label([Wallet]() { const auto* W=Wallet();
            return W && W->bSaveFailed ? FString(TEXT("Couldn't save the purchase. Your Soul Coins were not spent. Try again."))
                : FString(TEXT("Equip outfits at the campfire   ·   ESC / B  BACK TO CAMP")); },13,Faint())});
        ChildSlot[Screen(TEXT("ShopMenu"),FVector2D(1448,1086),Places)];
    }
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
    // 03.5, after the shop is unlocked: the next time she opens the forest road, the strange voice points her back to
    // endless, and the level select follows.
    else if (Kind==1 && HellgirlProgress::Flag(TEXT("ShopUnlocked")) && !HellgirlProgress::Flag(TEXT("EndlessSecrets")) && !HellgirlProgress::IsCheckRun()
        && ShowConversation(TEXT("C_EndlessSecrets")))
    {
        HellgirlProgress::SetFlag(TEXT("EndlessSecrets"));
        DialogueNextHubMenu=1;
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
    KeepMenuFocus();
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
