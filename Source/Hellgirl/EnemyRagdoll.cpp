#include "ArenaFighter.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PhysicsEngine/BodyInstance.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"

namespace
{
    void SetCorpseCollision(UPrimitiveComponent* Component)
    {
        // Bodies settle against scenery without obstructing combat, pickups or the camera.
        Component->SetCollisionObjectType(ECC_PhysicsBody);
        Component->SetCollisionResponseToAllChannels(ECR_Ignore);
        Component->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
        Component->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
        Component->SetGenerateOverlapEvents(false);
        Component->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        Component->SetEnableGravity(true);
        Component->SetAllUseCCD(true);
    }

    bool HasUsablePhysicsAsset(const USkeletalMeshComponent* RagdollMesh)
    {
        if (!RagdollMesh || !RagdollMesh->IsVisible() || !RagdollMesh->GetSkeletalMeshAsset()) return false;
        const UPhysicsAsset* Asset = RagdollMesh->GetPhysicsAsset();
        if (!Asset) return false;
        for (const USkeletalBodySetup* Setup : Asset->SkeletalBodySetups)
        {
            if (Setup && Setup->AggGeom.GetElementCount() > 0 && RagdollMesh->GetBoneIndex(Setup->BoneName) != INDEX_NONE)
                return true;
        }
        return false;
    }
}

void AArenaFighter::StartDeathRagdoll(const FVector& ImpulseVelocity)
{
    if (!bEnemy || IsAlive() || bDeathRagdollActive) return;
    bDeathRagdollActive = true;

    GetCharacterMovement()->StopMovementImmediately();
    GetCharacterMovement()->DisableMovement();
    GetCharacterMovement()->SetComponentTickEnabled(false);
    GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    GetCapsuleComponent()->SetGenerateOverlapEvents(false);
    SetActorEnableCollision(true);
    if (AttackFlash) AttackFlash->SetVisibility(false);
    if (EnemyNameLabel) EnemyNameLabel->SetVisibility(false);
    if (Sword) Sword->SetVisibility(false);

    // Keep the final animated pose as the starting pose, including on airborne deaths.
    USkeletalMeshComponent* RagdollMesh = GetMesh();
    RagdollMesh->bPauseAnims = true;
    RagdollMesh->RefreshBoneTransforms();
    const FVector Velocity = ImpulseVelocity.ContainsNaN() ? FVector::ZeroVector : ImpulseVelocity;
    const FVector TravelDirection = Velocity.GetSafeNormal2D().IsNearlyZero()
        ? GetActorForwardVector() : Velocity.GetSafeNormal2D();
    const FVector TumbleAxis = FVector::CrossProduct(FVector::UpVector, TravelDirection);

    bool bSkeletalRagdoll = false;
    if (HasUsablePhysicsAsset(RagdollMesh))
    {
        SetCorpseCollision(RagdollMesh);
        RagdollMesh->SetSimulatePhysics(true);
        // Override any kinematic bodies in imported assets so an arm cannot pin the corpse in place.
        RagdollMesh->SetAllBodiesSimulatePhysics(true);
        RagdollMesh->SetAllBodiesPhysicsBlendWeight(1.f);
        if (RagdollMesh->IsAnySimulatingPhysics())
        {
            RagdollMesh->SetAllPhysicsLinearVelocity(Velocity);
            RagdollMesh->SetAllPhysicsAngularVelocityInRadians(TumbleAxis * 1.2f);
            RagdollMesh->WakeAllRigidBodies();
            bSkeletalRagdoll = true;
        }
    }

    if (!bSkeletalRagdoll)
    {
        // Unrigged placeholders and future models without a PhysicsAsset still fall and tumble.
        // Their existing visible parts remain attached to this single physical capsule.
        RagdollMesh->SetAllBodiesSimulatePhysics(false);
        RagdollMesh->SetSimulatePhysics(false);
        RagdollMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        UCapsuleComponent* Capsule = GetCapsuleComponent();
        Capsule->BodyInstance.bLockXRotation = false;
        Capsule->BodyInstance.bLockYRotation = false;
        Capsule->BodyInstance.bLockZRotation = false;
        Capsule->BodyInstance.bLockRotation = false;
        Capsule->BodyInstance.SetDOFLock(EDOFMode::SixDOF);
        SetCorpseCollision(Capsule);
        Capsule->SetLinearDamping(.25f);
        Capsule->SetAngularDamping(.6f);
        Capsule->SetSimulatePhysics(true);
        Capsule->SetPhysicsLinearVelocity(Velocity);
        Capsule->SetPhysicsAngularVelocityInRadians(TumbleAxis * 2.2f);
        Capsule->WakeAllRigidBodies();
    }

    // Retain the actor so its components are owned and destroyed together on map changes or expiry.
    SetLifeSpan(12.f);
    UE_LOG(LogTemp, Display, TEXT("PHYSICS CORPSE: %s (%s)"), *GetName(),
        bSkeletalRagdoll ? TEXT("skeletal ragdoll") : TEXT("rigid placeholder"));
}
