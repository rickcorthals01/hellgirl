#include "UI/HellgirlPlayerController.h"
#include "Progress/HellgirlWallet.h"
#include "Misc/ScopeExit.h"
#include "Fighter/ArenaFighter.h"
#include "Levels/ArenaGameMode.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"
#include "ImageUtils.h"
#include "ImageCore.h"
#include "Misc/Paths.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/CoreStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Components/InputComponent.h"
#include "Components/CapsuleComponent.h"
#include "Misc/ConfigCacheIni.h"
#include "Fighter/HellgirlOutfits.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SInputKeySelector.h"
#include "Widgets/Layout/SScrollBox.h"
#include "GameFramework/InputSettings.h"
#include "Components/SkeletalMeshComponent.h"

class SHellgirlPause : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SHellgirlPause) {} SLATE_ARGUMENT(TWeakObjectPtr<AHellgirlPlayerController>, Owner) SLATE_ARGUMENT(UTexture2D*, Frame) SLATE_END_ARGS()
    void Construct(const FArguments& Args)
    {
        Owner = Args._Owner;
        bOptionsOpen = Owner.IsValid() && Owner->IsAtMainMenu();
        for (int32 I = 0; I < HellgirlOutfits::Count; ++I) OutfitOptions.Add(MakeShared<int32>(I));
        FrameBrush.SetResourceObject(Args._Frame);
        FrameBrush.ImageSize = FVector2D(400,640);
        FrameBrush.DrawAs = ESlateBrushDrawType::Image;
        const FLinearColor Ink(.94f,.88f,.79f);
        auto Text = [&](const TCHAR* Label, int32 Size) {
            return SNew(STextBlock).Text(FText::FromString(Label)).Font(FCoreStyle::GetDefaultFontStyle("Regular",Size)).ColorAndOpacity(Ink).Justification(ETextJustify::Center);
        };
        TSharedRef<SVerticalBox> Controls = SNew(SVerticalBox);
        Controls->AddSlot().AutoHeight()[Text(TEXT("KEYBOARD / MOUSE"),18)];
        Controls->AddSlot().AutoHeight()[Text(TEXT("Controller bindings stay fixed"),10)];
        const TArray<FName> Actions = {"Attack","HeavyAttack","Dodge","Jump","Special","Sprint","Walk","WeaponFists","WeaponSword","Restart"};
        for (const FName Action : Actions)
        {
            Controls->AddSlot().AutoHeight().Padding(0,4)
            [SNew(SHorizontalBox)
              + SHorizontalBox::Slot().FillWidth(1)[SNew(STextBlock).Text(FText::FromName(Action))]
              + SHorizontalBox::Slot().FillWidth(1)
              [SNew(SInputKeySelector).AllowGamepadKeys(false).AllowModifierKeys(false)
                .SelectedKey_Lambda([Action]() {
                    for (const auto& Mapping : GetDefault<UInputSettings>()->GetActionMappings())
                        if (Mapping.ActionName == Action && !Mapping.Key.IsGamepadKey()) return FInputChord(Mapping.Key);
                    return FInputChord(); })
                .OnIsSelectingKeyChanged_Lambda([this]() { bSelectingKey = !bSelectingKey; })
                .OnKeySelected_Lambda([this,Action](const FInputChord& Chord) {
                    auto* Settings = GetMutableDefault<UInputSettings>();
                    if (!Chord.Key.IsValid() || Chord.Key.IsGamepadKey()) return;
                    for (const auto& M : Settings->GetActionMappings())
                        if (M.ActionName != Action && M.Key == Chord.Key) { ControlsStatus = TEXT("Key already used. Choose another."); return; }
                    for (const auto& M : Settings->GetAxisMappings())
                        if (M.Key == Chord.Key) { ControlsStatus = TEXT("Key reserved for movement / camera."); return; }
                    const auto Mappings = Settings->GetActionMappings();
                    for (const auto& M : Mappings) if (M.ActionName == Action && !M.Key.IsGamepadKey()) Settings->RemoveActionMapping(M,false);
                    Settings->AddActionMapping(FInputActionKeyMapping(Action,Chord.Key),false);
                    Settings->SaveKeyMappings(); Settings->ForceRebuildKeymaps();
                    ControlsStatus = TEXT("Saved");
                })]];
        }
        Controls->AddSlot().AutoHeight()[SNew(STextBlock).Text_Lambda([this]() { return FText::FromString(ControlsStatus); })];
        Controls->AddSlot().AutoHeight().Padding(0,12)[SAssignNew(OptionsBack,SButton).OnClicked_Lambda([this]() { if (Owner.IsValid() && Owner->IsAtMainMenu()) Owner->ShowMainMenu(); else bOptionsOpen = false; return FReply::Handled(); })[Text(TEXT("BACK"),18)]];
        ChildSlot
        [ SNew(SOverlay)
          + SOverlay::Slot()[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Owner.IsValid() && Owner->IsAtMainMenu()?FLinearColor::Black:FLinearColor(.015f,.01f,.025f,.65f))]
          + SOverlay::Slot().Padding(10)[SNew(SImage).Image(&FrameBrush).ColorAndOpacity(FLinearColor(1,1,1,.85f)).Visibility(EVisibility::HitTestInvisible)]
          + SOverlay::Slot().Padding(24).HAlign(HAlign_Center).VAlign(VAlign_Center)
          [ SNew(SScaleBox).Stretch(EStretch::ScaleToFit).StretchDirection(EStretchDirection::DownOnly)
            [ SNew(SBox).WidthOverride(460).HeightOverride(720)
              [ SNew(SOverlay)
                + SOverlay::Slot().Padding(FMargin(62,105,62,80))
                [ SNew(SVerticalBox).Visibility_Lambda([this]() { return bOptionsOpen ? EVisibility::Collapsed : EVisibility::Visible; })
                  + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,8)[Text(TEXT("H E L L G I R L"),16)]
                  + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,18)[Text(TEXT("—  PAUSED  —"),12)]
                  + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,18)
                  [ SAssignNew(PlayButton,SButton).HAlign(HAlign_Center).ContentPadding(FMargin(24,14)).ButtonColorAndOpacity(FLinearColor(.34f,.22f,.28f,.4f))
                    .OnClicked_Lambda([this]() { if (Owner.IsValid()) Owner->ResumeGame(); return FReply::Handled(); })[Text(TEXT("PLAY"),25)] ]
                  + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,8)[Text(TEXT("OUTFIT"),12)]
                  + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,18)
                  [SNew(SComboBox<TSharedPtr<int32>>).IsEnabled_Lambda([this]() { const auto* GM=Owner.IsValid()?Cast<AArenaGameMode>(UGameplayStatics::GetGameMode(Owner.Get())):nullptr; return !GM || !GM->bForestHub; }).OptionsSource(&OutfitOptions)
                    .InitiallySelectedItem(OutfitOptions[Owner.IsValid() ? Owner->GetSelectedOutfit() : 0])
                    .MaxListHeight(300).ContentPadding(FMargin(12,10))
                    .OnGenerateWidget_Lambda([](TSharedPtr<int32> Item) -> TSharedRef<SWidget> {
                        return SNew(STextBlock).Text(FText::FromString(HellgirlOutfits::Label(*Item)))
                            .Font(FCoreStyle::GetDefaultFontStyle("Regular",18)); })
                    .OnSelectionChanged_Lambda([this](TSharedPtr<int32> Item, ESelectInfo::Type) {
                        if (Item.IsValid() && Owner.IsValid()) Owner->SelectOutfit(*Item); })
                    [SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular",18)).ColorAndOpacity(Ink)
                      .Text_Lambda([this]() { return FText::FromString(HellgirlOutfits::Label(Owner.IsValid() ? Owner->GetSelectedOutfit() : 0)); })]]
                  + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,3)[SNew(SButton).HAlign(HAlign_Center).OnClicked_Lambda([this]() { bOptionsOpen=true; return FReply::Handled(); })[Text(TEXT("OPTIONS"),22)]]
                  + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,24)[Text(TEXT("KEYBOARD CONTROLS"),10)]
                  + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,12)
                  [SNew(SButton).HAlign(HAlign_Center).OnClicked_Lambda([this]() { if (Owner.IsValid()) Owner->ShowMainMenu(); return FReply::Handled(); })[Text(TEXT("MAIN MENU"),22)]]
                  + SVerticalBox::Slot().AutoHeight().Padding(0,3)
                  [SNew(SButton).HAlign(HAlign_Center).IsEnabled_Lambda([this]() { auto* GM=Owner.IsValid()?Cast<AArenaGameMode>(UGameplayStatics::GetGameMode(Owner.Get())):nullptr; return GM && !GM->bForestHub && GM->GetUnlockedLevel()>=2; })
                    .OnClicked_Lambda([this]() { if (Owner.IsValid()) { auto* GM=Cast<AArenaGameMode>(UGameplayStatics::GetGameMode(Owner.Get())); Owner->ResumeGame(); if (GM) GM->TravelToHub(); } return FReply::Handled(); })[Text(TEXT("RETURN TO CAMP"),13)]]
                  + SVerticalBox::Slot().AutoHeight().Padding(0,6)
                  [SNew(SButton).HAlign(HAlign_Center).OnClicked_Lambda([this]() { if (Owner.IsValid()) Owner->OpenSaveMenu(); return FReply::Handled(); })[Text(TEXT("SAVE / LOAD"),16)]]
                  + SVerticalBox::Slot().FillHeight(1)
                  + SVerticalBox::Slot().AutoHeight()[Text(TEXT("ESC / P / START  ·  RESUME"),10)]
                ]
                + SOverlay::Slot().Padding(FMargin(25,65,25,30))
                [SNew(SScrollBox).Visibility_Lambda([this]() { return bOptionsOpen ? EVisibility::Visible : EVisibility::Collapsed; })
                  + SScrollBox::Slot()[Controls]]
              ]
            ]
          ]
        ];
    }
    TSharedPtr<SButton> PlayButton;
    TSharedPtr<SButton> OptionsBack;
    virtual bool SupportsKeyboardFocus() const override { return true; }
    virtual FReply OnPreviewKeyDown(const FGeometry&, const FKeyEvent& Event) override
    {
        if (bSelectingKey) return FReply::Unhandled();
        const FKey Key=Event.GetKey();
        // Held-button repeats (e.g. B still held from a dodge) must not close the menu.
        if (!Event.IsRepeat() && (Key==EKeys::Escape || Key==EKeys::P || Key==EKeys::Gamepad_Special_Right || Key==EKeys::Gamepad_FaceButton_Right))
        { if (Owner.IsValid() && Owner->IsAtMainMenu()) { Owner->ShowMainMenu(); return FReply::Handled(); } if (bOptionsOpen) { bOptionsOpen = false; return FReply::Handled(); } if (Owner.IsValid()) Owner->ResumeGame(); return FReply::Handled(); }
        return FReply::Unhandled();
    }
