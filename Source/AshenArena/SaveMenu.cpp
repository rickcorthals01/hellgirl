#include "HellgirlPlayerController.h"
#include "HellgirlWallet.h"
#include "ArenaFighter.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/CoreStyle.h"

class SHellgirlSaveMenu : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SHellgirlSaveMenu) {} SLATE_ARGUMENT(TWeakObjectPtr<AHellgirlPlayerController>,Owner) SLATE_END_ARGS()
    TSharedPtr<SButton> FirstButton;
    void Construct(const FArguments& Args)
    {
        Owner=Args._Owner; Refresh();
        auto Items=SNew(SVerticalBox);
        Items->AddSlot().AutoHeight().Padding(0,0,0,16)[SNew(STextBlock).Text(FText::FromString(TEXT("SAVE / LOAD GAME"))).Font(FCoreStyle::GetDefaultFontStyle("Bold",24))];
        Items->AddSlot().AutoHeight().Padding(0,0,0,16)[SNew(STextBlock).Text(FText::FromString(TEXT("Save in camp or between waves. Loading replaces current progress."))).AutoWrapText(true)];
        for (int32 Slot=1;Slot<=3;++Slot)
        {
            Items->AddSlot().AutoHeight().Padding(0,12)[SNew(STextBlock).Text_Lambda([this,Slot]() { return FText::FromString(FString::Printf(TEXT("SLOT %d / %s"),Slot,*Descriptions[Slot-1])); }).AutoWrapText(true)];
            auto Row=SNew(SHorizontalBox);
            for (bool Loading : {false,true})
            {
                TSharedPtr<SButton> Button;
                Row->AddSlot().FillWidth(1).Padding(4)
                [SAssignNew(Button,SButton).HAlign(HAlign_Center).ContentPadding(12)
                    .IsEnabled_Lambda([this,Slot,Loading]() { return Loading ? Descriptions[Slot-1]!=TEXT("Empty") : (Owner.IsValid() && !Owner->IsAtMainMenu()); })
                    .OnClicked_Lambda([this,Slot,Loading]() {
                        auto* Wallet=Owner.IsValid()?Cast<UHellgirlWallet>(Owner->GetGameInstance()):nullptr;
                        if (!Wallet) return FReply::Handled();
                        const int32 Action=Slot+(Loading?3:0);
                        if ((Loading || Descriptions[Slot-1]!=TEXT("Empty")) && Armed!=Action)
                        { Armed=Action; Status=Loading?TEXT("Press CONFIRM LOAD to replace current progress."):TEXT("Press CONFIRM SAVE to overwrite this slot."); return FReply::Handled(); }
                        Armed=0;
                        if (Loading) { if (!Wallet->LoadSlot(Slot)) Status=TEXT("Could not load this save. Current progress is unchanged."); }
                        else { Status=Wallet->SaveSlot(Slot); Refresh(); }
                        return FReply::Handled(); })
                    [SNew(STextBlock).Text_Lambda([this,Slot,Loading]() { return FText::FromString(Armed==Slot+(Loading?3:0)?(Loading?TEXT("CONFIRM LOAD"):TEXT("CONFIRM SAVE")):(Loading?TEXT("LOAD"):TEXT("SAVE"))); })]];
                if (!FirstButton) FirstButton=Button;
            }
            Items->AddSlot().AutoHeight()[Row];
        }
        Items->AddSlot().AutoHeight().Padding(0,16)[SNew(STextBlock).Text_Lambda([this]() { return FText::FromString(Status); }).AutoWrapText(true)];
        Items->AddSlot().AutoHeight()[SNew(SButton).HAlign(HAlign_Center).ContentPadding(12).OnClicked_Lambda([this]() { if (Owner.IsValid()) { if (Owner->IsAtMainMenu()) Owner->ShowMainMenu(); else { Owner->ResumeGame(); Owner->TogglePauseMenu(); } } return FReply::Handled(); })[SNew(STextBlock).Text(FText::FromString(TEXT("BACK")))]];
        ChildSlot[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Owner.IsValid() && Owner->IsAtMainMenu()?FLinearColor::Black:FLinearColor(0,0,0,.65f)).HAlign(HAlign_Center).VAlign(VAlign_Center)[SNew(SBox).WidthOverride(600)[SNew(SBorder).Padding(28).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(.025f,.02f,.03f,.95f))[Items]]]];
    }
    virtual FReply OnPreviewKeyDown(const FGeometry&,const FKeyEvent& Event) override
    {
        if (Event.GetKey()==EKeys::Escape || Event.GetKey()==EKeys::Gamepad_FaceButton_Right)
        { if (Owner.IsValid()) { if (Owner->IsAtMainMenu()) Owner->ShowMainMenu(); else { Owner->ResumeGame(); Owner->TogglePauseMenu(); } } return FReply::Handled(); }
        return FReply::Unhandled();
    }
private:
    TWeakObjectPtr<AHellgirlPlayerController> Owner;
    FString Descriptions[3],Status;
    int32 Armed=0;
    void Refresh() { if (auto* Wallet=Owner.IsValid()?Cast<UHellgirlWallet>(Owner->GetGameInstance()):nullptr) for (int32 I=0;I<3;++I) Descriptions[I]=Wallet->DescribeSlot(I+1); }
};

void AHellgirlPlayerController::OpenSaveMenu()
{
    if (!GetWorld()->GetGameViewport()) return;
    ResumeGame();
    if (!SetPause(true)) return;
    if (auto* Hero=Cast<AArenaFighter>(GetPawn())) Hero->PrepareForPause();
    FlushPressedKeys(); bMenuOpen=true; bShowMouseCursor=true;
    auto Menu=SNew(SHellgirlSaveMenu).Owner(this); PauseWidget=Menu;
    GetWorld()->GetGameViewport()->AddViewportWidgetContent(Menu,100);
    FInputModeUIOnly Mode; Mode.SetWidgetToFocus(Menu->FirstButton); Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock); SetInputMode(Mode);
}
