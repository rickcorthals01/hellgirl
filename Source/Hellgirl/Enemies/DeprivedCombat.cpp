// World IV's enemy, the Deprived: a strong shadow that hunts Hellgirl through the maze, one at a time (numbers in
// Rules/EnemyTuning.h, the moves in Enemies/EnemyMoveset.cpp; the maze raises them from their lairs, Levels/FrozenMaze.cpp).
//   It always knows where she is. Where it can see her it goes straight at her; otherwise it follows the maze toward her
//   (HuntWaypoint, the next square of the way, set by the maze). Chasing her from further off (or out of sight) it uses
//   its speed boost: twice its walking speed for 6 s, then 5 s before it can boost again.
//   Once per life, reaching her, it charges an execute: 0.5 s to the hit, heavy damage and a lunge through her. A dodge
//   avoids it; a perfect dodge parries it (her counter). After that it slices and claws: slow swings, medium damage.
// Until its own model arrives it is a shadow of Hellgirl: her body all black with a violet rim, her sword clips, a black
// blade and two white eyes (ApplyDeprivedStandIn).
#include "Fighter/ArenaFighter.h"
#include "Enemies/EnemyMovesetState.h"
#include "Levels/MapPieces.h"
#include "Rules/EnemyTuning.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Particles/ParticleSystem.h"

void AArenaFighter::UpdateDeprivedTactics(float Dt, AArenaFighter* Player)
{
    if (!Player) return;
    const FVector Delta = Player->GetActorLocation() - GetActorLocation();
    const float Distance = Delta.Size2D();
    const FVector Toward = Delta.GetSafeNormal2D().IsNearlyZero() ? GetActorForwardVector() : Delta.GetSafeNormal2D();
    FHitResult Wall;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(DeprivedSight), false, this);
    Query.AddIgnoredActor(Player);
    const bool Sees = !GetWorld()->LineTraceSingleByChannel(Wall, GetActorLocation(), Player->GetActorLocation(), ECC_Visibility, Query);
    // In sight: straight at her. Out of sight: along the way through the maze.
    FVector Way = ((Sees || !bHasHuntWaypoint ? Player->GetActorLocation() : HuntWaypoint) - GetActorLocation()).GetSafeNormal2D();
    if (Way.IsNearlyZero()) Way = Toward;
    // The speed boost, whenever it is chasing her from further off; a puff of shadow as it surges.
    if (DeprivedBoostClock <= 0.f && DeprivedBoostCooldown <= 0.f && AttackClock <= 0.f && (!Sees || Distance > EnemyTuning::DeprivedBoostFrom))
    {
        DeprivedBoostClock = EnemyTuning::DeprivedBoostSeconds;
        MoveLabel = TEXT("DEPRIVED / SPEED BOOST");
        MoveLabelClock = 1.f;
        if (auto* Smoke = LoadObject<UParticleSystem>(nullptr, TEXT("/Game/Realistic_Starter_VFX_Pack_Vol2/Particles/Smoke/P_Smoke_A.P_Smoke_A")))
            UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), Smoke, GetActorLocation() - FVector(0.f, 0.f, 70.f), FRotator::ZeroRotator, FVector(.5f));
    }
    if (AttackClock > 0.f) return;
    SetActorRotation((Sees ? Toward : Way).Rotation());
    if (Sees && CanBeginEnemyMove(Player))
    {
        if (!bDeprivedExecuteUsed && Distance < EnemyTuning::DeprivedExecuteReach)
        {
            BeginEnemyMove(EEnemyMove::DeprivedExecute, Player);
            if (EnemyMove == EEnemyMove::DeprivedExecute) { bDeprivedExecuteUsed = true; return; }
        }
        else if (bDeprivedExecuteUsed && Distance < EnemyTuning::DeprivedSwingReach)
        {
            const EEnemyMove Swing = DeprivedSwing % 2 ? EEnemyMove::DeprivedClaw : EEnemyMove::DeprivedSlice;
            BeginEnemyMove(Swing, Player);
            if (EnemyMove == Swing) { ++DeprivedSwing; return; }
        }
    }
    // Close in: right up to her for the execute, to swinging reach after it.
    if (!Sees || Distance > (bDeprivedExecuteUsed ? 150.f : 200.f)) AddMovementInput(Way, 1.f);
}

