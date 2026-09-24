#include "Fighter/ArenaFighter.h"
#include "Rules/FistCombatRules.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "HAL/PlatformMisc.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

#if WITH_EDITORONLY_DATA
#include "Rendering/SkeletalMeshModel.h"
#include "Rendering/SkeletalMeshLODModel.h"
#endif

namespace
{
// Logs the stored (bind-pose) sole height of the right foot from toe to heel, the same
// measurement Tools-side Blender checks make, so imports can be compared numerically.
void LogFootSole(USkeletalMesh* Mesh)
{
#if WITH_EDITORONLY_DATA
    if (!Mesh || !Mesh->GetImportedModel() || !Mesh->GetImportedModel()->LODModels.Num()) return;
    const FReferenceSkeleton& Ref = Mesh->GetRefSkeleton();
    const int32 FootIndex = Ref.FindBoneIndex(TEXT("RightFoot")), ToeIndex = Ref.FindBoneIndex(TEXT("RightToeBase"));
    if (FootIndex == INDEX_NONE || ToeIndex == INDEX_NONE) return;
    auto ComponentSpace = [&Ref](int32 Bone) { FTransform T = FTransform::Identity; for (int32 B = Bone; B != INDEX_NONE; B = Ref.GetParentIndex(B)) T = T * Ref.GetRefBonePose()[B]; return T; };
    const FVector Ankle = ComponentSpace(FootIndex).GetLocation(), Ball = ComponentSpace(ToeIndex).GetLocation();
    const FVector Along = FVector(Ball.X - Ankle.X, Ball.Y - Ankle.Y, 0.f).GetSafeNormal();
    TArray<FVector> Foot;
    for (const FSkelMeshSection& Section : Mesh->GetImportedModel()->LODModels[0].Sections)
        for (const FSoftSkinVertex& V : Section.SoftVertices)
        {
            const FVector P(V.Position);
            if (P.Z < 16.f && FVector::Dist2D(P, (Ankle + Ball) * .5f) < 14.f) Foot.Add(P);
        }
    float Lo = FLT_MAX, Hi = -FLT_MAX;
    for (const FVector& P : Foot) { const float D = FVector::DotProduct(P, Along); Lo = FMath::Min(Lo, D); Hi = FMath::Max(Hi, D); }
    // 0 = toe tip, 1 = back of the heel.
    auto Sole = [&](float A, float B) { float Z = FLT_MAX; for (const FVector& P : Foot) { const float F = 1.f - (FVector::DotProduct(P, Along) - Lo) / FMath::Max(Hi - Lo, 1.f); if (F >= A && F <= B) Z = FMath::Min(Z, (float)P.Z); } return Z; };
    UE_LOG(LogTemp, Display, TEXT("FEET MESH %s: %d foot verts, length %.1f cm | sole height: toes %.1f, ball %.1f, arch %.1f, heel %.1f | ankle joint z %.1f, ball joint z %.1f"),
        *Mesh->GetName(), Foot.Num(), Hi - Lo, Sole(0.f, .15f), Sole(.2f, .35f), Sole(.45f, .6f), Sole(.8f, 1.f), Ankle.Z, Ball.Z);
#endif
}
}