private:
    bool bOptionsOpen = false, bSelectingKey = false;
    FString ControlsStatus;
    TArray<TSharedPtr<int32>> OutfitOptions;
    FSlateBrush FrameBrush;
    TWeakObjectPtr<AHellgirlPlayerController> Owner;
};
void AHellgirlPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();
    InputComponent->BindAction("Interact",IE_Pressed,this,&AHellgirlPlayerController::InteractWithHub).bConsumeInput=false;
    InputComponent->BindAction("PauseMenu",IE_Pressed,this,&AHellgirlPlayerController::TogglePauseMenu).bExecuteWhenPaused=true;
    // Fallback for dialogue: if something took focus away from the dialogue box (the editor viewport, a level
    // change), these keys still reach the controller. They never consume input, so play is unaffected.
    for (const FKey& Key : {EKeys::SpaceBar,EKeys::Enter,EKeys::E,EKeys::Gamepad_FaceButton_Bottom,EKeys::LeftMouseButton})
    {
        FInputKeyBinding& Binding=InputComponent->BindKey(Key,IE_Pressed,this,&AHellgirlPlayerController::ContinueDialogueFromGame);
        Binding.bExecuteWhenPaused=true; Binding.bConsumeInput=false;
    }
}
void AHellgirlPlayerController::ContinueDialogueFromGame()
{
    // Ignore the press that opened the box (e.g. E at the merchant).
    if (bDialogueOpen && FPlatformTime::Seconds()-DialoguePageShownAt>.25) ContinueDialogue();
}
void AHellgirlPlayerController::TogglePauseMenu()
{
    if (bMainMenuActive && bMenuOpen) return;
    if (bMenuOpen) { ResumeGame(); return; }
    if (!GetWorld() || !GetWorld()->GetGameViewport() || !SetPause(true)) return;
    if (auto* Fighter=Cast<AArenaFighter>(GetPawn())) Fighter->PrepareForPause();
    FlushPressedKeys();
    bMenuOpen=true;
    EnsureGothicFrame();
    auto Menu=SNew(SHellgirlPause).Owner(this).Frame(FrameTexture);
    PauseWidget=Menu;
    GetWorld()->GetGameViewport()->AddViewportWidgetContent(Menu,100);
    bShowMouseCursor=true;
    FInputModeUIOnly Mode; Mode.SetWidgetToFocus(bMainMenuActive?Menu->OptionsBack:Menu->PlayButton); Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    SetInputMode(Mode);
}
void AHellgirlPlayerController::OpenMainOptions()
{
    ResumeGame();
    TogglePauseMenu();
}
int32 AHellgirlPlayerController::GetSelectedOutfit() const
{
    const auto* Fighter = Cast<AArenaFighter>(GetPawn());
    return Fighter ? Fighter->GetOutfit() : 0;
}

