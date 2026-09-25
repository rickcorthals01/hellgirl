#pragma once
#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "ArenaHUD.generated.h"
UCLASS()
class HELLGIRL_API AArenaHUD : public AHUD
{
    GENERATED_BODY()
public:
    virtual void DrawHUD() override;
private:
    // Move callouts ("RIGHT PUNCH - HIT", "THORNS!") pop up when they change and fade out.
    FString ShownLabel;
    double LabelShownAt = -100.0;
    // The combo multiplier pops when it goes up a tier.
    int32 ShownComboTier = 0;
    double ComboTierAt = -100.0;
};
