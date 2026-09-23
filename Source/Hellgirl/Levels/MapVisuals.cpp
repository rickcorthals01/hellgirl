#include "Levels/ArenaGameMode.h"
#include "Fighter/ArenaFighter.h"
#include "Levels/CastleTerrainLayout.h"
#include "Enemies/EnemySpawnPoint.h"
#include "ProceduralMeshComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/PointLight.h"
#include "Engine/PostProcessVolume.h"
#include "Components/DirectionalLightComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "Levels/MapPieces.h"

namespace MapPieces
{
UMaterialInstanceDynamic* Surface(UObject* Owner, FLinearColor Color, bool VertexColor, bool Glow)
{
    auto* Base = LoadObject<UMaterialInterface>(nullptr, Glow
        ? TEXT("/Game/Environment/Materials/M_EnvironmentGlow.M_EnvironmentGlow")
        : TEXT("/Game/Environment/Materials/M_EnvironmentSurface.M_EnvironmentSurface"));
    if (!Base) Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    auto* Mat = UMaterialInstanceDynamic::Create(Base, Owner);
    if (Mat)
    {
        Mat->SetVectorParameterValue(TEXT("Color"), Color);
        Mat->SetScalarParameterValue(TEXT("UseVertexColor"), VertexColor ? 1.f : 0.f);
        Mat->SetScalarParameterValue(TEXT("Roughness"), .92f);
        Mat->SetScalarParameterValue(TEXT("EmissiveStrength"), .7f);
    }
    return Mat;
}

// A broad, level cap over a tapered, irregular rock body. The top retains the
// authored rectangular platform footprint; its sides now have real geometry.
AActor* Rock(UWorld* World, FVector Center, FVector Size, FLinearColor Color, int32 Seed, bool Collision, bool LevelTop)
{
    auto* Actor = World->SpawnActor<AActor>();
    if (!Actor) return nullptr;
    auto* Mesh = NewObject<UProceduralMeshComponent>(Actor);
    Actor->SetRootComponent(Mesh);
    Actor->AddInstanceComponent(Mesh);
    Mesh->bUseAsyncCooking = false;
    Mesh->bUseComplexAsSimpleCollision = true;
    Mesh->SetCollisionProfileName(Collision ? TEXT("BlockAll") : TEXT("NoCollision"));
    Mesh->RegisterComponent();
    Actor->SetActorLocation(Center);
    Actor->Tags.Add(TEXT("EnvironmentRock"));
    FRandomStream Random(Seed);
    const FVector2D Outline[] = {{1,-1},{1,-.33},{1,.33},{1,1},{.33,1},{-.33,1},
        {-1,1},{-1,.33},{-1,-.33},{-1,-1},{-.33,-1},{.33,-1}};
    constexpr int32 Count = UE_ARRAY_COUNT(Outline);
    TArray<FVector> Rings;
    for (int32 Ring = 0; Ring < 3; ++Ring)
        for (const FVector2D& P : Outline)
        {
            const float Scale = Ring == 0 ? (LevelTop ? 1.f : Random.FRandRange(.55f,.95f)) : Ring == 1 ? Random.FRandRange(.92f, 1.12f) : Random.FRandRange(.18f,.45f);
            const float Z = Ring == 0 ? (LevelTop ? 0.f : Size.Z * Random.FRandRange(.05f,.45f)) : Ring == 1 ? -Size.Z * Random.FRandRange(.55f,.8f) : -Size.Z * Random.FRandRange(1.35f,1.9f);
            const FVector2D Shape = LevelTop ? P : P.GetSafeNormal();
            Rings.Add(FVector(Shape.X * Size.X * .5f * Scale, Shape.Y * Size.Y * .5f * Scale, Z));
        }
    TArray<FVector> Vertices, Normals;
    TArray<FVector2D> UVs;
    TArray<FLinearColor> Colors;
    TArray<FProcMeshTangent> Tangents;
    TArray<int32> Triangles;
    auto Triangle = [&](FVector A, FVector B, FVector C, float Shade)
    {
        const FVector Normal = FVector::CrossProduct(C-A, B-A).GetSafeNormal();
        const int32 Start = Vertices.Num();
        FLinearColor Tint = Color * Shade; Tint.A = 1.f;
        for (const FVector& P : {A,B,C})
        {
            Vertices.Add(P); Normals.Add(Normal); UVs.Add(FVector2D(P.X, P.Y));
            Colors.Add(Tint); Tangents.Add(FProcMeshTangent((B-A).GetSafeNormal(),false));
        }
        Triangles.Append({Start,Start+1,Start+2});
    };
    for (int32 I=0; I<Count; ++I)
    {
        const int32 Next=(I+1)%Count;
        Triangle(FVector(0,0,LevelTop ? 0.f : Size.Z*.6f),Rings[Next],Rings[I],LevelTop ? 1.f : Random.FRandRange(.94f,1.07f));
        for (int32 Ring=0; Ring<2; ++Ring)
        {
            const int32 A=Ring*Count+I, B=Ring*Count+Next, C=A+Count, D=B+Count;
            const float Shade=Random.FRandRange(.68f,.98f);
            Triangle(Rings[A],Rings[B],Rings[C],Shade);
            Triangle(Rings[B],Rings[D],Rings[C],Shade);
        }
        Triangle(FVector(0,0,-Size.Z*1.6f),Rings[2*Count+I],Rings[2*Count+Next],.6f);
    }
    Mesh->CreateMeshSection_LinearColor(0,Vertices,Triangles,Normals,UVs,Colors,Tangents,Collision);
    Mesh->SetMaterial(0,Surface(Actor,FLinearColor::White,true));
    return Actor;
}

UHierarchicalInstancedStaticMeshComponent* DecorationBatch(UWorld* World, const TCHAR* Asset, FLinearColor Color, bool Glow)
{
    auto* Actor=World->SpawnActor<AActor>();
    auto* Mesh=NewObject<UHierarchicalInstancedStaticMeshComponent>(Actor);
    Actor->SetRootComponent(Mesh); Actor->AddInstanceComponent(Mesh);
    Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,Asset));
    Mesh->SetCollisionProfileName(TEXT("NoCollision"));
    Mesh->SetGenerateOverlapEvents(false);
    Mesh->SetMaterial(0,Surface(Actor,Color,false,Glow));
    Mesh->RegisterComponent();
    return Mesh;
}
}

