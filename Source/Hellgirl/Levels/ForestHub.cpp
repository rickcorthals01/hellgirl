#include "Levels/ArenaGameMode.h"
#include "Levels/ForestArt.h"
#include "Fighter/ArenaFighter.h"
#include "Enemies/EnemyTypes.h"
#include "UI/HellgirlPlayerController.h"
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
#include "Progress/CampaignProgress.h"

void AArenaGameMode::TravelToHub()
{
    if (GetUnlockedLevel()<2) return;
    UGameplayStatics::OpenLevel(this,FName(*UGameplayStatics::GetCurrentLevelName(this)),true,TEXT("ForestHub=1"));
}
void AArenaGameMode::BuildForestHub()
{
    MapTitle=TEXT("FOREST CAMP");
    HubInteractionPoints={FVector(0,0,0),FVector(800,0,0),FVector(-100,650,0)};
    UWorld* World=GetWorld();
    // An invisible, level floor keeps the hub interactions where they were; the scenery sits on top of it.
    Prop(FVector(0,0,-80),FVector(38,38,1.6f),FLinearColor::Black)->SetActorHiddenInGame(true);
    BuildForestHubDetails();
    // The campfire: stone ring, logs and embers from the kit, burning with the stylised fire.
    ForestArt::Solid(World,ForestArt::Kit(TEXT("SM_Campfire")),FTransform(FRotator(0,20,0),FVector(0,0,0),FVector(1.15f)),true);
    ForestArt::Fire(World,FVector(0,0,18),1.1f,TEXT("NS_Stylish_Fire_1"));
    ForestArt::Embers(World,FVector(0,0,60),.5f);
    CampfireLight=GetWorld()->SpawnActor<APointLight>(FVector(0,0,160),FRotator::ZeroRotator);
    CampfireLight->PointLightComponent->SetMobility(EComponentMobility::Movable);
    CampfireLight->PointLightComponent->SetLightColor(FLinearColor(1,.48f,.17f));
    CampfireLight->PointLightComponent->SetAttenuationRadius(1150);
    CampfireLight->PointLightComponent->SetIntensity(14000);
    CampfireLight->PointLightComponent->SetVolumetricScatteringIntensity(1.5f);
    // A fallen log to sit on and a bedroll on the quiet side of camp.
    ForestArt::Solid(World,ForestArt::Kit(TEXT("SM_Log")),FTransform(FRotator(0,25,0),FVector(-320,-350,26),FVector(.6f)),true);
    auto* Bedroll=Prop(FVector(150,-560,12),FVector(2.4f,1.1f,.2f),FLinearColor(.21f,.14f,.18f));
    Bedroll->SetActorRotation(FRotator(0,-8,0));
    Prop(FVector(255,-565,24),FVector(.45f,1.05f,.28f),FLinearColor(.3f,.24f,.26f))->SetActorRotation(FRotator(0,-8,0));
    // The forest road leads to a rune gateway, where the level menu opens.
    ForestArt::Solid(World,ForestArt::Kit(TEXT("SM_Gateway")),FTransform(FRotator::ZeroRotator,FVector(1240,0,-5),FVector(1.2f)),true);
    Prop(FVector(1650,0,440),FVector(.5f,6,9),FLinearColor(.008f,.014f,.012f));
    // A friendly display actor, never an enemy fighter, so attacks and targeting cannot hurt him.
    // He follows Hellgirl home once she has beaten Stage 3 (03.5); checks of the camp itself always have him.
    if (HellgirlProgress::Flag(TEXT("Stage3Won")) || (HellgirlProgress::IsCheckRun() && GetUnlockedLevel()>=4))
    {
        const auto& Model=GetDefault<UHellgirlEnemyModels>()->ForType(EHellgirlEnemyType::Goblins);
        HubMerchant=GetWorld()->SpawnActor<ASkeletalMeshActor>(FVector(-100,700,5),FRotator(0,-160,0));
        HubMerchant->GetSkeletalMeshComponent()->SetSkeletalMesh(Model.Mesh.LoadSynchronous());
        HubMerchant->GetSkeletalMeshComponent()->SetRelativeScale3D(FVector(.8f));
        HubMerchant->GetSkeletalMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        HubMerchant->GetSkeletalMeshComponent()->PlayAnimation(Model.Idle.LoadSynchronous(),true);
        // His stall: a chest of wares, crates, a vase and a heap of coins.
        ForestArt::Solid(World,ForestArt::Inferno(TEXT("Props/SM_ChestBig_001")),FTransform(FRotator(0,-160,0),FVector(90,830,0),FVector(.55f)),true);
        ForestArt::Solid(World,ForestArt::Inferno(TEXT("Props/SM_Box_001")),FTransform(FRotator(0,-140,0),FVector(230,930,0),FVector(.5f)),true);
        ForestArt::Solid(World,ForestArt::Inferno(TEXT("Props/SM_Box_001")),FTransform(FRotator(0,-120,0),FVector(240,925,70),FVector(.35f)),false);
        ForestArt::Solid(World,ForestArt::Inferno(TEXT("Props/SM_Vase_001")),FTransform(FRotator(0,40,0),FVector(-20,900,0),FVector(.6f)),false);
        ForestArt::Solid(World,ForestArt::Inferno(TEXT("Props/SM_GoldPileSmall_001")),FTransform(FRotator(0,10,0),FVector(170,760,0),FVector(.5f)),false);
        ForestArt::PointGlow(World,FVector(0,780,300),FLinearColor(1.f,.59f,.29f),3300.f,730.f);
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
    const FRotator MoonLight(-30.f,200.f,0.f);
    ForestArt::NightSky(World,-MoonLight.Vector());
    ForestArt::Moonlight(World,MoonLight,.018f);
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
    // One-time camp moments, in story order: setting up camp after Stage 1 (on a black screen), the endless
    // mode notice after Stage 2, and the goblin who followed her home after Stage 3.
    if (!HellgirlProgress::IsAutomated() || FParse::Param(FCommandLine::Get(),TEXT("HellgirlStoryCheck")))
        if (auto* PC=Cast<AHellgirlPlayerController>(UGameplayStatics::GetPlayerController(this,0)); PC && !PC->IsPauseMenuOpen())
        {
            const int32 Unlocked=GetUnlockedLevel();
            const TCHAR* Flag=nullptr; const TCHAR* Moment=nullptr;
            if (Unlocked>=2 && !HellgirlProgress::Flag(TEXT("CampSetUp"))) { Flag=TEXT("CampSetUp"); Moment=TEXT("C_SetUpCamp"); }
            else if (HellgirlProgress::Flag(TEXT("Stage2Won")) && !HellgirlProgress::Flag(TEXT("EndlessGoblins"))) { Flag=TEXT("EndlessGoblins"); Moment=TEXT("C_EndlessUnlocked"); }
            else if (HellgirlProgress::Flag(TEXT("Stage3Won")) && !HellgirlProgress::Flag(TEXT("GoblinFollowed"))) { Flag=TEXT("GoblinFollowed"); Moment=TEXT("C_GoblinFollowed"); }
            if (Flag && PC->ShowConversation(Moment)) HellgirlProgress::SetFlag(Flag);
            else if (Flag) HellgirlProgress::SetFlag(Flag); // a missing conversation must not block camp
            if (Flag) return;
        }
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
