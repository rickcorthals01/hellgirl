#include "HellgirlAnimInstance.h"
#include "SecondaryMotionSpring.h"
#include "Animation/AnimSingleNodeInstanceProxy.h"
#include "Animation/AnimInstanceProxy.h"
#include "BonePose.h"
#include "Components/SkeletalMeshComponent.h"
#include "HAL/IConsoleManager.h"

static TAutoConsoleVariable<float> CVarBodyMotion(TEXT("hellgirl.BodyMotion"), 1.f, TEXT("Subtle body motion strength: 0 off, 1 default."));
static TAutoConsoleVariable<float> CVarHairMotion(TEXT("hellgirl.HairMotion"), 0.f, TEXT("Legacy bun motion strength. Disabled while fitting the longer hairstyle."));

struct FAnchorSpring
{
    FSecondaryMotionSpring Spring;
    FVector LastPosition = FVector::ZeroVector;
    FVector LastVelocity = FVector::ZeroVector;
    bool Ready = false;
    void Update(FVector Position, float Dt, float Limit, float Inertia)
    {
        if (!Ready || Dt <= 0.f || Dt > .1f || FVector::DistSquared(Position, LastPosition) > 10000.f)
        {
            Spring = {}; LastPosition = Position; LastVelocity = FVector::ZeroVector; Ready = true; return;
        }
        const FVector Velocity = (Position - LastPosition) / Dt;
        Spring.Step((Velocity - LastVelocity) / Dt, Dt, Limit, Inertia);
        LastPosition = Position; LastVelocity = Velocity;
    }
};

struct FHellgirlAnimProxy : public FAnimSingleNodeInstanceProxy
{
    explicit FHellgirlAnimProxy(UAnimInstance* Instance) : FAnimSingleNodeInstanceProxy(Instance) {}
    FAnchorSpring Body, Hair;
    FVector BodyOffset = FVector::ZeroVector;
    FVector HairOffset = FVector::ZeroVector;
    virtual void PreUpdate(UAnimInstance* Instance, float Dt) override
    {
        FAnimSingleNodeInstanceProxy::PreUpdate(Instance, Dt);
        if (USkeletalMeshComponent* Mesh = Instance->GetSkelMeshComponent())
        {
            Body.Update(Mesh->GetSocketLocation(TEXT("spine_04")), Dt, .45f, .12f);
            Hair.Update(Mesh->GetSocketLocation(TEXT("head")), Dt, .65f, .20f);
            BodyOffset = Mesh->GetComponentTransform().InverseTransformVectorNoScale(Body.Spring.Offset) * FMath::Clamp(CVarBodyMotion.GetValueOnGameThread(), 0.f, 1.f);
            HairOffset = Mesh->GetComponentTransform().InverseTransformVectorNoScale(Hair.Spring.Offset) * FMath::Clamp(CVarHairMotion.GetValueOnGameThread(), 0.f, 1.f);
        }
    }
    virtual bool Evaluate(FPoseContext& Output) override
    {
        const bool Result = FAnimSingleNodeInstanceProxy::Evaluate(Output);
        if (!Result) return Result;
        FCSPose<FCompactPose> ComponentPose;
        ComponentPose.InitPose(Output.Pose);
        const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
        for (const FName Name : {FName(TEXT("breast1_l")), FName(TEXT("breast1_r"))})
        {
            const int32 MeshIndex = Bones.GetReferenceSkeleton().FindBoneIndex(Name);
            if (MeshIndex == INDEX_NONE) continue;
            const FCompactPoseBoneIndex Index = Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(MeshIndex));
            if (Index == INDEX_NONE) continue;
            const FCompactPoseBoneIndex Parent = Output.Pose.GetParentBoneIndex(Index);
            if (Parent == INDEX_NONE) continue;
            const FVector LocalOffset = ComponentPose.GetComponentSpaceTransform(Parent).InverseTransformVector(BodyOffset);
            Output.Pose[Index].AddToTranslation(LocalOffset);
        }
        Output.Curve.Set(FName(TEXT("HairSwaySide")), HairOffset.X / 1.2f);
        Output.Curve.Set(FName(TEXT("HairSwayBack")), -HairOffset.Y / 1.2f);
        return Result;
    }
};

FAnimInstanceProxy* UHellgirlAnimInstance::CreateAnimInstanceProxy()
{
    return new FHellgirlAnimProxy(this);
}