using namespace MapPieces;

void AArenaGameMode::RockPlatform(FVector Center,FVector Size,FLinearColor Color)
{
    const int32 Seed=FMath::RoundToInt(Center.X*3.f+Center.Y*7.f+Size.X);
    Rock(GetWorld(),Center,Size,Color*1.3f,Seed,true);
}

void AArenaGameMode::BuildForestHubDetails()
{
    FRandomStream Random(4291);
    auto* Trunks=DecorationBatch(GetWorld(),TEXT("/Engine/BasicShapes/Cylinder.Cylinder"),FLinearColor(.18f,.105f,.058f));
    auto* Needles=DecorationBatch(GetWorld(),TEXT("/Engine/BasicShapes/Cone.Cone"),FLinearColor(.18f,.34f,.2f));
    auto* DarkNeedles=DecorationBatch(GetWorld(),TEXT("/Engine/BasicShapes/Cone.Cone"),FLinearColor(.11f,.23f,.14f));
    auto* Moss=DecorationBatch(GetWorld(),TEXT("/Engine/BasicShapes/Sphere.Sphere"),FLinearColor(.12f,.25f,.09f));
    auto* Ferns=DecorationBatch(GetWorld(),TEXT("/Engine/BasicShapes/Cone.Cone"),FLinearColor(.12f,.29f,.12f));
    auto* Pebbles=DecorationBatch(GetWorld(),TEXT("/Engine/BasicShapes/Sphere.Sphere"),FLinearColor(.22f,.22f,.18f));
    auto* Fireflies=DecorationBatch(GetWorld(),TEXT("/Engine/BasicShapes/Sphere.Sphere"),FLinearColor(.35f,1.f,.24f),true);
    Needles->SetCastShadow(false);
    DarkNeedles->SetCastShadow(false);
    for (auto* Batch : {Trunks,Needles,DarkNeedles,Moss,Ferns,Pebbles,Fireflies})
    {
        Batch->bAutoRebuildTreeOnInstanceChanges=false;
        Batch->PreAllocateInstancesMemory(900);
    }
    // Uneven tree heights and overlapping crowns form a continuous forest wall.
    // The center, merchant, and road remain open and readable.
    for (int32 I=0;I<76;++I)
    {
        const float A=I*2.399963f+Random.FRandRange(-.13f,.13f);
        const float R=Random.FRandRange(1170.f,1720.f);
        const FVector P(FMath::Cos(A)*R,FMath::Sin(A)*R,0);
        if (P.X>430.f && FMath::Abs(P.Y)<450.f) continue;
        const float H=Random.FRandRange(650.f,1120.f);
        const float Crown=Random.FRandRange(145.f,210.f);
        Trunks->AddInstance(FTransform(FRotator(0,Random.FRandRange(0.f,360.f),0),P+FVector(0,0,H*.5f),FVector(.48f,.48f,H/100.f)));
        for (int32 Layer=0;Layer<3;++Layer)
        {
            const float T=Layer/3.f;
            const FVector C=P+FVector(Random.FRandRange(-35.f,35.f),Random.FRandRange(-35.f,35.f),H*(.56f+T*.2f));
            auto* CrownBatch=(I+Layer)%3==0?DarkNeedles:Needles;
            CrownBatch->AddInstance(FTransform(FRotator(0,Random.FRandRange(0.f,360.f),0),C,
                FVector(Crown*(1.f-.28f*Layer)/50.f,Crown*(1.f-.28f*Layer)/50.f,H*(.5f-.08f*Layer)/100.f)));
        }
        if (I%3==0)
        {
            const FVector M=P+FVector(90.f,70.f,40.f);
            Moss->AddInstance(FTransform(FRotator::ZeroRotator,M,FVector(1.7f,1.15f,.65f)));
        }
        if (I%6==0)
            Rock(GetWorld(),P+FVector(Random.FRandRange(-180.f,180.f),Random.FRandRange(-180.f,180.f),10.f),
                FVector(Random.FRandRange(180.f,480.f),Random.FRandRange(220.f,570.f),Random.FRandRange(170.f,420.f)),
                FLinearColor(.17f,.19f,.14f),6000+I,false,false);
    }
    // Moss islands, fern banks, and loose stone break up the flat clearing.
    for (int32 I=0;I<330;++I)
    {
        const float A=Random.FRandRange(0.f,2.f*PI);
        const float R=Random.FRandRange(950.f,1740.f);
        const FVector P(FMath::Cos(A)*R,FMath::Sin(A)*R,0.f);
        if (P.X>380.f && FMath::Abs(P.Y)<350.f) continue;
        const float S=Random.FRandRange(.22f,.7f);
        Ferns->AddInstance(FTransform(FRotator(0,Random.FRandRange(0.f,360.f),0),P+FVector(0,0,20.f),FVector(S,S,Random.FRandRange(.65f,1.6f))));
        if (I%5==0)
            Moss->AddInstance(FTransform(FRotator::ZeroRotator,P+FVector(20.f,10.f,7.f),FVector(Random.FRandRange(.6f,1.8f),Random.FRandRange(.5f,1.6f),.13f)));
    }
    for (int32 I=0;I<170;++I)
    {
        const float A=Random.FRandRange(0.f,2.f*PI),R=Random.FRandRange(220.f,1700.f);
        const FVector P(FMath::Cos(A)*R,FMath::Sin(A)*R,8.f);
        if ((P.X>280.f && FMath::Abs(P.Y)<280.f) || (FMath::Abs(P.X)<350.f && FMath::Abs(P.Y)<850.f)) continue;
        Pebbles->AddInstance(FTransform(FRotator(Random.FRandRange(-20.f,20.f),0,0),P,
            FVector(Random.FRandRange(.18f,.58f),Random.FRandRange(.18f,.48f),Random.FRandRange(.1f,.28f))));
    }
    for (int32 I=0;I<55;++I)
    {
        const float A=Random.FRandRange(0.f,2.f*PI),R=Random.FRandRange(900.f,1700.f);
        Fireflies->AddInstance(FTransform(FRotator::ZeroRotator,
            FVector(FMath::Cos(A)*R,FMath::Sin(A)*R,Random.FRandRange(90.f,430.f)),FVector(Random.FRandRange(.018f,.045f))));
    }
    for (auto* Batch : {Trunks,Needles,DarkNeedles,Moss,Ferns,Pebbles,Fireflies})
        Batch->BuildTreeIfOutdated(true,true);

    // Warm road lanterns carry the eye from the fire to the level-select arch.
    for (int32 Side : {-1,1})
    {
        const FVector P(620.f,Side*330.f,0.f);
        auto* Post=Prop(P+FVector(0,0,170),FVector(.18f,.18f,3.4f),FLinearColor(.11f,.075f,.04f));
        Post->SetActorEnableCollision(false);
        if (auto* Lamp=Prop(P+FVector(0,0,350),FVector(.24f,.24f,.35f),FLinearColor(1.f,.5f,.1f),true))
        {
            Lamp->SetActorEnableCollision(false);
            Lamp->GetStaticMeshComponent()->SetMaterial(0,Surface(Lamp,FLinearColor(1.f,.36f,.06f),false,true));
        }
        if (auto* Glow=GetWorld()->SpawnActor<APointLight>(P+FVector(0,0,355),FRotator::ZeroRotator))
        {
            auto* Light=CastChecked<UPointLightComponent>(Glow->GetLightComponent());
            Light->SetMobility(EComponentMobility::Movable);
            Light->SetLightColor(FLinearColor(1.f,.43f,.16f));
            Light->SetIntensity(5200.f); Light->SetAttenuationRadius(720.f);
        }
    }
    if (auto* Glow=GetWorld()->SpawnActor<APointLight>(FVector(0,780,300),FRotator::ZeroRotator))
    {
        auto* Light=CastChecked<UPointLightComponent>(Glow->GetLightComponent());
        Light->SetMobility(EComponentMobility::Movable);
        Light->SetLightColor(FLinearColor(1.f,.59f,.29f));
        Light->SetIntensity(3300.f); Light->SetAttenuationRadius(730.f);
    }
}

