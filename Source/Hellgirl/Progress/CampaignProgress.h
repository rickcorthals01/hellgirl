#pragma once
#include "CoreMinimal.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

// One-time story unlocks for World I, kept beside UnlockedLevel in [HellgirlCampaign] (and copied into save slots).
namespace HellgirlProgress
{
    inline const TCHAR* Section = TEXT("HellgirlCampaign");
    // Every flag, so a new game can clear them and a save slot can carry them.
    inline const TArray<FString>& AllFlags()
    {
        static const TArray<FString> Names = {
            TEXT("CampSetUp"),          // 01.5: "This place looks safe.." played on the first visit to camp
            TEXT("EndlessGoblins"),     // 02.5: endless mode unlocked (and its notice shown)
            TEXT("UltimatesUnlocked"),  // 03: the Goblin Queen at 30% health
            TEXT("GoblinFollowed"),     // 03.5: "A goblin has followed Hellgirl to her camp."
            TEXT("ShopUnlocked")};      // 03.5: first talk with the goblin
        return Names;
    }
    // Automated runs (-Hellgirl...) start with no flags and keep them in memory, never touching the player's progress.
    inline bool IsAutomated() { return FString(FCommandLine::Get()).Contains(TEXT("-Hellgirl")); }
    inline TSet<FString>& SessionFlags() { static TSet<FString> Flags; return Flags; }
    inline bool Flag(const TCHAR* Name)
    {
        if (IsAutomated()) return SessionFlags().Contains(Name);
        bool Value = false;
        GConfig->GetBool(Section, Name, Value, GGameUserSettingsIni);
        return Value;
    }
    inline void SetFlag(const TCHAR* Name, bool Value = true)
    {
        if (IsAutomated()) { if (Value) SessionFlags().Add(Name); else SessionFlags().Remove(Name); return; }
        GConfig->SetBool(Section, Name, Value, GGameUserSettingsIni);
        GConfig->Flush(false, GGameUserSettingsIni);
    }
    // Automated checks (other than the story check) keep every move available.
    inline bool IsCheckRun()
    {
        return IsAutomated() && !FParse::Param(FCommandLine::Get(), TEXT("HellgirlStoryCheck"));
    }
    inline bool UltimatesUnlocked()
    {
        if (IsCheckRun()) return true;
        if (IsAutomated()) return Flag(TEXT("UltimatesUnlocked"));
        // Progress from before this unlock existed: the Queen was already beaten.
        int32 Unlocked = 1;
        GConfig->GetInt(Section, TEXT("UnlockedLevel"), Unlocked, GGameUserSettingsIni);
        return Unlocked >= 4 || Flag(TEXT("UltimatesUnlocked"));
    }
    inline int32& SessionBest() { static int32 Best = 0; return Best; }
    inline int32 EndlessBest()
    {
        if (IsAutomated()) return SessionBest();
        int32 Best = 0;
        GConfig->GetInt(Section, TEXT("EndlessGoblinsBest"), Best, GGameUserSettingsIni);
        return Best;
    }
    inline void RecordEndless(int32 Wave)
    {
        if (Wave <= EndlessBest()) return;
        if (IsAutomated()) { SessionBest() = Wave; return; }
        GConfig->SetInt(Section, TEXT("EndlessGoblinsBest"), Wave, GGameUserSettingsIni);
        GConfig->Flush(false, GGameUserSettingsIni);
    }
}