void AHellgirlPlayerController::SelectOutfit(int32 Outfit)
{
    const auto* Wallet=Cast<UHellgirlWallet>(GetGameInstance());
    if (Outfit==4 && (!Wallet || !Wallet->bGoblinQueenOwned)) return;
    auto* Fighter = Cast<AArenaFighter>(GetPawn());
    if (!Fighter || !Fighter->SetOutfit(Outfit)) return;
    GConfig->SetInt(TEXT("HellgirlAppearance"), TEXT("Outfit"), Outfit, GGameUserSettingsIni);
    GConfig->Flush(false, GGameUserSettingsIni);
}

void AHellgirlPlayerController::ResumeGame()
{
    if (!bMenuOpen) return;
    if (PauseWidget.IsValid() && GetWorld() && GetWorld()->GetGameViewport()) GetWorld()->GetGameViewport()->RemoveViewportWidgetContent(PauseWidget.ToSharedRef());
    ConversationId=NAME_None; ConversationPage=0; ConversationSpeakers.Reset(); ConversationLines.Reset();
    ConversationPortraits.Reset(); ConversationLeft.Reset(); ConversationNarration.Reset(); ConversationBlack.Reset();
    PauseWidget.Reset(); bMenuOpen=false; bDialogueOpen=false; DialogueNextHubMenu=-1; DialoguePortrait=nullptr;
    FlushPressedKeys();
    SetPause(false); bShowMouseCursor=false;
    SetInputMode(FInputModeGameOnly());
    FSlateApplication::Get().SetAllUserFocusToGameViewport();
}
void AHellgirlPlayerController::EndPlay(const EEndPlayReason::Type Reason)
{
    if (bMenuOpen) ResumeGame();
    Super::EndPlay(Reason);
}