void AArenaGameMode::BuildEnvironmentDetails()
{
    if (IsImpArena()) { BuildImpArenaDetails(); return; }
    if (auto* Fog=GetWorld()->SpawnActor<AExponentialHeightFog>(FVector(0,0,-80),FRotator::ZeroRotator))
    {
        auto* Component=Fog->GetComponent();
        Component->SetFogDensity(MapNumber==1 ? .009f : .014f);
        Component->SetFogHeightFalloff(.22f);
        Component->SetStartDistance(1500.f);
        Component->SetFogMaxOpacity(.65f);
        Component->SetFogInscatteringColor(MapNumber==1 ? FLinearColor(.018f,.028f,.065f)
            : MapNumber==2 ? FLinearColor(.28f,.075f,.035f) : FLinearColor(.075f,.035f,.17f));
    }
    if (auto* Post=GetWorld()->SpawnActor<APostProcessVolume>())
    {
        Post->bUnbound=true;
        auto& Settings=Post->Settings;
        if (MapNumber==1)
        {
            Settings.bOverride_AutoExposureMinBrightness=Settings.bOverride_AutoExposureMaxBrightness=true;
            Settings.AutoExposureMinBrightness=Settings.AutoExposureMaxBrightness=1.f;
        }
        Settings.bOverride_BloomIntensity=true; Settings.BloomIntensity=.25f;
        Settings.bOverride_AmbientOcclusionIntensity=true; Settings.AmbientOcclusionIntensity=.8f;
        Settings.bOverride_AmbientOcclusionRadius=true; Settings.AmbientOcclusionRadius=110.f;
        Settings.bOverride_VignetteIntensity=true; Settings.VignetteIntensity=.15f;
    }
    FRandomStream Random(1173+MapNumber*971);
    if (MapNumber==1)
    {
        auto* Grass=DecorationBatch(GetWorld(),TEXT("/Engine/BasicShapes/Cone.Cone"),FLinearColor(.15f,.19f,.09f));
        auto* Debris=DecorationBatch(GetWorld(),TEXT("/Engine/BasicShapes/Cube.Cube"),FLinearColor(.30f,.29f,.25f));
        // Small tufts and buried fragments cannot block attacks or push the camera.
        for (int32 I=0; I<700; ++I)
        {
            const float X=Random.FRandRange(-5800,5800), Y=Random.FRandRange(-4250,4250);
            if (FMath::Abs(Y-CastleTerrain::PathCenterY(X))<480.f) continue;
            const float Z=CastleTerrain::Height(X,Y);
            const float Height=Random.FRandRange(.14f,.38f);
            Grass->AddInstance(FTransform(FRotator(Random.FRandRange(-12,12),Random.FRandRange(0,360),0),
                FVector(X,Y,Z+Height*30.f),FVector(.045f,.06f,Height)));
        }
        for (int32 I=0; I<90; ++I)
        {
            const float X=Random.FRandRange(-5600,5600), Y=Random.FRandRange(-4100,4100);
            if (FMath::Abs(Y-CastleTerrain::PathCenterY(X))<320.f) continue;
            Debris->AddInstance(FTransform(FRotator(Random.FRandRange(-10,10),Random.FRandRange(0,360),Random.FRandRange(-8,8)),
                FVector(X,Y,CastleTerrain::Height(X,Y)+6.f),FVector(Random.FRandRange(.3f,1.1f),Random.FRandRange(.25f,.65f),.14f)));
        }
        // Keep all solid boulders outside the encounter courtyard and approach.
        for (int32 I=0; I<38; ++I)
        {
            const float X=Random.FRandRange(-5750,5750);
            const float Y=Random.FRandRange(2300,4200)*(I%2 ? 1.f : -1.f);
            const float Width=Random.FRandRange(260,650);
            if (auto* Boulder=Rock(GetWorld(),FVector(X,Y,CastleTerrain::Height(X,Y)+Random.FRandRange(130,240)),
                FVector(Width,Width*Random.FRandRange(.6f,1.2f),Random.FRandRange(190,330)),FLinearColor(.31f,.32f,.27f),I+77,true,false))
                Boulder->SetActorRotation(FRotator(Random.FRandRange(-17,17),Random.FRandRange(0,360),Random.FRandRange(-15,15)));
        }
        // Rubble shoulders frame each entrance without narrowing the gateway.
        for (float X : {-3400.f,3300.f})
            for (int32 Side : {-1,1})
                for (int32 I=0; I<4; ++I)
                {
                    const float RX=X+Random.FRandRange(-330,330), RY=Side*Random.FRandRange(880,1220);
                    auto* Block=Prop(FVector(RX,RY,CastleTerrain::Height(RX,RY)+70.f),FVector(1.9f,1.3f,1.1f),FLinearColor(.30f,.29f,.26f));
                    if (Block) Block->SetActorRotation(FRotator(Random.FRandRange(-20,20),Random.FRandRange(0,360),Random.FRandRange(-20,20)));
                }
        // Distant silhouettes sit beyond the playable boundary.
        for (int32 I=0; I<18; ++I)
        {
            const float Angle=2.f*PI*I/18.f;
            Rock(GetWorld(),FVector(7600.f*FMath::Cos(Angle),6200.f*FMath::Sin(Angle),Random.FRandRange(800,1600)),
                FVector(Random.FRandRange(1800,2700),Random.FRandRange(1300,2100),1100.f),FLinearColor(.18f,.22f,.23f),I+900,false,false);
        }
    }
    else
    {
        auto* Crystals=DecorationBatch(GetWorld(),TEXT("/Engine/BasicShapes/Cone.Cone"),MapNumber==2
            ? FLinearColor(.4f,.045f,.008f) : FLinearColor(.20f,.12f,.65f),MapNumber==3);
        for (int32 I=0; I<MapPlatforms.Num(); ++I)
        {
            const auto& P=MapPlatforms[I];
            if (P.Z<1000.f) continue; // Leave the small jumping stones unobstructed.
            for (int32 Corner=0; Corner<4; ++Corner)
            {
                const float X=P.X+(Corner%2 ? 1.f : -1.f)*P.Z*.40f;
                const float Y=P.Y+(Corner/2 ? 1.f : -1.f)*P.W*.40f;
                Crystals->AddInstance(FTransform(FRotator(Random.FRandRange(-15,15),Random.FRandRange(0,360),0),
                    FVector(X,Y,100.f),FVector(.65f,.85f,Random.FRandRange(2.f,4.f))));
            }
        }
        if (MapNumber==3)
        {
            // Suspended fragments underneath the route reveal the void's depth.
            for (int32 I=0; I<24; ++I)
                Rock(GetWorld(),FVector(Random.FRandRange(-6800,6400),Random.FRandRange(-4200,4200),Random.FRandRange(-1700,-650)),
                    FVector(Random.FRandRange(130,420),Random.FRandRange(100,380),Random.FRandRange(200,450)),
                    FLinearColor(.21f,.13f,.29f),I+650,false,false);
        }
    }
}