void AArenaFighter::ApplyDeprivedStandIn()
{
    USkeletalMesh* Shape = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Hellgirl/Outfits/Rags/Rags.Rags"));
    if (!Shape) return;
    auto Clip = [](const TCHAR* Name)
    {
        return LoadObject<UAnimSequence>(nullptr, *FString::Printf(TEXT("/Game/Hellgirl/Outfits/Rags/Animations/%s.%s"), Name, Name));
    };
    GetMesh()->SetSkeletalMesh(Shape);
    GetMesh()->SetRelativeLocation(FVector(0.f, 0.f, -88.f));
    GetMesh()->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
    GetMesh()->SetRelativeScale3D(FVector(1.08f)); // a little taller than her
    GetMesh()->SetAnimationMode(EAnimationMode::AnimationSingleNode);
    GetMesh()->SetAnimation(nullptr);
    EnemyIdleAnimation = Clip(TEXT("SwordIdle"));
    EnemyMoveAnimation = Clip(TEXT("SwordRun"));
    EnemyAttackAnimation = Clip(TEXT("SwordSlash"));            // the slice
    EnemyAttack2Animation = Clip(TEXT("SwordBackslash"));       // the claw
    EnemyHeavyAttackAnimation = Clip(TEXT("SwordChargedStrike")); // the execute
    EnemyHitAnimation = Clip(TEXT("SwordHit"));
    EnemyDeathAnimation = Clip(TEXT("SwordDeath"));
    EnemyQuickAttackAnimation = EnemyAirAttackAnimation = EnemyDodgeAnimation = EnemyJumpAnimation = nullptr;
    EnemyActiveAnimation = nullptr;
    UpdateEnemyAnimation(0.f);
    // All black, with a violet rim of light.
    UMaterialInterface* Shadow = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Environment/Maze/MI_DeprivedShadow.MI_DeprivedShadow"));
    if (Shadow) for (int32 I = 0; I < GetMesh()->GetNumMaterials(); ++I) GetMesh()->SetMaterial(I, Shadow);
    GetMesh()->SetVisibility(true);
    for (UStaticMeshComponent* Part : {Body.Get(), Head.Get(), RightHand.Get(), LeftHand.Get(), RightFoot.Get(), LeftFoot.Get()}) Part->SetVisibility(false);
    // Her sword as its black blade.
    Sword->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, TEXT("RightHand"));
    ApplySwordLook();
    if (Shadow) for (int32 I = 0; I < Sword->GetNumMaterials(); ++I) Sword->SetMaterial(I, Shadow);
    Sword->SetVisibility(true);
    // Two small white eyes on the head, set in the head bone's own axes (worked out from where it faces now).
    if (GetMesh()->DoesSocketExist(TEXT("Head")))
    {
        const FTransform HeadBone = GetMesh()->GetSocketTransform(TEXT("Head"), RTS_World);
        const FVector Front = HeadBone.InverseTransformVectorNoScale(GetActorForwardVector());
        const FVector Up = HeadBone.InverseTransformVectorNoScale(FVector::UpVector);
        const FVector Right = HeadBone.InverseTransformVectorNoScale(GetActorRightVector());
        UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
        for (const float Side : {-1.f, 1.f})
        {
            auto* Eye = NewObject<UStaticMeshComponent>(this);
            Eye->SetStaticMesh(Sphere);
            Eye->SetupAttachment(GetMesh(), TEXT("Head"));
            Eye->SetRelativeLocation(Front * 8.5f + Up * 7.f + Right * 3.1f * Side);
            Eye->SetRelativeScale3D(FVector(.018f));
            Eye->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Eye->SetCastShadow(false);
            if (auto* Glow = MapPieces::Surface(Eye, FLinearColor(1.f, .96f, .9f), false, true))
            {
                Glow->SetScalarParameterValue(TEXT("EmissiveStrength"), 12.f);
                Eye->SetMaterial(0, Glow);
            }
            Eye->RegisterComponent();
        }
    }
}
