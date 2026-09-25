#include "UI/HellgirlPlayerController.h"
#include "Fighter/ArenaFighter.h"
#include "Fighter/HellgirlOutfits.h"
#include "Levels/ArenaGameMode.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/CoreStyle.h"
#include "Containers/Ticker.h"
#include "HAL/PlatformTime.h"

#include "Widgets/SLeafWidget.h"
#include "Rendering/DrawElements.h"
class SDialogueFrame : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SDialogueFrame) {} SLATE_ARGUMENT(UTexture2D*,Frame) SLATE_END_ARGS()
    void Construct(const FArguments& Args)
    {
        const FVector2f UVMin[]={FVector2f(.035f,.035f),FVector2f(.55f,.035f),FVector2f(.035f,.81f),FVector2f(.55f,.81f)};
        for (int I=0;I<4;++I)
        {
            Corners[I].SetResourceObject(Args._Frame); Corners[I].DrawAs=ESlateBrushDrawType::Image;
            Corners[I].SetUVRegion(FBox2f(UVMin[I],UVMin[I]+FVector2f(.415f,.155f)));
        }
    }
    virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(1,1); }
    virtual int32 OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle&,bool) const override
    {
        const FVector2D Size=G.GetLocalSize(); const float CW=Size.X<350?62.f:110.f,CH=CW*.6f;
        const FVector2D P[]={FVector2D(0,0),FVector2D(Size.X-CW,0),FVector2D(0,Size.Y-CH),FVector2D(Size.X-CW,Size.Y-CH)};
        const auto* Line=FCoreStyle::Get().GetBrush("WhiteBrush"); const FLinearColor Tint(.8f,.7f,.53f,.65f);
        for (float Y:{8.f,static_cast<float>(Size.Y)-9.f})
            FSlateDrawElement::MakeBox(Out,Layer,G.ToPaintGeometry(FVector2D(Size.X-2*CW,1),FSlateLayoutTransform(FVector2D(CW,Y))),Line,ESlateDrawEffect::None,Tint);
        for (float X:{8.f,static_cast<float>(Size.X)-9.f})
            FSlateDrawElement::MakeBox(Out,Layer,G.ToPaintGeometry(FVector2D(1,Size.Y-2*CH),FSlateLayoutTransform(FVector2D(X,CH))),Line,ESlateDrawEffect::None,Tint);
        for (int I=0;I<4;++I) FSlateDrawElement::MakeBox(Out,Layer+1,G.ToPaintGeometry(FVector2D(CW,CH),FSlateLayoutTransform(P[I])),&Corners[I],ESlateDrawEffect::None,FLinearColor(1,1,1,.8f));
        return Layer+1;
    }
private:
    FSlateBrush Corners[4];
};

