#include "Fighter/ArenaFighter.h"
#include "Rules/FistCombatRules.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "HAL/PlatformMisc.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

// Visual check for combat clips (needs a GPU, so it is not part of Tests/run-checks.ps1):
//   UnrealEditor.exe Hellgirl.uproject /Engine/Maps/Entry?ForestHub=1 -game -windowed -ResX=900 -ResY=900 -HellgirlAttackPreview
// Holds every player attack at wind-up, contact and follow-through and saves screenshots to
// Saved/Screenshots/Attacks/<Clip>_<0|1|2>.png. The attack state is set directly, so no damage is dealt.
void AArenaFighter::RunAttackAnimationPreview(float Dt)
{
#if WITH_DEV_AUTOMATION_TESTS
    if (bEnemy || UGameplayStatics::GetPlayerPawn(this, 0) != this || !FParse::Param(FCommandLine::Get(), TEXT("HellgirlAttackPreview"))) return;
    struct FPreviewMove { const TCHAR* Clip; bool Heavy; int32 Combo; bool Air; bool AfterDodge; bool Sword; };
    static const FPreviewMove Moves[] = {
        {TEXT("RightPunch"), false, 0, false, false, false}, {TEXT("LeftPunch"), false, 1, false, false, false},
        {TEXT("DoubleJab"), false, 3, false, false, false}, {TEXT("RightKick"), true, 0, false, false, false},
        {TEXT("LeftKick"), true, 1, false, false, false}, {TEXT("LegSweep"), true, 3, false, false, false},
        {TEXT("Headbutt"), false, 0, false, true, false}, {TEXT("DodgeSlam"), true, 0, false, true, false},
        {TEXT("AirPunch"), false, 0, true, false, false}, {TEXT("AirLeftPunch"), false, 1, true, false, false},
        {TEXT("AirKick"), false, 2, true, false, false}, {TEXT("AirCrashKick"), false, 3, true, false, false},
        {TEXT("SwordSlash"), false, 0, false, false, true}, {TEXT("SwordBackslash"), false, 1, false, false, true},
        {TEXT("SwordThrust"), false, 2, false, false, true}, {TEXT("SwordSpin"), false, 3, false, false, true}};
    static int32 Shot = -1;
    static float ShotClock = 0.f;
    const int32 Total = UE_ARRAY_COUNT(Moves) * 3;
    if (GetWorld()->GetTimeSeconds() < 2.f) return;
    if (Shot < 0) Shot = 0;
    // Pin a near-profile view every frame (a straight punch toward a front camera hides its
    // extension, and the combat camera would otherwise drift between shots).
    SetActorRotation(FRotator::ZeroRotator);
    DesiredCameraDistance = 340.f;
    if (auto* PC = Cast<APlayerController>(GetController())) PC->SetControlRotation(FRotator(-8.f, 100.f, 0.f));
    if (Shot >= Total)
    {
        UE_LOG(LogTemp, Display, TEXT("ATTACK PREVIEW PASSED: %d screenshots in Saved/Screenshots/Attacks"), Total);
        FPlatformMisc::RequestExitWithStatus(false, 0);
        Shot = Total + 1;
        return;
    }
    if (Shot > Total) return;
    const FPreviewMove& Move = Moves[Shot / 3];
    SelectedWeapon = Move.Sword ? 1 : 0;
    Sword->SetVisibility(Move.Sword);
    CurrentAttack = FistCombat::Select(Move.Heavy, Move.Combo, Move.Air, Move.AfterDodge, Move.Sword);
    const float Progress = Shot % 3 == 0 ? .15f : Shot % 3 == 1 ? CurrentAttack.ContactFraction : .85f;
    // UpdateAttackTiming subtracts this frame's time after this runs; bHitResolved skips the damage sweep.
    AttackClock = CurrentAttack.Duration * (1.f - Progress) + Dt;
    bHitResolved = true;
    MoveLabel = FString::Printf(TEXT("%s %s"), Move.Clip, Shot % 3 == 0 ? TEXT("wind-up") : Shot % 3 == 1 ? TEXT("CONTACT") : TEXT("follow-through"));
    MoveLabelClock = 1.f;
    ShotClock += Dt;
    // Startup frames can be long; wait for several frames too, so the pose is applied before capture.
    static int32 ShotFrames = 0;
    if (++ShotFrames >= 6 && ShotClock >= .35f)
    {
        ShotFrames = 0;
        const UAnimSequence* Expected = FindAttackAnimation(CurrentAttack.Type);
        UE_LOG(LogTemp, Display, TEXT("ATTACK PREVIEW %s_%d: expected %s at %.3fs, mesh plays %s at %.3fs"), Move.Clip, Shot % 3,
            Expected ? *Expected->GetName() : TEXT("none"), AttackClipPosition(Progress, Expected),
            ActiveAnimation ? *ActiveAnimation->GetName() : TEXT("none"), GetMesh()->GetPosition());
        FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / FString::Printf(TEXT("Screenshots/Attacks/%s_%d.png"), Move.Clip, Shot % 3), false, false);
        ShotClock = 0.f;
        ++Shot;
    }
#endif
}
