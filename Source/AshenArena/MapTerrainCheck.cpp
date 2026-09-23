#include "ArenaGameMode.h"
#include "ArenaFighter.h"
#include "CastleTerrainLayout.h"
#include "EnemySpawnPoint.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

void AArenaGameMode::RunTerrainCheck(float Dt)
{
#if WITH_DEV_AUTOMATION_TESTS
    if (!FParse::Param(FCommandLine::Get(),TEXT("HellgirlTerrainCheck")) || MapNumber!=1) return;
    static int32 Phase=0;
    static float Elapsed=0.f;
    Elapsed+=Dt;
    if (Phase==2 || GetWorld()->GetTimeSeconds()<1.f) return;
    auto Fail=[](const TCHAR* Why)
    {
        UE_LOG(LogTemp,Error,TEXT("TERRAIN CHECK FAILED: %s"),Why);
        FPlatformMisc::RequestExitWithStatus(false,1);
    };
    auto* Player=Cast<AArenaFighter>(UGameplayStatics::GetPlayerPawn(this,0));
    if (!Player) { Fail(TEXT("No player")); return; }
    FCollisionObjectQueryParams Types;
    Types.AddObjectTypesToQuery(ECC_WorldStatic); Types.AddObjectTypesToQuery(ECC_WorldDynamic);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(TerrainCheck),true,Player);
    auto FloorAt=[&](float X,float Y,FHitResult& Hit)
    {
        return GetWorld()->LineTraceSingleByObjectType(Hit,FVector(X,Y,1600),FVector(X,Y,-600),Types,Query)
            && Hit.ImpactNormal.Z>=Player->GetCharacterMovement()->GetWalkableFloorZ();
    };
    if (Phase==0)
    {
        FVector Previous=FVector::ZeroVector;
        bool HasPrevious=false;
        for (float X=-5000.f; X<=4800.f; X+=100.f)
        {
            FHitResult Floor;
            if (!FloorAt(X,CastleTerrain::PathCenterY(X),Floor)) { Fail(TEXT("Missing or unwalkable main path")); return; }
            const FVector Center=Floor.ImpactPoint+FVector(0,0,98);
            if (HasPrevious)
            {
                FHitResult Obstacle;
                if (GetWorld()->SweepSingleByObjectType(Obstacle,Previous,Center,FQuat::Identity,Types,FCollisionShape::MakeCapsule(38,88),Query))
                { Fail(TEXT("A gateway or path decoration blocks the player capsule")); return; }
            }
            Previous=Center; HasPrevious=true;
        }
        for (auto Site : SpawnSites)
        {
            Site->SetActorTickEnabled(false);
            for (int32 I=0; I<Site->EnemyCount; ++I)
            {
                const float Angle=I*2.39996f, Radius=190.f+45.f*I;
                const FVector P=Site->GetActorLocation()+FVector(FMath::Cos(Angle)*Radius,FMath::Sin(Angle)*Radius,0);
                FHitResult Floor;
                if (!FloorAt(P.X,P.Y,Floor)) { Fail(TEXT("Missing floor beneath an encounter spawn")); return; }
                const float Scale=Site->bBoss ? 1.5f : 1.f;
                if (GetWorld()->OverlapAnyTestByObjectType(Floor.ImpactPoint+FVector(0,0,88.f*Scale+15.f),FQuat::Identity,
                    Types,FCollisionShape::MakeCapsule(38.f*Scale,88.f*Scale),Query))
                { Fail(TEXT("Scenery blocks an enemy spawn")); return; }
            }
        }
        FHitResult Bank,Path;
        if (!FloorAt(-4800,-2400,Bank) || !FloorAt(-5000,0,Path) || Bank.ImpactPoint.Z-Path.ImpactPoint.Z<50.f)
        { Fail(TEXT("Castle still has no terrain relief")); return; }
        Player->SetActorLocation(FVector(-5000,0,115),false,nullptr,ETeleportType::TeleportPhysics);
        Player->ResetAfterRecovery();
        Player->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
        Elapsed=0.f; Phase=1;
        UE_LOG(LogTemp,Display,TEXT("TERRAIN CHECK geometry passed: route sweeps, encounter clearances, raised banks"));
    }
    if (Phase==1)
    {
        const FVector P=Player->GetActorLocation();
        if (P.Z<70.f || P.Z>650.f) { Fail(TEXT("Walking left the terrain")); Phase=2; return; }
        if (P.X>=-1500.f)
        {
            Phase=2;
            UE_LOG(LogTemp,Display,TEXT("TERRAIN CHECK PASSED: walked from entrance through ruined gate, floor geometry and all encounter spawns clear"));
            FPlatformMisc::RequestExitWithStatus(false,0); return;
        }
        if (Elapsed>14.f) { Fail(TEXT("Player cannot walk through the approach and gate")); Phase=2; return; }
        const float AheadX=FMath::Min(static_cast<float>(P.X)+220.f,-1400.f);
        Player->AddMovementInput((FVector(AheadX,CastleTerrain::PathCenterY(AheadX),P.Z)-P).GetSafeNormal2D());
    }
#endif
}