#include "Containers/Ticker.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/PlatformTime.h"
#include "UnrealClient.h"
void AHellgirlPlayerController::BeginPlay()
{
    Super::BeginPlay();
    RunMainMenuCheck();
    RunDialogueCheck();
    RunStoryCheck();
#if WITH_DEV_AUTOMATION_TESTS
    const bool CheckPhysics=FParse::Param(FCommandLine::Get(),TEXT("HellgirlPhysicsPauseCheck"));
    if (!CheckPhysics && !FParse::Param(FCommandLine::Get(),TEXT("HellgirlPauseCheck"))) return;
    struct FCheckState { int Phase=0; double Started=0; float WorldTime=0; TWeakObjectPtr<AArenaFighter> Corpse; FVector CorpseStart=FVector::ZeroVector; };
    auto State=MakeShared<FCheckState>();
    TWeakObjectPtr<AHellgirlPlayerController> Weak(this);
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Weak,State,CheckPhysics](float)
    {
        if (!Weak.IsValid()) return false;
        auto* PC=Weak.Get();
        auto Fail=[](const TCHAR* Reason) { UE_LOG(LogTemp,Error,TEXT("PAUSE CHECK FAILED: %s"),Reason); FPlatformMisc::RequestExitWithStatus(false,1); };
        if (State->Phase==0)
        {
            if (PC->GetWorld()->GetTimeSeconds()<2.f) return true;
            if (CheckPhysics)
            {
                State->Corpse=PC->GetWorld()->SpawnActor<AArenaFighter>(FVector(-7000,3000,3000),FRotator::ZeroRotator);
                if (!State->Corpse.IsValid()) { Fail(TEXT("Physics pause subject spawn")); return false; }
                State->Corpse->MakeEnemy(1,false);
                State->Corpse->ApplyPhysicsDamage(10000.f,FVector(500,0,100));
                State->CorpseStart=State->Corpse->GetCapsuleComponent()->GetCenterOfMass();
            }
            PC->TogglePauseMenu(); State->WorldTime=PC->GetWorld()->GetTimeSeconds(); State->Started=FPlatformTime::Seconds(); State->Phase=1;
            return true;
        }
        const double Age=FPlatformTime::Seconds()-State->Started;
        if (State->Phase==1 && Age>1)
        {
            if (!PC->bMenuOpen || !PC->IsPaused() || !PC->bShowMouseCursor || !PC->FrameTexture || FMath::Abs(PC->GetWorld()->GetTimeSeconds()-State->WorldTime)>.05f)
            { Fail(TEXT("Overlay, texture, cursor, or frozen world")); return false; }
            auto* Player = Cast<AArenaFighter>(PC->GetPawn());
            if (!Player) { Fail(TEXT("Outfit player missing")); return false; }
            const int32 OriginalOutfit = Player->GetOutfit();
            const float OriginalHealth = Player->Health, OriginalEnergy = Player->Energy;
            // The Goblin Queen outfit must be bought; grant it in memory only (never saved) so the check
            // does not depend on the player's own save.
            auto* Wallet = Cast<UHellgirlWallet>(PC->GetGameInstance());
            const bool bOwnedBefore = Wallet && Wallet->bGoblinQueenOwned;
            if (Wallet) Wallet->bGoblinQueenOwned = true;
            ON_SCOPE_EXIT { if (Wallet) Wallet->bGoblinQueenOwned = bOwnedBefore; };
            for (int32 Step = 0; Step <= HellgirlOutfits::Count; ++Step)
            {
                const int32 Outfit = Step == HellgirlOutfits::Count ? OriginalOutfit : Step;
                if (!Player->SetOutfit(Outfit)) { Fail(TEXT("Native outfit mesh or animation missing")); return false; }
                if (!Player->GetMesh()->GetMaterial(0)) { Fail(TEXT("Outfit color material missing")); return false; }
                PC->SelectOutfit(Outfit);
                int32 SavedOutfit = -1;
                GConfig->GetInt(TEXT("HellgirlAppearance"), TEXT("Outfit"), SavedOutfit, GGameUserSettingsIni);
                if (SavedOutfit != Outfit || Player->GetOutfit() != Outfit || Player->Health != OriginalHealth || Player->Energy != OriginalEnergy)
                { Fail(TEXT("Outfit selection, persistence, or combat state")); return false; }
            }
            if (CheckPhysics && (!State->Corpse.IsValid() || !State->Corpse->IsDeathRagdollActive()
                || !State->Corpse->GetCapsuleComponent()->GetCenterOfMass().Equals(State->CorpseStart,1.f)))
            { Fail(TEXT("Corpse continued simulating during pause")); return false; }
            FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("PausePreview.png"),true,false);
            State->Phase=2;
        }
        if (State->Phase==2 && Age>3)
        {
            PC->ResumeGame(); State->WorldTime=PC->GetWorld()->GetTimeSeconds(); State->Phase=3;
        }
        if (State->Phase==3 && Age>4)
        {
            if (PC->bMenuOpen || PC->IsPaused() || PC->bShowMouseCursor || PC->GetWorld()->GetTimeSeconds()<=State->WorldTime)
            { Fail(TEXT("Resume did not restore running game")); return false; }
            if (CheckPhysics && (!State->Corpse.IsValid() || FVector::Dist(State->CorpseStart,State->Corpse->GetCapsuleComponent()->GetCenterOfMass())<20.f))
            { Fail(TEXT("Corpse did not resume physical movement")); return false; }
            UE_LOG(LogTemp,Display,TEXT("PAUSE CHECK PASSED: frame texture, paused simulation, all seven native outfits and materials, saved selection, unchanged combat state, cursor, resume"));
            FPlatformMisc::RequestExitWithStatus(false,0); return false;
        }
        return true;
    }));