// The dialogue box runs along the bottom of the screen; the speaker's portrait stands on top of it,
// on the left for Hellgirl and on the right for everyone else. Narration is centred text on a black screen.
class SGothicDialogue : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SGothicDialogue) {} SLATE_ARGUMENT(TWeakObjectPtr<AHellgirlPlayerController>,Owner)
        SLATE_ARGUMENT(UTexture2D*,Frame) SLATE_ARGUMENT(UTexture2D*,Portrait) SLATE_ARGUMENT(bool,PortraitLeft) SLATE_ARGUMENT(bool,Narration) SLATE_ARGUMENT(bool,Backdrop)
        SLATE_ARGUMENT(FText,Speaker) SLATE_ARGUMENT(FText,Line) SLATE_END_ARGS()
    TSharedPtr<SButton> ContinueButton;
    void Construct(const FArguments& Args)
    {
        Owner=Args._Owner;
        PortraitBrush.SetResourceObject(Args._Portrait); PortraitBrush.ImageSize=FVector2D(420,420); PortraitBrush.DrawAs=ESlateBrushDrawType::Image;
        const FLinearColor Ink(.95f,.89f,.77f),Shadow(.012f,.008f,.023f,.62f);
        auto Continue=SAssignNew(ContinueButton,SButton).ContentPadding(FMargin(12,3)).ButtonColorAndOpacity(FLinearColor(.09f,.055f,.11f,.6f))
            .OnClicked_Lambda([this]() { if (Owner.IsValid()) Owner->ContinueDialogue(); return FReply::Handled(); })
            [SNew(STextBlock).Text(FText::FromString(TEXT("Continue  /  A"))).Font(FCoreStyle::GetDefaultFontStyle("Regular",12)).ColorAndOpacity(Ink)];
        if (Args._Narration)
        {
            ChildSlot[SNew(SOverlay)
                + SOverlay::Slot()[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor::Black)]
                + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(60)
                  [SNew(STextBlock).Text(Args._Line).Font(FCoreStyle::GetDefaultFontStyle("Italic",34)).ColorAndOpacity(Ink).Justification(ETextJustify::Center).AutoWrapText(true)]
                + SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(40)[Continue]];
            return;
        }
        const bool HasPortrait=Args._Portrait!=nullptr, HasName=!Args._Speaker.IsEmpty();
        const EHorizontalAlignment Side=Args._PortraitLeft?HAlign_Left:HAlign_Right;
        // Style=Black: the ordinary box and portrait, over a black screen.
        ChildSlot[SNew(SOverlay)
          + SOverlay::Slot()[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor::Black).Visibility(Args._Backdrop?EVisibility::HitTestInvisible:EVisibility::Collapsed)]
          + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(FMargin(24,0,24,20))
        [SNew(SScaleBox).Stretch(EStretch::ScaleToFit).StretchDirection(EStretchDirection::DownOnly)
          [SNew(SBox).WidthOverride(1500).HeightOverride(560)
            [SNew(SOverlay)
              // The portrait first, so the box in front covers the bottom of the bust.
              + SOverlay::Slot().HAlign(Side).VAlign(VAlign_Top).Padding(FMargin(70,0,70,0))
                [SNew(SBox).WidthOverride(420).HeightOverride(420).Visibility(HasPortrait?EVisibility::HitTestInvisible:EVisibility::Collapsed)
                  [SNew(SImage).Image(&PortraitBrush)]]
              + SOverlay::Slot().VAlign(VAlign_Bottom)
                [SNew(SBox).HeightOverride(220)
                  [SNew(SOverlay)
                    + SOverlay::Slot()[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Shadow)]
                    + SOverlay::Slot()[SNew(SDialogueFrame).Frame(Args._Frame).Visibility(EVisibility::HitTestInvisible)]
                    + SOverlay::Slot().Padding(120,26,120,22)
                      [SNew(SVerticalBox)
                        + SVerticalBox::Slot().AutoHeight().HAlign(Side).Padding(0,0,0,10)
                          [SNew(STextBlock).Text(Args._Speaker).Font(FCoreStyle::GetDefaultFontStyle("Bold",24)).ColorAndOpacity(FLinearColor(.95f,.72f,.32f))
                            .Visibility(HasName?EVisibility::Visible:EVisibility::Collapsed)]
                        + SVerticalBox::Slot().FillHeight(1)
                          [SNew(SScrollBox)
                            + SScrollBox::Slot()[SNew(STextBlock).Text(Args._Line).WrapTextAt(1220).Font(FCoreStyle::GetDefaultFontStyle("Regular",24)).ColorAndOpacity(Ink)]]
                        + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(0,6,0,0)[Continue]]]]]]]];
    }
    virtual FReply OnPreviewKeyDown(const FGeometry&,const FKeyEvent& Event) override
    {
        if (Event.IsRepeat()) return FReply::Handled();
        const FKey Key=Event.GetKey();
        if (Key==EKeys::Escape || Key==EKeys::Gamepad_FaceButton_Right || Key==EKeys::Gamepad_Special_Right)
        { if (Owner.IsValid()) Owner->CloseDialogue(); return FReply::Handled(); }
        if (Key==EKeys::Enter || Key==EKeys::SpaceBar || Key==EKeys::E || Key==EKeys::Gamepad_FaceButton_Bottom)
        { if (Owner.IsValid()) Owner->ContinueDialogue(); return FReply::Handled(); }
        return FReply::Unhandled();
    }
