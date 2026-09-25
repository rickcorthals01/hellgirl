#pragma once
#include "CoreMinimal.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/DateTime.h"
#include "HAL/PlatformTime.h"
#include "Progress/CampaignProgress.h"

// Achievements (the full system comes later): each is unlocked once and kept for the whole profile, not per save
// slot and not cleared by a new game. Stored as [HellgirlAchievements] Id=date in the user settings; automated runs
// keep them in memory only.
namespace HellgirlAchievements
{
inline const TCHAR* Section = TEXT("HellgirlAchievements");
inline const TCHAR* EndlessGoblins50 = TEXT("EndlessGoblins50");   // cleared wave 50 of endless Goblins
inline const TCHAR* GoblinQueenOutfit = TEXT("GoblinQueenOutfit"); // bought the Goblin Queen outfit

struct FInfo { const TCHAR* Id; const TCHAR* Title; };
inline const TArray<FInfo>& All()
{
    static const TArray<FInfo> List = {
        {EndlessGoblins50, TEXT("Beaten wave 50 · Goblins")},
        {GoblinQueenOutfit, TEXT("Bought the Goblin Queen outfit")}};
    return List;
}
inline const TCHAR* Title(const TCHAR* Id)
{
    for (const FInfo& A : All()) if (FCString::Strcmp(A.Id, Id) == 0) return A.Title;
    return Id;
}
inline TSet<FString>& Session() { static TSet<FString> Unlocked; return Unlocked; }
// The latest unlock, for a short HUD notice.
inline FString& LastUnlocked() { static FString Title; return Title; }
inline double& LastUnlockedAt() { static double At = -100.0; return At; }

inline bool Has(const TCHAR* Id)
{
    if (HellgirlProgress::IsAutomated()) return Session().Contains(Id);
    FString Date;
    return GConfig->GetString(Section, Id, Date, GGameUserSettingsIni) && !Date.IsEmpty();
}
// Unlocks once; bAnnounce shows the HUD notice (off for progress made before achievements existed).
inline bool Unlock(const TCHAR* Id, bool bAnnounce = true)
{
    if (Has(Id)) return false;
    if (HellgirlProgress::IsAutomated()) Session().Add(Id);
    else
    {
        GConfig->SetString(Section, Id, *FDateTime::Now().ToString(TEXT("%Y-%m-%d %H:%M")), GGameUserSettingsIni);
        GConfig->Flush(false, GGameUserSettingsIni);
    }
    UE_LOG(LogTemp, Display, TEXT("Achievement unlocked: %s"), Title(Id));
    if (bAnnounce) { LastUnlocked() = Title(Id); LastUnlockedAt() = FPlatformTime::Seconds(); }
    return true;
}
}
