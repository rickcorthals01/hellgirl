#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Fighter/ArenaFighter.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FImpAnimationPlaybackTest, "Hellgirl.Animation.ImpPlayback", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FImpAnimationPlaybackTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    for (const bool Flying : {false, true})
    {
        AArenaFighter* Imp = World->SpawnActor<AArenaFighter>();
        Imp->MakeEnemy(1, Flying);
        Imp->SetEnemyType(Flying ? EHellgirlEnemyType::FlyingImps : EHellgirlEnemyType::Imps);
        if (!TestNotNull(TEXT("Configured Imp idle loads"), Imp->EnemyIdleAnimation.Get())) continue;
        TestNotNull(TEXT("Movement loads"), Imp->EnemyMoveAnimation.Get());
        TestNotNull(TEXT("Attack loads"), Imp->EnemyAttackAnimation.Get());
        TestNotNull(TEXT("Hit loads"), Imp->EnemyHitAnimation.Get());
        TestNotNull(TEXT("Death loads"), Imp->EnemyDeathAnimation.Get());
        Imp->UpdateEnemyAnimation(.1f);
        TestTrue(TEXT("Idle starts automatically"), Imp->EnemyActiveAnimation == Imp->EnemyIdleAnimation);
        Imp->GetCharacterMovement()->Velocity = FVector(224,0,0);
        Imp->UpdateEnemyAnimation(.1f);
        TestTrue(TEXT("Movement selects correct loop"), Imp->EnemyActiveAnimation == Imp->EnemyMoveAnimation);
        Imp->StartAttack(false);
        Imp->AttackClock = Imp->CurrentAttack.Duration * .4f;
        Imp->UpdateEnemyAnimation(0.f);
        TestTrue(TEXT("Claw overrides movement"), Imp->EnemyActiveAnimation == Imp->EnemyAttackAnimation);
        TestTrue(TEXT("Contact frame matches combat timing"), FMath::IsNearlyEqual(Imp->EnemyAnimationTime, .75f));
        Imp->ReceiveHit(1.f, FVector(1,0,0));
        Imp->UpdateEnemyAnimation(.05f);
        TestTrue(TEXT("Incoming hit interrupts attack"), Imp->EnemyActiveAnimation == Imp->EnemyHitAnimation);
        Imp->GetCharacterMovement()->Velocity = FVector::ZeroVector;
        Imp->UpdateEnemyAnimation(.4f);
        TestTrue(TEXT("Hit returns to idle"), Imp->EnemyActiveAnimation == Imp->EnemyIdleAnimation);
        Imp->HitClock = 0.f;
        const UAnimSequence* LastLivingAnimation = Imp->EnemyActiveAnimation;
        const float LastLivingAnimationTime = Imp->EnemyAnimationTime;
        Imp->ReceiveHit(10000.f, FVector(1,0,0));
        Imp->UpdateEnemyAnimation(.2f);
        TestTrue(TEXT("Death starts a physical corpse"), Imp->IsDeathRagdollActive());
        TestTrue(TEXT("Death freezes animation driving"), Imp->GetMesh()->bPauseAnims);
        TestTrue(TEXT("Death keeps current pose for ragdoll handoff"), Imp->EnemyActiveAnimation == LastLivingAnimation);
        Imp->UpdateEnemyAnimation(1.f);
        TestTrue(TEXT("Animation cannot overwrite simulated bones"), FMath::IsNearlyEqual(Imp->EnemyAnimationTime, LastLivingAnimationTime));
        TestTrue(TEXT("Death stops character movement"), Imp->GetCharacterMovement()->MovementMode == MOVE_None);
        TestTrue(TEXT("Corpse is kept for physics before cleanup"), Imp->GetLifeSpan() >= 11.9f);
        Imp->Destroy();
    }
    World->DestroyWorld(false);
    return true;
}
#endif
