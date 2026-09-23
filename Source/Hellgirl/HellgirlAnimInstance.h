#pragma once
#include "Animation/AnimSingleNodeInstance.h"
#include "HellgirlAnimInstance.generated.h"

// Keeps combat-clock playback while adding restrained secondary motion after pose evaluation.
UCLASS(Transient)
class HELLGIRL_API UHellgirlAnimInstance : public UAnimSingleNodeInstance
{
    GENERATED_BODY()
protected:
    virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
};