private:
    FSlateBrush PortraitBrush;
    TWeakObjectPtr<AHellgirlPlayerController> Owner;
};
void AHellgirlPlayerController::ShowDialogue(FText Speaker,FText Line,UTexture2D* Portrait)
{
    if (bMenuOpen || !GetWorld() || !GetWorld()->GetGameViewport() || !SetPause(true)) return;
    if (auto* Fighter=Cast<AArenaFighter>(GetPawn())) Fighter->PrepareForPause();
    FlushPressedKeys(); EnsureGothicFrame();
    bMenuOpen=bDialogueOpen=true; DialoguePortrait=Portrait; DialogueNextHubMenu=-1;
    ConversationPortraits.Reset(); ConversationLeft.Reset(); ConversationNarration.Reset(); ConversationBlack.Reset();
    PresentDialoguePage(Speaker,Line,Portrait);
}
void AHellgirlPlayerController::PresentDialoguePage(FText Speaker,FText Line,UTexture2D* Portrait,bool bPortraitLeft,bool bNarration,bool bBlack)
{
    if (PauseWidget.IsValid()) GetWorld()->GetGameViewport()->RemoveViewportWidgetContent(PauseWidget.ToSharedRef());
    auto Widget=SNew(SGothicDialogue).Owner(this).Frame(FrameTexture).Portrait(Portrait).PortraitLeft(bPortraitLeft).Narration(bNarration).Backdrop(bBlack).Speaker(Speaker).Line(Line);
    PauseWidget=Widget; GetWorld()->GetGameViewport()->AddViewportWidgetContent(Widget,110);
    DialoguePageShownAt=FPlatformTime::Seconds();
    // Game-and-UI, so the controller's fallback keys still work if the box loses focus.
    bShowMouseCursor=true; FInputModeGameAndUI Mode; Mode.SetWidgetToFocus(Widget->ContinueButton); Mode.SetHideCursorDuringCapture(false);
    Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock); SetInputMode(Mode);
    // A new widget can only take focus once it has been laid out; focus it again shortly after (as the menus do).
    TWeakObjectPtr<AHellgirlPlayerController> Weak(this);
    TWeakPtr<SGothicDialogue> WeakWidget(Widget);
    for (const float Delay : {.05f,.3f})
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Weak,WeakWidget](float)
        {
            const auto Box=WeakWidget.Pin();
            if (Weak.IsValid() && Box.IsValid() && Weak->IsDialogueOpen() && Weak->PauseWidget==Box && Box->ContinueButton.IsValid())
            {
                FSlateApplication::Get().SetKeyboardFocus(Box->ContinueButton,EFocusCause::SetDirectly);
                FSlateApplication::Get().SetAllUserFocus(Box->ContinueButton,EFocusCause::SetDirectly);
                Weak->bShowMouseCursor=true;
            }
            return false;
        }),Delay);
}
void AHellgirlPlayerController::CloseDialogue() { if (bDialogueOpen && ConversationId.IsNone()) ResumeGame(); }
void AHellgirlPlayerController::ContinueDialogue()
{
    if (!bDialogueOpen) return;
    if (!ConversationId.IsNone() && ConversationLines.IsValidIndex(ConversationPage+1))
    {
        ++ConversationPage;
        PresentDialoguePage(ConversationSpeakers[ConversationPage],ConversationLines[ConversationPage],ConversationPortraits[ConversationPage],
            ConversationLeft[ConversationPage],ConversationNarration[ConversationPage],ConversationBlack[ConversationPage]);
        return;
    }
    const FName Finished=ConversationId;
    const int32 NextMenu=DialogueNextHubMenu;
    ResumeGame();
    if (NextMenu>=0) OpenHubMenu(NextMenu);
    if (!Finished.IsNone()) if (auto* GM=Cast<AArenaGameMode>(UGameplayStatics::GetGameMode(this))) GM->StoryFinished(Finished);
}