// Visual check for combat clips (needs a GPU, so it is not part of Tests/run-checks.ps1):
//   UnrealEditor.exe Hellgirl.uproject /Engine/Maps/Entry?ForestHub=1 -game -windowed -ResX=900 -ResY=900 -HellgirlAttackPreview
// Holds every player attack at wind-up, contact and follow-through and saves screenshots to
// Saved/Screenshots/Attacks/<Clip>_<0|1|2>.png. The attack state is set directly, so no damage is dealt.
void AArenaFighter::RunAttackAnimationPreview(float Dt)
{
#if WITH_DEV_AUTOMATION_TESTS
    if (bEnemy || UGameplayStatics::GetPlayerPawn(this, 0) != this) return;
    // -HellgirlFeetPreview: side view of standing and walking, to inspect foot placement.
    if (FParse::Param(FCommandLine::Get(), TEXT("HellgirlFeetPreview")))
    {
        static int32 FeetShot = 0;
        static float FeetClock = 0.f;
        const float Now = GetWorld()->GetTimeSeconds();
        if (Now < 2.f || FeetShot > 7) return;
        // -PreviewOutfit=N shows outfit N instead of the saved one (not saved).
        int32 PreviewOutfit = -1;
        if (FeetShot == 0 && FeetClock == 0.f && FParse::Value(FCommandLine::Get(), TEXT("PreviewOutfit="), PreviewOutfit)) SetOutfit(PreviewOutfit);
        const bool Walking = FeetShot >= 2 && FeetShot <= 5;
        // Shots 6-7: no animation at all, so the mesh shows the imported reference pose.
        if (FeetShot >= 6 && GetMesh()->GetAnimationMode() != EAnimationMode::AnimationCustomMode)
        {
            GetMesh()->SetAnimationMode(EAnimationMode::AnimationCustomMode);
            // -FeetMesh=/Game/... shows another import of the mesh for comparison.
            FString MeshPath;
            if (FParse::Value(FCommandLine::Get(), TEXT("FeetMesh="), MeshPath))
                if (USkeletalMesh* Other = LoadObject<USkeletalMesh>(nullptr, *MeshPath)) GetMesh()->SetSkeletalMesh(Other);
            LogFootSole(GetMesh()->GetSkeletalMeshAsset());
        }
        // Walk away from the campfire (-X); standing faces the same way. Camera side-on at hip height.
        if (Walking) AddMovementInput(-FVector::ForwardVector, FeetShot >= 4 ? 1.f : .35f);
        else SetActorRotation(FRotator(0.f, 180.f, 0.f));
        // -FeetFullBody frames the whole character instead of the feet.
        const bool FullBody = FParse::Param(FCommandLine::Get(), TEXT("FeetFullBody"));
        DesiredCameraDistance = FullBody ? 420.f : 230.f;
        CameraArm->SocketOffset = FVector(0.f, 0.f, FullBody ? 0.f : -45.f);
        CameraArm->bEnableCameraLag = false;
        if (auto* PC = Cast<APlayerController>(GetController())) PC->SetControlRotation(FRotator(-4.f, 90.f, 0.f));
        FeetClock += Dt;
        if (FeetClock >= (FeetShot % 2 ? .23f : .6f))
        {
            FeetClock = 0.f;
            UE_LOG(LogTemp, Display, TEXT("FEET PREVIEW %d: speed %.0f, clip %s at %.3fs, capsule bottom z %.1f, mesh z %.1f"), FeetShot,
                GetVelocity().Size2D(), ActiveAnimation ? *ActiveAnimation->GetName() : TEXT("none"), GetMesh()->GetPosition(),
                GetActorLocation().Z - GetCapsuleComponent()->GetScaledCapsuleHalfHeight(), GetMesh()->GetComponentLocation().Z);
            FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / FString::Printf(TEXT("Screenshots/Feet/%d.png"), FeetShot), false, false);
            if (++FeetShot > 7) { UE_LOG(LogTemp, Display, TEXT("FEET PREVIEW PASSED")); FPlatformMisc::RequestExitWithStatus(false, 0); }
        }
        return;
    }
    // -HellgirlJumpPreview: a real jump, captured rising, near the top, falling and just after landing.
    if (FParse::Param(FCommandLine::Get(), TEXT("HellgirlJumpPreview")))
    {
        static int32 JumpShot = -1;
        static float JumpClock = 0.f;
        static bool bWasAirborne = false;
        static int32 LandedFrames = 0;
        if (GetWorld()->GetTimeSeconds() < 2.f || JumpShot > 3) return;
        SetActorRotation(FRotator(0.f, 180.f, 0.f));
        DesiredCameraDistance = 420.f;
        CameraArm->SocketOffset = FVector::ZeroVector;
        CameraArm->bEnableCameraLag = false;
        if (auto* PC = Cast<APlayerController>(GetController())) PC->SetControlRotation(FRotator(-4.f, 90.f, 0.f));
        const bool Airborne = GetCharacterMovement()->IsFalling();
        if (JumpShot < 0) { Jump(); JumpShot = 0; JumpClock = 0.f; bWasAirborne = false; return; }
        JumpClock += Dt;
        const float VerticalSpeed = GetVelocity().Z;
        const bool Take = (JumpShot == 0 && Airborne && JumpClock > .12f) || (JumpShot == 1 && Airborne && VerticalSpeed < 80.f && VerticalSpeed > -80.f)
            || (JumpShot == 2 && Airborne && VerticalSpeed < -350.f) || (JumpShot == 3 && LandedFrames >= 5);
        // Wait a few frames after touchdown so the landing pose has been applied before capturing.
        if (bWasAirborne && !Airborne) ++LandedFrames;
        bWasAirborne |= Airborne;
        if (Take)
        {
            UE_LOG(LogTemp, Display, TEXT("JUMP PREVIEW %d: airborne %d, vertical speed %.0f, clip %s at %.3fs"), JumpShot, Airborne ? 1 : 0, VerticalSpeed,
                ActiveAnimation ? *ActiveAnimation->GetName() : TEXT("none"), GetMesh()->GetPosition());
            FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / FString::Printf(TEXT("Screenshots/Jump/%d.png"), JumpShot), false, false);
            if (++JumpShot > 3) { UE_LOG(LogTemp, Display, TEXT("JUMP PREVIEW PASSED")); FPlatformMisc::RequestExitWithStatus(false, 0); }
        }
        if (JumpClock > 6.f) { UE_LOG(LogTemp, Error, TEXT("JUMP PREVIEW FAILED at shot %d"), JumpShot); FPlatformMisc::RequestExitWithStatus(false, 1); JumpShot = 4; }
        return;
    }
    if (!FParse::Param(FCommandLine::Get(), TEXT("HellgirlAttackPreview"))) return;
    // Special: 1 = charged strike (full charge), 2 = holding heavy (charging loop), 3 = dodge roll, 4 = knockdown, 5 = hit reaction.
    struct FPreviewMove { const TCHAR* Clip; bool Heavy; int32 Combo; bool Air; bool AfterDodge; bool Sword; int32 Special = 0; };
    static const FPreviewMove Moves[] = {
        {TEXT("Dodge"), false, 0, false, false, false, 3},
        {TEXT("Charge"), true, 0, false, false, false, 2}, {TEXT("ChargedStrike"), true, 0, false, false, false, 1},
        {TEXT("RightPunch"), false, 0, false, false, false}, {TEXT("LeftPunch"), false, 1, false, false, false},
        {TEXT("DoubleJab"), false, 3, false, false, false}, {TEXT("RightKick"), true, 0, false, false, false},
        {TEXT("LeftKick"), true, 1, false, false, false}, {TEXT("LegSweep"), true, 3, false, false, false},
        {TEXT("Headbutt"), false, 0, false, true, false}, {TEXT("DodgeSlam"), true, 0, false, true, false},
        {TEXT("AirPunch"), false, 0, true, false, false}, {TEXT("AirLeftPunch"), false, 1, true, false, false},
        {TEXT("AirKick"), false, 2, true, false, false}, {TEXT("AirCrashKick"), false, 3, true, false, false},
        {TEXT("AirSlam"), true, 0, true, false, false}, {TEXT("Knockdown"), false, 0, false, false, false, 4}, {TEXT("Hit"), false, 0, false, false, false, 5},
        {TEXT("SwordSlash"), false, 0, false, false, true}, {TEXT("SwordBackslash"), false, 1, false, false, true},
        {TEXT("SwordThrust"), false, 2, false, false, true}, {TEXT("SwordSpin"), false, 3, false, false, true}};
    static int32 Shot = -1;
    static float ShotClock = 0.f;
    const int32 Total = UE_ARRAY_COUNT(Moves) * 3;
    if (GetWorld()->GetTimeSeconds() < 2.f) return;
    if (Shot < 0)
    {
        Shot = 0;
        // -PreviewOutfit=N shows outfit N instead of the saved one (not saved).
        int32 PreviewOutfit = -1;
        if (FParse::Value(FCommandLine::Get(), TEXT("PreviewOutfit="), PreviewOutfit)) SetOutfit(PreviewOutfit);
    }
    // Pin a near-profile view every frame (a straight punch toward a front camera hides its
    // extension, and the combat camera would otherwise drift between shots).
    SetActorRotation(FRotator::ZeroRotator);
    // -AttackPreviewClose frames the upper body, to inspect the hands.
    const bool Close = FParse::Param(FCommandLine::Get(), TEXT("AttackPreviewClose"));
    DesiredCameraDistance = Close ? 170.f : 340.f;
    if (Close) { CameraArm->SocketOffset = FVector(0.f, 0.f, 25.f); CameraArm->bEnableCameraLag = false; }
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
    CurrentAttack = Move.Special == 1 ? FistCombat::Charged(1.f) : FistCombat::Select(Move.Heavy, Move.Combo, Move.Air, Move.AfterDodge, Move.Sword);
    const float Progress = Move.Special >= 3 ? (Shot % 3 == 0 ? .15f : Shot % 3 == 1 ? .45f : .8f)
        : Shot % 3 == 0 ? .15f : Shot % 3 == 1 ? CurrentAttack.ContactFraction : .85f;
    // Charging has no attack clock: the pose code plays the Charge loop while heavy is held.
    bHeavyHeld = Move.Special == 2;
    // UpdateAttackTiming subtracts this frame's time after this runs; bHitResolved skips the damage sweep.
    AttackClock = Move.Special >= 2 ? 0.f : CurrentAttack.Duration * (1.f - Progress) + Dt;
    // A typical knockdown lasts .9 s; the pose code maps its clip over that time.
    PlayerKnockdownDuration = .9f;
    KnockdownClock = Move.Special == 4 ? PlayerKnockdownDuration * (1.f - Progress) + Dt : 0.f;
    // The hit reaction runs .3 s on its own timer, which the pose code advances by this frame's time.
    PlayerHitAnimationTime = Move.Special == 5 ? Progress * .3f - Dt : 100.f;
    // The roll plays on its own timer, which the pose code advances by this frame's time.
    DodgeAnimationTime = Move.Special == 3 ? Progress * DodgeAnimationDuration - Dt : 100.f;
    bHitResolved = true;
    MoveLabel = FString::Printf(TEXT("%s %s"), Move.Clip, Shot % 3 == 0 ? TEXT("wind-up") : Shot % 3 == 1 ? TEXT("CONTACT") : TEXT("follow-through"));
    MoveLabelClock = 1.f;
    ShotClock += Dt;
    // Startup frames can be long; wait for several frames too, so the pose is applied before capture.
    static int32 ShotFrames = 0;
    if (++ShotFrames >= 6 && ShotClock >= .35f)
    {
        ShotFrames = 0;
        const UAnimSequence* Expected = Move.Special == 2 ? CombatAnimations.FindRef(TEXT("Charge")).Get()
            : Move.Special == 3 ? CombatAnimations.FindRef(TEXT("Dodge")).Get()
            : Move.Special == 4 ? CombatAnimations.FindRef(TEXT("Knockdown")).Get()
            : Move.Special == 5 ? CombatAnimations.FindRef(TEXT("Hit")).Get() : FindAttackAnimation(CurrentAttack.Type);
        UE_LOG(LogTemp, Display, TEXT("ATTACK PREVIEW %s_%d: expected %s at %.3fs, mesh plays %s at %.3fs"), Move.Clip, Shot % 3,
            // The charging loop runs on world time, so only its clip is compared.
            Expected ? *Expected->GetName() : TEXT("none"), Move.Special == 2 ? GetMesh()->GetPosition()
                : Move.Special >= 3 ? (Expected ? Progress * Expected->GetPlayLength() : 0.f) : AttackClipPosition(Progress, Expected),
            ActiveAnimation ? *ActiveAnimation->GetName() : TEXT("none"), GetMesh()->GetPosition());
        FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / FString::Printf(TEXT("Screenshots/Attacks/%s_%d.png"), Move.Clip, Shot % 3), false, false);
        ShotClock = 0.f;
        ++Shot;
    }
#endif
}
