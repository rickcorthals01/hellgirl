#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "HellgirlPlayerController.generated.h"
class SWidget;
class UTexture2D;
UCLASS()
class ASHENARENA_API AHellgirlPlayerController : public APlayerController
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
    void RunStoryCheck();
    void SelectOutfit(int32 Outfit);
    int32 GetSelectedOutfit() const;
    bool IsPauseMenuOpen() const { return bMenuOpen; }
private:
    bool bMainMenuActive = false;
    FName ConversationId;
    int32 ConversationPage=0;
    TArray<FText> ConversationSpeakers, ConversationLines;
    void PresentDialoguePage(FText Speaker,FText Line,UTexture2D* Portrait);
    bool bMenuOpen = false;
    bool bDialogueOpen = false;
    int32 DialogueNextHubMenu = -1;
    UPROPERTY() TObjectPtr<UTexture2D> DialoguePortrait;
    TSharedPtr<SWidget> PauseWidget;
    UPROPERTY() TObjectPtr<UTexture2D> FrameTexture;
};