// Hellgirl's portraits follow her outfit (falling back to Rags); other speakers use a set named after them,
// e.g. "Goblin Queen" -> /Game/Dialogue/Portraits/GoblinQueen/T_GoblinQueen_Angry. Missing art shows no portrait.
UTexture2D* AHellgirlPlayerController::FindPortrait(const FString& Speaker,const FString& Mood) const
{
    if (Speaker.IsEmpty() || Mood.Equals(TEXT("none"),ESearchCase::IgnoreCase)) return nullptr;
    const FString Wanted=Mood.IsEmpty()?TEXT("Neutral"):Mood;
    TArray<FString> Sets;
    if (Speaker.Equals(TEXT("Hellgirl"),ESearchCase::IgnoreCase)) { Sets.Add(HellgirlOutfits::Folder(GetSelectedOutfit())); Sets.AddUnique(TEXT("Rags")); }
    else Sets.Add(Speaker.Replace(TEXT(" "),TEXT("")));
    for (const FString& Set : Sets)
        for (const FString& Try : {Wanted,FString(TEXT("Neutral"))})
            if (auto* Texture=LoadObject<UTexture2D>(nullptr,*FString::Printf(TEXT("/Game/Dialogue/Portraits/%s/T_%s_%s.T_%s_%s"),*Set,*Set,*Try,*Set,*Try)))
                return Texture;
    return nullptr;
}

void AHellgirlPlayerController::FadeFromBlack(float Seconds)
{
    if (!GetWorld() || !GetWorld()->GetGameViewport()) return;
    if (FadeWidget.IsValid()) GetWorld()->GetGameViewport()->RemoveViewportWidgetContent(FadeWidget.ToSharedRef());
    const double Start=FPlatformTime::Seconds();
    FadeWidget=SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).Visibility(EVisibility::HitTestInvisible)
        .BorderBackgroundColor_Lambda([Start,Seconds]() { return FLinearColor(0,0,0,FMath::Clamp(1.f-static_cast<float>(FPlatformTime::Seconds()-Start)/FMath::Max(Seconds,.01f),0.f,1.f)); });
    GetWorld()->GetGameViewport()->AddViewportWidgetContent(FadeWidget.ToSharedRef(),105);
    TWeakObjectPtr<AHellgirlPlayerController> Weak(this);
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Weak](float)
    {
        if (Weak.IsValid() && Weak->FadeWidget.IsValid() && Weak->GetWorld() && Weak->GetWorld()->GetGameViewport())
            Weak->GetWorld()->GetGameViewport()->RemoveViewportWidgetContent(Weak->FadeWidget.ToSharedRef());
        if (Weak.IsValid()) Weak->FadeWidget.Reset();
        return false;
    }),Seconds+.1f);
}

