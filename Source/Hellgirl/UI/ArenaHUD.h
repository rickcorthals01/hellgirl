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
};
