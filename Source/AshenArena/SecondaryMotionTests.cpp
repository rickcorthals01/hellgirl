#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "SecondaryMotionSpring.h"
#include "HellgirlAnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSecondaryMotionStabilityTest, "Hellgirl.Animation.SecondaryMotionStability", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSecondaryMotionStabilityTest::RunTest(const FString& Parameters)
{
    for (const float Dt : {1.f / 30.f, 1.f / 60.f, 1.f / 144.f})
    {
        FSecondaryMotionSpring Spring;
        for (int32 I = 0; I < FMath::RoundToInt(1.f / Dt); ++I)
        {
            Spring.Step(FVector(100000.f, -100000.f, 100000.f), Dt, .45f, .12f);
            TestTrue(TEXT("Extreme movement stays finite and within the body limit"), !Spring.Offset.ContainsNaN() && Spring.Offset.Size() <= .451f);
        }
        TestTrue(TEXT("Movement produces a visible nonzero response"), Spring.Offset.Size() > .01f);
        for (int32 I = 0; I < FMath::RoundToInt(2.f / Dt); ++I)
            Spring.Step(FVector::ZeroVector, Dt, .45f, .12f);
        TestTrue(TEXT("Motion settles after stopping"), Spring.Offset.Size() < .001f && Spring.Velocity.Size() < .01f);
        Spring.Offset = FVector(.3f); Spring.Velocity = FVector(5.f);
        Spring.Step(FVector(200.f), .5f, .45f, .12f);
        TestTrue(TEXT("Long frame hitch safely resets motion"), Spring.Offset.IsZero() && Spring.Velocity.IsZero());
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSecondaryMotionPoseTest, "Hellgirl.Animation.SecondaryMotionPose", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSecondaryMotionPoseTest::RunTest(const FString& Parameters)
{
    USkeletalMesh* Asset = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/HellgirlTest/Hellgirl_Secondary.Hellgirl_Secondary"));
    UAnimSequence* Animation = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/HellgirlTest/Locomotion/Run.Run"));
    if (!TestNotNull(TEXT("Secondary-motion mesh loads"), Asset) || !TestNotNull(TEXT("Run animation loads"), Animation)) return false;
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    IConsoleVariable* BodySetting = IConsoleManager::Get().FindConsoleVariable(TEXT("hellgirl.BodyMotion"));
    IConsoleVariable* HairSetting = IConsoleManager::Get().FindConsoleVariable(TEXT("hellgirl.HairMotion"));
    const float OldBody = BodySetting->GetFloat(), OldHair = HairSetting->GetFloat();
    TArray<FVector> Baseline;
    float MaxBodyDelta = 0.f, MaxHairWeight = 0.f;
    for (int32 Pass = 0; Pass < 2; ++Pass)
    {
        BodySetting->Set(float(Pass), ECVF_SetByCode); HairSetting->Set(float(Pass), ECVF_SetByCode);
        USkeletalMeshComponent* Mesh = NewObject<USkeletalMeshComponent>(World);
        Mesh->SetSkeletalMesh(Asset);
        Mesh->RegisterComponentWithWorld(World);
        Mesh->SetAnimInstanceClass(UHellgirlAnimInstance::StaticClass());
        Mesh->SetAnimation(Animation);
        TestTrue(TEXT("Custom animation instance is active"), Mesh->GetAnimInstance() && Mesh->GetAnimInstance()->IsA<UHellgirlAnimInstance>());
        for (int32 I = 0; I < 120; ++I)
        {
            Mesh->SetWorldLocation(FVector(10.f * FMath::Sin(I / 60.f * 12.f), 0.f, 0.f));
            Mesh->SetPosition(FMath::Fmod(I / 60.f, .6f), false);
            Mesh->TickAnimation(1.f / 60.f, false);
            Mesh->RefreshBoneTransforms();
            const FVector Bone = Mesh->GetSocketTransform(TEXT("breast1_l"), RTS_Component).GetTranslation();
            if (Pass == 0) Baseline.Add(Bone);
            else
            {
                MaxBodyDelta = FMath::Max(MaxBodyDelta, float(FVector::Distance(Bone, Baseline[I])));
                MaxHairWeight = FMath::Max(MaxHairWeight, FMath::Abs(Mesh->GetAnimInstance()->GetCurveValue(TEXT("HairSwaySide"))));
            }
        }
        Mesh->UnregisterComponent();
    }
    BodySetting->Set(OldBody, ECVF_SetByCode); HairSetting->Set(OldHair, ECVF_SetByCode);
    World->DestroyWorld(false);
    AddInfo(FString::Printf(TEXT("Measured body displacement %.4f cm; hair curve %.4f"), MaxBodyDelta, MaxHairWeight));
    TestTrue(TEXT("Body bone responds and remains subtle"), MaxBodyDelta > .01f && MaxBodyDelta <= .46f);
    TestTrue(TEXT("Hair morph responds and remains bounded"), MaxHairWeight > .01f && MaxHairWeight <= .55f);
    return true;
}
#endif