#endif
}

void AHellgirlPlayerController::EnsureGothicFrame()
{
    if (!FrameTexture)
    {
        FImage FrameImage;
        if (FImageUtils::LoadImage(*(FPaths::ProjectContentDir()/TEXT("UI/Pause/GothicFrame.jpg")),FrameImage))
        {
            FrameImage.ChangeFormat(ERawImageFormat::BGRA8,EGammaSpace::sRGB);
            // Treat the reference as an ink mask: paper is transparent, ornament is pale gold.
            for (FColor& Pixel : FrameImage.AsBGRA8())
            {
                const uint8 Opacity=255-FMath::Max3(Pixel.R,Pixel.G,Pixel.B);
                Pixel=FColor(235,216,187,Opacity);
            }
            FrameTexture=FImageUtils::CreateTexture2DFromImage(FrameImage);
        }
    }
}

bool AHellgirlPlayerController::InputKey(const FInputKeyEventArgs& Params)
{
    // A button, or a stick or trigger pushed well past its dead zone, means a controller; a key, a mouse button or
    // a real mouse movement means keyboard and mouse.
    const bool Analog = Params.Key.IsAxis1D() || Params.Key.IsAxis2D();
    if (Params.Key.IsGamepadKey()) { if (!Analog || FMath::Abs(Params.AmountDepressed) > .5f) bUsingGamepad = true; }
    else if (!Analog || FMath::Abs(Params.AmountDepressed) > 2.f) bUsingGamepad = false;
    return Super::InputKey(Params);
}

FString AHellgirlPlayerController::FriendlyKeyName(const FKey& Key)
{
    FString Name = Key.GetDisplayName().ToString();
    if (Key.IsGamepadKey())
    {
        Name.RemoveFromStart(TEXT("Gamepad "));
        Name.RemoveFromEnd(TEXT(" Button"));
    }
    return Name;
}

FString AHellgirlPlayerController::KeysFor(FName Action) const
{
    FString Keys, AnyDevice;
    for (const FInputActionKeyMapping& Mapping : GetDefault<UInputSettings>()->GetActionMappings())
    {
        if (Mapping.ActionName != Action) continue;
        const FString Name = FriendlyKeyName(Mapping.Key);
        AnyDevice += (AnyDevice.IsEmpty() ? TEXT("") : TEXT(" / ")) + Name;
        if (Mapping.Key.IsGamepadKey() == bUsingGamepad) Keys += (Keys.IsEmpty() ? TEXT("") : TEXT(" / ")) + Name;
    }
    return Keys.IsEmpty() ? AnyDevice : Keys;
}