#include "Containers/Ticker.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "HAL/PlatformTime.h"
#include "UnrealClient.h"
void AHellgirlPlayerController::RunDialogueCheck()
{
#if WITH_DEV_AUTOMATION_TESTS
    if (!FParse::Param(FCommandLine::Get(),TEXT("HellgirlDialogueCheck"))) return;
    struct FCheck { int32 Phase=0; double Started=0; float WorldTime=0; };
    auto State=MakeShared<FCheck>(); TWeakObjectPtr<AHellgirlPlayerController> Weak(this);
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Weak,State](float) {
        if (!Weak.IsValid()) return false;
        auto* PC=Weak.Get();
        auto Fail=[](const TCHAR* Why) { UE_LOG(LogTemp,Error,TEXT("DIALOGUE CHECK FAILED: %s"),Why); FPlatformMisc::RequestExitWithStatus(false,1); };
        if (State->Phase==0)
        {
            if (!PC->GetPawn() || PC->GetWorld()->GetTimeSeconds()<1.f) return true;
            PC->GetPawn()->SetActorLocation(FVector(-100,440,110)); PC->SetControlRotation(FRotator(-12,90,0));
            PC->InteractWithHub();
            if (!PC->IsDialogueOpen() || !PC->FrameTexture || !PC->IsPaused() || !PC->bShowMouseCursor) { Fail(TEXT("Merchant dialogue, frame, pause or cursor")); return false; }
            State->WorldTime=PC->GetWorld()->GetTimeSeconds(); State->Started=FPlatformTime::Seconds(); State->Phase=1;
        }
        const double Age=FPlatformTime::Seconds()-State->Started;
        if (State->Phase==1 && Age>1.5)
        {
            if (FMath::Abs(PC->GetWorld()->GetTimeSeconds()-State->WorldTime)>.05f) { Fail(TEXT("World was not paused")); return false; }
            FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/Dialogue.png"),true,false); State->Phase=2;
        }
        if (State->Phase==2 && Age>2.5)
        {
            PC->ContinueDialogue();
            if (PC->IsDialogueOpen() || !PC->IsPauseMenuOpen()) { Fail(TEXT("Continue did not open shop")); return false; }
            PC->ResumeGame();
            PC->ShowDialogue(FText::FromString(TEXT("Hellgirl")),FText::FromString(TEXT("A quiet moment.")));
            PC->CloseDialogue();
            if (PC->IsPauseMenuOpen() || PC->IsPaused() || PC->bShowMouseCursor) { Fail(TEXT("Dismiss did not restore play")); return false; }
            UE_LOG(LogTemp,Display,TEXT("DIALOGUE CHECK PASSED: merchant greeting, transparent frame, pause, continue to shop and dismissal"));
            FPlatformMisc::RequestExitWithStatus(false,0); return false;
        }
        return true;
    }));
#endif
}

// A conversation is a section of Content/Dialogue/LevelOne.ini with numbered pages:
//   SpeakerN = who talks (empty: a box without a name)
//   LineN    = what they say
//   MoodN    = portrait mood (Neutral, Angry, Surprised, Headache, Quiet, Smirk, EvilSmirk, Laugh, Hurt...; "none" hides it)
//   StyleN   = Narration for centred text on a black screen
bool AHellgirlPlayerController::ShowConversation(FName Id)
{
    if (bMenuOpen) return false;
    FConfigFile Script;
    Script.Read(FPaths::ProjectContentDir()/TEXT("Dialogue/LevelOne.ini"));
    TArray<FText> Speakers,Lines;
    TArray<TObjectPtr<UTexture2D>> Portraits;
    TArray<bool> Left,Narration,Black;
    for (int32 I=0;;++I)
    {
        FString Speaker,Line,Mood,Style;
        if (!Script.GetString(*Id.ToString(),*FString::Printf(TEXT("Line%d"),I),Line)) break;
        Script.GetString(*Id.ToString(),*FString::Printf(TEXT("Speaker%d"),I),Speaker);
        Script.GetString(*Id.ToString(),*FString::Printf(TEXT("Mood%d"),I),Mood);
        Script.GetString(*Id.ToString(),*FString::Printf(TEXT("Style%d"),I),Style);
        const bool IsNarration=Style.Equals(TEXT("Narration"),ESearchCase::IgnoreCase);
        Speakers.Add(FText::FromString(Speaker)); Lines.Add(FText::FromString(Line));
        Portraits.Add(IsNarration?nullptr:FindPortrait(Speaker,Mood));
        Left.Add(Speaker.Equals(TEXT("Hellgirl"),ESearchCase::IgnoreCase)); Narration.Add(IsNarration); Black.Add(Style.Equals(TEXT("Black"),ESearchCase::IgnoreCase));
    }
    if (Lines.IsEmpty()) return false;
    ShowDialogue(Speakers[0],Lines[0]);
    if (!bDialogueOpen) return false;
    ConversationId=Id; ConversationPage=0; ConversationSpeakers=MoveTemp(Speakers); ConversationLines=MoveTemp(Lines);
    ConversationPortraits=MoveTemp(Portraits); ConversationLeft=MoveTemp(Left); ConversationNarration=MoveTemp(Narration); ConversationBlack=MoveTemp(Black);
    PresentDialoguePage(ConversationSpeakers[0],ConversationLines[0],ConversationPortraits[0],ConversationLeft[0],ConversationNarration[0],ConversationBlack[0]);
    return true;
}
