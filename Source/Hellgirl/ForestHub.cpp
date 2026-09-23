#include "ArenaGameMode.h"
#include "ArenaFighter.h"
#include "EnemyTypes.h"
#include "HellgirlPlayerController.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Animation/SkeletalMeshActor.h"
#include "Engine/PointLight.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

void AArenaGameMode::TravelToHub()
{
    if (GetUnlockedLevel()<2) return;
    UGameplayStatics::OpenLevel(this,FName(*UGameplayStatics::GetCurrentLevelName(this)),true,TEXT("ForestHub=1"));
}
void AArenaGameMode::BuildForestHub()
{
    MapTitle=TEXT("FOREST CAMP");
    HubInteractionPoints={FVector(0,0,0),FVector(800,0,0),FVector(-100,650,0)};
    auto Shape=[&](FVector P,FVector Scale,FLinearColor Color,const TCHAR* MeshPath,bool Collide=true) {
        auto* A=Prop(P,Scale,Color);
        A->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,MeshPath));
        A->SetActorEnableCollision(Collide); return A;
    };
    const TCHAR* Cylinder=TEXT("/Engine/BasicShapes/Cylinder.Cylinder"),*Cone=TEXT("/Engine/BasicShapes/Cone.Cone");
    // The clearing and road stay level for the existing hub interactions.
    auto* Ground=Prop(FVector(0,0,-85),FVector(38,38,1.6f),FLinearColor(.095f,.13f,.07f));
    if (auto* Earth=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Environment/Materials/M_EnvironmentEarth.M_EnvironmentEarth")))
        if (auto* Mat=UMaterialInstanceDynamic::Create(Earth,Ground))
        {
            Mat->SetVectorParameterValue(TEXT("Color"),FLinearColor(.38f,.48f,.32f));
            Ground->GetStaticMeshComponent()->SetMaterial(0,Mat);
        }
    Shape(FVector(0,0,-7),FVector(19.5f,19.5f,.15f),FLinearColor(.29f,.205f,.12f),Cylinder);
    for (int32 I=0;I<6;++I)
        Shape(FVector(350+I*130,0,-1),FVector(3.6f,3.3f,.09f),FLinearColor(.23f,.16f,.095f),Cylinder);
    BuildForestHubDetails();
    // Stone fire ring, charred logs and simple emissive flames.
    for (int32 I=0;I<12;++I)
    {
        const float A=I*PI/6.f;
        Prop(FVector(FMath::Cos(A)*110,FMath::Sin(A)*110,20),FVector(.55f,.45f,.4f),FLinearColor(.25f,.25f,.22f),true);
    }
    for (int32 I=0;I<3;++I)
    {
        auto* Log=Shape(FVector(0,0,25),FVector(.28f,.28f,1.6f),FLinearColor(.07f,.035f,.018f),Cylinder);
        Log->SetActorRotation(FRotator(90,I*60,0));
    }
    for (int32 I=0;I<5;++I)
    {
        auto* Flame=Shape(FVector((I-2)*17,0,65+I%2*12),FVector(.35f,.35f,1.f),FLinearColor(1,.2f,.015f),Cone,false);
        if (auto* Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Environment/Materials/M_EnvironmentGlow.M_EnvironmentGlow")))
        {
            auto* Material=UMaterialInstanceDynamic::Create(Base,Flame);
            Material->SetVectorParameterValue(TEXT("Color"),FLinearColor(1.f,.24f,.025f));
            Flame->GetStaticMeshComponent()->SetMaterial(0,Material);
        }
    }
    CampfireLight=GetWorld()->SpawnActor<APointLight>(FVector(0,0,160),FRotator::ZeroRotator);
    CampfireLight->PointLightComponent->SetMobility(EComponentMobility::Movable);
    CampfireLight->PointLightComponent->SetLightColor(FLinearColor(1,.48f,.17f));
    CampfireLight->PointLightComponent->SetAttenuationRadius(1050);
    CampfireLight->PointLightComponent->SetIntensity(14000);
    // Seats and bedroll on the quiet side of camp.
    auto* Seat=Shape(FVector(-320,-350,45),FVector(.8f,.8f,4.2f),FLinearColor(.14f,.075f,.035f),Cylinder);
    Seat->SetActorRotation(FRotator(90,25,0));
    Prop(FVector(150,-560,15),FVector(2.4f,1.1f,.22f),FLinearColor(.21f,.17f,.22f));
    Prop(FVector(255,-560,26),FVector(.45f,1.1f,.3f),FLinearColor(.29f,.24f,.27f));
    // The trail fades into a dark forest arch; the menu opens before the boundary.
    for (int32 Side : {-1,1})
    {
        auto* Trunk=Shape(FVector(1220,Side*260,460),FVector(1.3f,1.3f,10.f),FLinearColor(.055f,.045f,.035f),Cylinder);
        Trunk->SetActorRotation(FRotator(0,0,Side*10));
        Prop(FVector(1320,Side*300,650),FVector(5,5,7),FLinearColor(.014f,.035f,.025f),true)->SetActorEnableCollision(false);
    }
    Prop(FVector(1650,0,440),FVector(.5f,6,9),FLinearColor(.008f,.014f,.012f));
    // A friendly display actor, never an enemy fighter, so attacks and targeting cannot hurt him.
    if (GetUnlockedLevel()>=4)
    {
        const auto& Model=GetDefault<UHellgirlEnemyModels>()->ForType(EHellgirlEnemyType::Goblins);
        HubMerchant=GetWorld()->SpawnActor<ASkeletalMeshActor>(FVector(-100,700,5),FRotator(0,-160,0));
        HubMerchant->GetSkeletalMeshComponent()->SetSkeletalMesh(Model.Mesh.LoadSynchronous());
        HubMerchant->GetSkeletalMeshComponent()->SetRelativeScale3D(FVector(.8f));
        HubMerchant->GetSkeletalMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        HubMerchant->GetSkeletalMeshComponent()->PlayAnimation(Model.Idle.LoadSynchronous(),true);
        Prop(FVector(100,820,90),FVector(2.8f,1.1f,.18f),FLinearColor(.16f,.09f,.04f));
        for (int32 Side : {-1,1}) Prop(FVector(100+Side*100,820,42),FVector(.15f,.7f,.9f),FLinearColor(.12f,.06f,.03f));
        Prop(FVector(210,940,35),FVector(.75f,.75f,.7f),FLinearColor(.22f,.13f,.065f));
        Prop(FVector(170,820,110),FVector(.32f,.32f,.32f),FLinearColor(.38f,.1f,.2f),true);
    }
    // Invisible collision follows the clearing rather than the edge of the ground.
    // Leave only the short, fenced road to level select open.
    auto Boundary=[&](FVector From,FVector To)
    {
        const FVector Delta=To-From;
        auto* Wall=Prop((From+To)*.5f+FVector(0,0,2900.f),
            FVector((Delta.Size()+24.f)/100.f,.8f,60.f),FLinearColor::Black);
        Wall->SetActorRotation(FRotator(0,FMath::RadiansToDegrees(FMath::Atan2(Delta.Y,Delta.X)),0));
        Wall->Tags.Add(TEXT("HubBoundary"));
        Wall->SetActorHiddenInGame(true);
    };
    constexpr int32 Sections=40;
    constexpr float Radius=1060.f;
    for (int32 I=0;I<Sections;++I)
    {
        const float A=2.f*PI*I/Sections;
        const float B=2.f*PI*(I+1)/Sections;
        if (FMath::Abs(FMath::UnwindRadians((A+B)*.5f))<.32f) continue;
        Boundary(FVector(Radius*FMath::Cos(A),Radius*FMath::Sin(A),0),
            FVector(Radius*FMath::Cos(B),Radius*FMath::Sin(B),0));
    }
    for (int32 Side : {-1,1})
        Boundary(FVector(990.f,Side*345.f,0),FVector(1460.f,Side*345.f,0));
    Boundary(FVector(1460,-345,0),FVector(1460,345,0));
    auto* Moon=GetWorld()->SpawnActor<ADirectionalLight>(FVector(0,0,1800),FRotator(-42,-35,0));
    Moon->GetLightComponent()->SetMobility(EComponentMobility::Movable); Moon->GetLightComponent()->SetIntensity(1.9f);
    Moon->GetLightComponent()->SetLightColor(FLinearColor(.39f,.53f,.78f));
    auto* Sky=GetWorld()->SpawnActor<ASkyLight>(); Sky->GetLightComponent()->SetMobility(EComponentMobility::Movable); Sky->GetLightComponent()->SetIntensity(.62f);
    Sky->GetLightComponent()->SetRealTimeCaptureEnabled(false);
    auto* Fog=GetWorld()->SpawnActor<AExponentialHeightFog>(); Fog->GetComponent()->SetFogDensity(.018f);
    Fog->GetComponent()->SetFogInscatteringColor(FLinearColor(.028f,.06f,.063f)); Fog->GetComponent()->SetStartDistance(750.f);
    auto* Post=GetWorld()->SpawnActor<APostProcessVolume>(); Post->bUnbound=true;
    Post->Settings.bOverride_AutoExposureMinBrightness=Post->Settings.bOverride_AutoExposureMaxBrightness=true;
    Post->Settings.AutoExposureMinBrightness=Post->Settings.AutoExposureMaxBrightness=1.f;
    Post->Settings.bOverride_BloomIntensity=true; Post->Settings.BloomIntensity=.48f;
    Post->Settings.bOverride_AmbientOcclusionIntensity=true; Post->Settings.AmbientOcclusionIntensity=1.f;
}
int32 AArenaGameMode::GetHubInteraction() const
{
    const auto* Player=UGameplayStatics::GetPlayerPawn(this,0);
    if (!bForestHub || !Player) return -1;
    for (int32 I=0;I<HubInteractionPoints.Num();++I)
        if ((I!=2 || HubMerchant) && FVector::DistSquared(Player->GetActorLocation(),HubInteractionPoints[I]+FVector(0,0,88))<FMath::Square(290.f)) return I;
    return -1;
}
void AArenaGameMode::TickForestHub(float Dt)
{
    HubTime+=Dt;
    if (CampfireLight) CampfireLight->PointLightComponent->SetIntensity(14000.f+1500.f*FMath::Sin(HubTime*11.f)+700.f*FMath::Sin(HubTime*19.f));
    const int32 Interaction=GetHubInteraction();
    if (Interaction != 1) bHubLevelMenuTriggered = false;
    else if (!bHubLevelMenuTriggered)
    {
        if (auto* PC = Cast<AHellgirlPlayerController>(UGameplayStatics::GetPlayerController(this,0)); PC && !PC->IsPauseMenuOpen())
        {
            PC->OpenHubMenu(1);
            // Closing the menu keeps it closed until the player leaves and returns.
            bHubLevelMenuTriggered = PC->IsPauseMenuOpen();
        }
    }
    Prompt=Interaction==0 ? TEXT("CAMPFIRE / Rest and choose your outfit") : Interaction==1 ? TEXT("FOREST ROAD / Select a level") : Interaction==2 ? TEXT("GOBLIN MERCHANT / Open shop") : TEXT("");
    if (auto* Player=Cast<AArenaFighter>(UGameplayStatics::GetPlayerPawn(this,0)); Player && Player->GetActorLocation().Z < -300.f)
    {
        Player->SetActorLocation(FVector(-550,0,110)); Player->ResetAfterRecovery();
    }
}
