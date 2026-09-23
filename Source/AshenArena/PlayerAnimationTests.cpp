#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "ArenaFighter.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlayerAnimationPlaybackTest,"Hellgirl.Animation.PlayerMovePlayback",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPlayerAnimationPlaybackTest::RunTest(const FString& Parameters)
{
 UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
 AArenaFighter* Player=World->SpawnActor<AArenaFighter>();
 using FistCombat::Move;
 for (int32 I=0;I<static_cast<int32>(Move::EnemyClaw);++I)
 {
  const Move Type=static_cast<Move>(I);
  UAnimSequence* Clip=Player->FindAttackAnimation(Type);
  if (!TestNotNull(TEXT("Every player attack has an animation"),Clip)) continue;
  Player->CurrentAttack={Type,Clip->GetPlayLength(),.5f,1.f,100.f,0.f,0.f,0};
  Player->AttackClock=Clip->GetPlayLength()*.5f;
  Player->UpdatePose(0.f);
  TestTrue(TEXT("Combat selects the matching animation"),Player->ActiveAnimation==Clip);
  TestTrue(TEXT("Playback follows combat progress"),FMath::IsNearlyEqual(Player->GetMesh()->GetSingleNodeInstance()->GetCurrentTime(),Clip->GetPlayLength()*.5f));
 }
 Player->CurrentAttack=FistCombat::Select(true,0,true,false);
 Player->AttackClock=.1f;Player->bGroundImpactPending=true;
 Player->UpdatePose(0.f);
 TestTrue(TEXT("Slam holds before contact while airborne"),Player->GetMesh()->GetSingleNodeInstance()->GetCurrentTime()<Player->CurrentAttack.ContactFraction*Player->ActiveAnimation->GetPlayLength());
 Player->bGroundImpactPending=false;Player->AttackClock=.25f;Player->UpdatePose(0.f);
 TestTrue(TEXT("Landing releases slam recovery"),Player->GetMesh()->GetSingleNodeInstance()->GetCurrentTime()>Player->CurrentAttack.ContactFraction*Player->ActiveAnimation->GetPlayLength());
 Player->DodgeClock=.125f;Player->UpdatePose(0.f);
 TestTrue(TEXT("Dodge overrides an interrupted attack"),Player->ActiveAnimation==Player->ExtendedAnimations.FindRef(TEXT("Dodge")));
 Player->AttackClock=Player->DodgeClock=0.f;Player->bHeavyHeld=true;Player->UpdatePose(.1f);
 TestTrue(TEXT("Holding heavy attack uses charge pose"),Player->ActiveAnimation==Player->ExtendedAnimations.FindRef(TEXT("Charge")));
 Player->bHeavyHeld=false;Player->ReceiveHit(1.f,FVector(1,0,0));Player->UpdatePose(.05f);
 TestTrue(TEXT("Damage triggers hit reaction"),Player->ActiveAnimation==Player->ExtendedAnimations.FindRef(TEXT("Hit")));
 Player->Health=0.f;Player->UpdatePose(2.f);
 TestTrue(TEXT("Death overrides all other playback"),Player->ActiveAnimation==Player->ExtendedAnimations.FindRef(TEXT("Death")));
 TestTrue(TEXT("Death holds its final frame"),FMath::IsNearlyEqual(Player->GetMesh()->GetSingleNodeInstance()->GetCurrentTime(),Player->ActiveAnimation->GetPlayLength()));
 Player->Destroy();World->DestroyWorld(false);return true;
}
#endif
