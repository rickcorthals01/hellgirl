#pragma once
#include "CoreMinimal.h"

namespace HellgirlOutfits
{
    // Stable indices are persisted in the player's local settings.
    inline constexpr int32 Count = 7;
    inline const TCHAR* Folder(int32 Index)
    {
        static const TCHAR* Names[] = {TEXT("Rags"), TEXT("SuccubusArmor"), TEXT("Ghost"), TEXT("Frog"), TEXT("GoblinQueen"), TEXT("ImpMother"), TEXT("Rat")};
        return Names[FMath::Clamp(Index, 0, Count - 1)];
    }
    inline const TCHAR* Label(int32 Index)
    {
        static const TCHAR* Labels[] = {TEXT("RAGS"), TEXT("SUCCUBUS ARMOR"), TEXT("GHOST"), TEXT("FROG"), TEXT("GOBLIN QUEEN"), TEXT("IMP MOTHER"), TEXT("RAT")};
        return Labels[FMath::Clamp(Index, 0, Count - 1)];
    }
}
