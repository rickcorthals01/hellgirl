#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "HellgirlPlayerController.generated.h"
class SWidget;
class UTexture2D;
UCLASS()
class HELLGIRL_API AHellgirlPlayerController : public APlayerController
{
    GENERATED_BODY()
public:
    virtual void BeginPlay() override;
    virtual void SetupInputComponent() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    void TogglePauseMenu();
    void InteractWithHub();
    void OpenHubMenu(int32 Kind);
    void OpenSaveMenu();
    void ShowMainMenu();
    void PlayFromMainMenu();
    void OpenMainOptions();
    void RunMainMenuCheck();
    bool IsAtMainMenu() const { return bMainMenuActive; }
    void ResumeGame();
    UFUNCTION(BlueprintCallable, Category="Dialogue") void ShowDialogue(FText Speaker, FText Line, UTexture2D* Portrait = nullptr);
    UFUNCTION(BlueprintCallable, Category="Dialogue") void CloseDialogue();
    void ContinueDialogue();
    bool IsDialogueOpen() const { return bDialogueOpen; }
    void EnsureGothicFrame();
    void RunDialogueCheck();
    bool ShowConversation(FName Id);
    FName GetConversation() const { return ConversationId; }
    int32 GetConversationPage() const { return ConversationPage; }
    // The portrait shown on the current conversation page (null when none).
    UTexture2D* GetConversationPortrait() const { return ConversationPortraits.IsValidIndex(ConversationPage) ? ConversationPortraits[ConversationPage].Get() : nullptr; }
    // A black screen that fades to the game over Seconds (real time, so it runs during dialogue).
    void FadeFromBlack(float Seconds);
    void RunStoryCheck();
    void SelectOutfit(int32 Outfit);
    int32 GetSelectedOutfit() const;
    bool IsPauseMenuOpen() const { return bMenuOpen; }
private:
    bool bMainMenuActive = false;
    FName ConversationId;
    int32 ConversationPage=0;
    TArray<FText> ConversationSpeakers, ConversationLines;
    UPROPERTY() TArray<TObjectPtr<UTexture2D>> ConversationPortraits;
    TArray<bool> ConversationLeft, ConversationNarration;
    // Portrait on the left for Hellgirl, on the right for everyone else; narration is centred text on black.
    void PresentDialoguePage(FText Speaker,FText Line,UTexture2D* Portrait,bool bPortraitLeft=false,bool bNarration=false);
    UTexture2D* FindPortrait(const FString& Speaker,const FString& Mood) const;
    TSharedPtr<SWidget> FadeWidget;
    bool bMenuOpen = false;
    bool bDialogueOpen = false;
    int32 DialogueNextHubMenu = -1;
    UPROPERTY() TObjectPtr<UTexture2D> DialoguePortrait;
    TSharedPtr<SWidget> PauseWidget;
    UPROPERTY() TObjectPtr<UTexture2D> FrameTexture;
};