void AArenaGameMode::BuildImpArenaDetails()
{
    FRandomStream Random(92022);
    UE_LOG(LogTemp,Display,TEXT("IMP ARENA BUILD: sea"));
    // The luminous sea is one continuous surface, well below the playable rock.
    if (auto* Lava=Prop(FVector(0,0,-1080),FVector(420,420,.5f),FLinearColor(.42f,.018f,.004f)))
    {
        auto* Mat=Surface(Lava,FLinearColor(.75f,.025f,.003f),false,true);
        Mat->SetScalarParameterValue(TEXT("EmissiveStrength"),3.3f);
        Mat->SetScalarParameterValue(TEXT("TextureScale"),.0008f);
        Lava->GetStaticMeshComponent()->SetMaterial(0,Mat);
        Lava->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Lava->Tags.Add(TEXT("ImpArenaLava"));
    }
    auto* Currents=DecorationBatch(GetWorld(),TEXT("/Engine/BasicShapes/Cube.Cube"),FLinearColor(1.f,.11f,.014f),true);
    Currents->bAutoRebuildTreeOnInstanceChanges=false;
    Currents->PreAllocateInstancesMemory(90);
    for (int32 I=0;I<90;++I)
    {
        const float A=Random.FRandRange(0.f,2.f*PI),R=Random.FRandRange(6000.f,18000.f);
        Currents->AddInstance(FTransform(FRotator(0,Random.FRandRange(0.f,360.f),0),
            FVector(FMath::Cos(A)*R,FMath::Sin(A)*R,-1040.f),
            FVector(Random.FRandRange(2.f,7.f),Random.FRandRange(.09f,.24f),.015f)));
    }
    Currents->BuildTreeIfOutdated(true,true);
    UE_LOG(LogTemp,Display,TEXT("IMP ARENA BUILD: cavern"));
    // An enclosed cavern silhouette: fractured walls, roof, and dense rock teeth.
    for (int32 I=0;I<32;++I)
    {
        const float A=2.f*PI*I/32.f+Random.FRandRange(-.055f,.055f);
        const float R=Random.FRandRange(9600.f,12500.f);
        const FVector P(FMath::Cos(A)*R,FMath::Sin(A)*R,Random.FRandRange(850.f,2100.f));
        if (auto* Wall=Rock(GetWorld(),P,
            FVector(Random.FRandRange(2800.f,5000.f),Random.FRandRange(2500.f,4800.f),Random.FRandRange(3900.f,6400.f)),
            FLinearColor(.105f,.075f,.073f),I+8100,false,false))
        {
            Wall->SetActorRotation(FRotator(Random.FRandRange(-8.f,8.f),FMath::RadiansToDegrees(A),Random.FRandRange(-14.f,14.f)));
            Wall->Tags.Add(TEXT("ImpArenaCavern"));
        }
    }
    if (auto* Roof=Rock(GetWorld(),FVector(0,0,9200),FVector(34500,34500,3600),
        FLinearColor(.09f,.055f,.055f),8765,false,false))
    {
        Roof->Tags.Add(TEXT("ImpArenaRoof"));
        if (auto* Mesh=Cast<UPrimitiveComponent>(Roof->GetRootComponent())) Mesh->SetCastShadow(false);
    }
    auto* Teeth=DecorationBatch(GetWorld(),TEXT("/Engine/BasicShapes/Cone.Cone"),FLinearColor(.14f,.095f,.086f));
    Teeth->bAutoRebuildTreeOnInstanceChanges=false;
    Teeth->PreAllocateInstancesMemory(260);
    for (int32 I=0;I<260;++I)
    {
        const float A=Random.FRandRange(0.f,2.f*PI),R=Random.FRandRange(5700.f,15000.f);
        const bool Hanging=I%3==0;
        const float H=Random.FRandRange(5.f,27.f);
        Teeth->AddInstance(FTransform(FRotator(Hanging?180.f:Random.FRandRange(-12.f,12.f),Random.FRandRange(0.f,360.f),0),
            FVector(FMath::Cos(A)*R,FMath::Sin(A)*R,Hanging?Random.FRandRange(4800.f,7200.f):-850.f+H*50.f),
            FVector(Random.FRandRange(2.f,8.f),Random.FRandRange(2.f,8.f),H)));
    }
    Teeth->BuildTreeIfOutdated(true,true);
    UE_LOG(LogTemp,Display,TEXT("IMP ARENA BUILD: rim"));
    // Irregular spalls around the rim leave the combat surface flat and open.
    for (int32 I=0;I<45;++I)
    {
        const float A=2.f*PI*I/45.f,R=Random.FRandRange(5100.f,5700.f);
        Rock(GetWorld(),FVector(FMath::Cos(A)*R,FMath::Sin(A)*R,Random.FRandRange(-130.f,20.f)),
            FVector(Random.FRandRange(220.f,510.f),Random.FRandRange(170.f,470.f),Random.FRandRange(250.f,520.f)),
            FLinearColor(.17f,.14f,.13f),I+9200,false,false);
    }
    // Wrought iron anchors surround an open center. The chains hang above players.
    auto* Iron=DecorationBatch(GetWorld(),TEXT("/Engine/BasicShapes/Cylinder.Cylinder"),FLinearColor(.11f,.09f,.085f));
    Iron->GetOwner()->Tags.Add(TEXT("ImpArenaChains"));
    Iron->bAutoRebuildTreeOnInstanceChanges=false;
    Iron->PreAllocateInstancesMemory(1100);
    auto Beam=[Iron](FVector From,FVector To,float Width)
    {
        const FVector Delta=To-From;
        if (Delta.IsNearlyZero()) return;
        Iron->AddInstance(FTransform(FQuat::FindBetweenNormals(FVector::UpVector,Delta.GetSafeNormal()),
            (From+To)*.5f,FVector(Width/50.f,Width/50.f,Delta.Size()/100.f)));
    };
    for (int32 I=0;I<8;++I)
    {
        const float A=2.f*PI*I/8.f;
        const FVector Anchor(FMath::Cos(A)*1400.f,FMath::Sin(A)*1400.f,0.f);
        Beam(Anchor,FVector(Anchor.X,Anchor.Y,770.f),24.f);
        Beam(FVector(Anchor.X,Anchor.Y,760.f),FVector(Anchor.X,Anchor.Y,830.f),53.f);
        const FVector End(FMath::Cos(A)*290.f,FMath::Sin(A)*290.f,1160.f);
        FVector Last=FVector(Anchor.X,Anchor.Y,820.f);
        for (int32 Link=1;Link<=15;++Link)
        {
            const float T=Link/15.f;
            const FVector Next=FMath::Lerp(FVector(Anchor.X,Anchor.Y,820.f),End,T)-FVector(0,0,160.f*FMath::Sin(PI*T));
            const FVector Dir=(Next-Last).GetSafeNormal();
            const FVector Side=FVector::CrossProduct(Dir,FVector::UpVector).GetSafeNormal();
            const FVector Span=Link%2 ? Side : FVector::CrossProduct(Dir,Side).GetSafeNormal();
            for (int32 K=0;K<8;++K)
            {
                const float B=2.f*PI*K/8.f,C=2.f*PI*(K+1)/8.f;
                const FVector Mid=(Last+Next)*.5f;
                Beam(Mid+Dir*42.f*FMath::Cos(B)+Span*20.f*FMath::Sin(B),
                     Mid+Dir*42.f*FMath::Cos(C)+Span*20.f*FMath::Sin(C),6.f);
            }
            Last=Next;
        }
    }
    for (int32 I=0;I<24;++I)
    {
        const float A=2.f*PI*I/24.f,B=2.f*PI*(I+1)/24.f;
        Beam(FVector(FMath::Cos(A)*290,FMath::Sin(A)*290,1160),
             FVector(FMath::Cos(B)*290,FMath::Sin(B)*290,1160),18.f);
    }
    Iron->BuildTreeIfOutdated(true,true);
    UE_LOG(LogTemp,Display,TEXT("IMP ARENA BUILD: embers"));
    auto* Embers=DecorationBatch(GetWorld(),TEXT("/Engine/BasicShapes/Sphere.Sphere"),FLinearColor(1.f,.14f,.015f),true);
    Embers->bAutoRebuildTreeOnInstanceChanges=false;
    Embers->PreAllocateInstancesMemory(370);
    for (int32 I=0;I<370;++I)
    {
        const float A=Random.FRandRange(0.f,2.f*PI),R=Random.FRandRange(5500.f,16500.f);
        const float S=Random.FRandRange(.025f,.085f);
        Embers->AddInstance(FTransform(FRotator::ZeroRotator,
            FVector(FMath::Cos(A)*R,FMath::Sin(A)*R,Random.FRandRange(-550.f,4100.f)),FVector(S)));
    }
    Embers->BuildTreeIfOutdated(true,true);
    UE_LOG(LogTemp,Display,TEXT("IMP ARENA BUILD: lighting"));
    for (int32 I=0;I<6;++I)
    {
        const float A=2.f*PI*I/6.f;
        if (auto* Glow=GetWorld()->SpawnActor<APointLight>(FVector(FMath::Cos(A)*4800.f,FMath::Sin(A)*4800.f,-180.f),FRotator::ZeroRotator))
        {
            auto* Light=CastChecked<UPointLightComponent>(Glow->GetLightComponent());
            Light->SetMobility(EComponentMobility::Movable);
            Light->SetLightColor(FLinearColor(1.f,.11f,.025f));
            Light->SetIntensity(23000.f);
            Light->SetAttenuationRadius(3900.f);
        }
    }
    if (auto* Fog=GetWorld()->SpawnActor<AExponentialHeightFog>(FVector(0,0,-1000),FRotator::ZeroRotator))
    {
        auto* Component=Fog->GetComponent();
        Component->SetFogDensity(.028f);
        Component->SetFogHeightFalloff(.12f);
        Component->SetStartDistance(2200.f);
        Component->SetFogMaxOpacity(.94f);
        Component->SetFogInscatteringColor(FLinearColor(.34f,.028f,.018f));
    }
    if (auto* Post=GetWorld()->SpawnActor<APostProcessVolume>())
    {
        Post->bUnbound=true;
        auto& Settings=Post->Settings;
        Settings.bOverride_AutoExposureMinBrightness=Settings.bOverride_AutoExposureMaxBrightness=true;
        Settings.AutoExposureMinBrightness=Settings.AutoExposureMaxBrightness=1.f;
        Settings.bOverride_BloomIntensity=true; Settings.BloomIntensity=.65f;
        Settings.bOverride_AmbientOcclusionIntensity=true; Settings.AmbientOcclusionIntensity=1.f;
        Settings.bOverride_VignetteIntensity=true; Settings.VignetteIntensity=.3f;
    }
    UE_LOG(LogTemp,Display,TEXT("IMP ARENA BUILD: complete"));
}

void AArenaGameMode::RunMapVisualCheck(float Dt)
{
#if WITH_DEV_AUTOMATION_TESTS
    if (!FParse::Param(FCommandLine::Get(),TEXT("HellgirlMapPreview"))) return;
    static float Elapsed=0.f;
    static int32 Phase=0;
    static bool CommanderPreview=false;
    Elapsed+=Dt;
    if (Phase==0 && Elapsed>1.f)
    {
        for (auto S : SpawnSites) S->SetActorTickEnabled(false);
        FString View; FParse::Value(FCommandLine::Get(),TEXT("MapPreviewView="),View);
        CommanderPreview=View==TEXT("commander") && MapNumber==1;
        if (CommanderPreview)
        {
            for (auto S : SpawnSites) { S->bCleared=true; S->bEnabled=false; }
            auto* Site=SpawnSites.Last().Get();
            Site->bCleared=false; Site->bEnabled=Site->bActivated=true; Site->EnemyCount=0;
            auto* Boss=GetWorld()->SpawnActor<AArenaFighter>(Site->GetActorLocation()+FVector(0,0,142),FRotator::ZeroRotator);
            if (Boss)
            {
                Boss->MakeEnemy(1,false); Boss->SetEnemyType(EHellgirlEnemyType::ImpCommander);
                Boss->SetActorScale3D(FVector(1.5f)); Boss->MaxHealth=450.f; Boss->Health=225.f;
                Boss->HomePosition=Site->GetActorLocation(); Site->RegisterReinforcement(Boss);
            }
            if (auto* Player=Cast<AArenaFighter>(UGameplayStatics::GetPlayerPawn(this,0)))
            {
                Player->SetActorLocation(Site->GetActorLocation()+FVector(-520,-120,115));
                Player->ResetAfterRecovery(); Player->Health=Player->MaxHealth=10000.f;
                if (auto* PC=Cast<APlayerController>(Player->GetController()))
                {
                    Player->DisableInput(PC); PC->SetIgnoreMoveInput(true); PC->SetIgnoreLookInput(true);
                    PC->SetControlRotation(FRotator(-16,13,0));
                }
            }
        }
        if (View==TEXT("overview"))
        {
            const FVector Position=IsImpArena() ? FVector(0,-8800,4200)
                : MapNumber==1 ? FVector(-7000,-7200,6500) : FVector(-7200,-7800,7400);
            auto* Camera=GetWorld()->SpawnActor<ACameraActor>(Position,(-Position+FVector(0,0,-300)).Rotation());
            Camera->GetCameraComponent()->SetFieldOfView(70.f);
            if (auto* PC=UGameplayStatics::GetPlayerController(this,0)) PC->SetViewTarget(Camera);
        }
        Phase=1;
    }
    if (Phase==1 && Elapsed>(CommanderPreview ? 4.5f : 3.f))
    {
        FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/MapPreview.png"),false,false);
        Phase=2;
    }
    if (Phase==2 && Elapsed>(CommanderPreview ? 5.5f : 4.f))
    {
        Phase=3;
        UE_LOG(LogTemp,Display,TEXT("MAP VISUAL CHECK PASSED: captured map %d"),MapNumber);
        FPlatformMisc::RequestExitWithStatus(false,0);
    }
#endif
}
