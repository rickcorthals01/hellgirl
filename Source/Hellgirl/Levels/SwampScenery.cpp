// The swamp's scenery: muddy ground, the glowing soul water with ragged shores, the ring of dead trees, the water's
// glow, mist, fireflies, the light at the end and a dark overcast night. The planned pieces (boardwalk, islands,
// lily pads, arms...) are placed in Swamp.cpp. Art: Tools/Environment/build_swamp_kit.ps1.
#include "Levels/ArenaGameMode.h"
#include "Levels/ForestArt.h"
#include "Levels/GraveyardArt.h"
#include "Rules/SwampRules.h"
#include "ProceduralMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace SwampArt
{
UStaticMesh* Kit(const TCHAR* Name)
{
    return LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/Environment/Swamp/Kit/%s.%s"), Name, Name));
}

UMaterialInterface* Material(const TCHAR* Name)
{
    return LoadObject<UMaterialInterface>(nullptr, *FString::Printf(TEXT("/Game/Environment/Swamp/%s.%s"), Name, Name));
}

// Flat inside the tree ring; the mud rises a little under the trees and beyond.
float GroundHeight(FVector2D P)
{
    const float Out = FMath::Max3(0.f, static_cast<float>(FMath::Abs(P.X)) - SwampRoom::Half - 300.f, static_cast<float>(FMath::Abs(P.Y)) - SwampRoom::HalfWidth - 300.f);
    return Out * .1f * (.6f + .4f * FMath::PerlinNoise2D(P * .001f));
}

// The water's surface from the west shore to the east shore, both shores ragged, reaching under the tree ring on the
// long sides (world-space UVs for the material's drifting veins).
void Water(UWorld* World, float West, float East, UMaterialInterface* Material, int32 Seed)
{
    auto* Actor = World->SpawnActor<AActor>();
    auto* Mesh = NewObject<UProceduralMeshComponent>(Actor);
    Actor->SetRootComponent(Mesh);
    Actor->AddInstanceComponent(Mesh);
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Mesh->SetCastShadow(false);
    Mesh->RegisterComponent();
    TArray<FVector> Vertices, Normals;
    TArray<FVector2D> UVs;
    TArray<int32> Triangles;
    const float Reach = SwampRoom::HalfWidth + 1200.f;
    constexpr int32 Steps = 60;
    for (int32 I = 0; I <= Steps; ++I)
    {
        const float Y = -Reach + 2.f * Reach * I / Steps;
        for (const float Shore : {West, East})
        {
            const float Ragged = 180.f * FMath::PerlinNoise1D(Y * .0011f + Shore * .01f + Seed) + 70.f * FMath::PerlinNoise1D(Y * .004f + Shore + Seed);
            const FVector P(Shore + Ragged, Y, SwampRoom::WaterZ);
            Vertices.Add(P);
            Normals.Add(FVector::UpVector);
            UVs.Add(FVector2D(P.X, P.Y) / 650.f);
        }
        if (I < Steps)
        {
            const int32 A = I * 2;
            Triangles.Append({A, A + 2, A + 1, A + 1, A + 2, A + 3});
        }
    }
    // Clockwise from above faces up in Unreal.
    if (FVector::CrossProduct(Vertices[2] - Vertices[0], Vertices[1] - Vertices[0]).Z > 0.f)
        for (int32 I = 0; I < Triangles.Num(); I += 3) Swap(Triangles[I + 1], Triangles[I + 2]);
    Mesh->CreateMeshSection(0, Vertices, Triangles, Normals, UVs, {}, {}, false);
    Mesh->SetMaterial(0, Material);
    Actor->Tags.Add(TEXT("SwampWater"));
}

// An overcast night: no moon through the clouds, a faint cold light from above, thick low green-grey fog. The water
// is the brightest thing in the swamp.
void Night(UWorld* World)
{
    if (auto* Dome = ForestArt::Solid(World, LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")), FTransform(FRotator::ZeroRotator, FVector::ZeroVector, FVector(800.f)), false))
    {
        Dome->GetStaticMeshComponent()->SetCastShadow(false);
        if (auto* Sky = UMaterialInstanceDynamic::Create(GraveArt::Material(TEXT("M_GraveSky")), Dome))
        {
            Sky->SetVectorParameterValue(TEXT("Zenith"), FLinearColor(.002f, .004f, .004f));
            Sky->SetVectorParameterValue(TEXT("Horizon"), FLinearColor(.01f, .022f, .02f));
            Sky->SetVectorParameterValue(TEXT("MoonColor"), FLinearColor(.05f, .07f, .07f));
            Sky->SetVectorParameterValue(TEXT("MoonDir"), FLinearColor(FRotator(30.f, 40.f, 0.f).Vector()));
            Sky->SetScalarParameterValue(TEXT("MoonBrightness"), 0.f);
            Sky->SetScalarParameterValue(TEXT("StarBrightness"), .25f);
            Sky->SetScalarParameterValue(TEXT("CloudCover"), .3f);
            Sky->SetScalarParameterValue(TEXT("CloudScale"), 1.3f);
            Dome->GetStaticMeshComponent()->SetMaterial(0, Sky);
        }
    }
    auto* Top = World->SpawnActor<ADirectionalLight>(FVector(0, 0, 2000), FRotator(-52.f, 30.f, 0.f));
    auto* TopComponent = CastChecked<UDirectionalLightComponent>(Top->GetLightComponent());
    TopComponent->SetMobility(EComponentMobility::Movable);
    TopComponent->SetIntensity(.8f);
    TopComponent->SetLightColor(FLinearColor(.45f, .62f, .66f));
    TopComponent->SetVolumetricScatteringIntensity(1.f);
    TopComponent->SetDynamicShadowDistanceMovableLight(8000.f);
    auto* Fill = World->SpawnActor<ADirectionalLight>(FVector(0, 0, 1500), FRotator(-35.f, 180.f, 0.f));
    auto* FillComponent = CastChecked<UDirectionalLightComponent>(Fill->GetLightComponent());
    FillComponent->SetMobility(EComponentMobility::Movable);
    FillComponent->SetIntensity(.3f);
    FillComponent->SetLightColor(FLinearColor(.35f, .55f, .65f));
    FillComponent->SetCastShadows(false);
    FillComponent->SetVolumetricScatteringIntensity(0.f);
    auto* Sky = World->SpawnActor<ASkyLight>();
    Sky->GetLightComponent()->SetMobility(EComponentMobility::Movable);
    Sky->GetLightComponent()->SetIntensity(1.f);
    Sky->GetLightComponent()->SetLightColor(FLinearColor(.35f, .5f, .5f));
    Sky->GetLightComponent()->SetRealTimeCaptureEnabled(false);
    auto* Fog = World->SpawnActor<AExponentialHeightFog>();
    auto* FogComponent = Fog->GetComponent();
    FogComponent->SetFogDensity(.05f);
    FogComponent->SetFogHeightFalloff(.3f);
    FogComponent->SetFogInscatteringColor(FLinearColor(.012f, .028f, .028f));
    FogComponent->SetStartDistance(400.f);
    FogComponent->SetFogCutoffDistance(30000.f);
    FogComponent->SetVolumetricFog(true);
    FogComponent->SetVolumetricFogScatteringDistribution(.6f);
    FogComponent->SetVolumetricFogAlbedo(FColor(150, 190, 185));
    FogComponent->SetVolumetricFogExtinctionScale(.8f);
    auto* Post = World->SpawnActor<APostProcessVolume>();
    Post->bUnbound = true;
    auto& S = Post->Settings;
    S.bOverride_AutoExposureMinBrightness = S.bOverride_AutoExposureMaxBrightness = true;
    S.AutoExposureMinBrightness = S.AutoExposureMaxBrightness = 1.f;
    S.bOverride_BloomIntensity = true; S.BloomIntensity = .9f;
    S.bOverride_VignetteIntensity = true; S.VignetteIntensity = .65f;
    S.bOverride_AmbientOcclusionIntensity = true; S.AmbientOcclusionIntensity = .8f;
    S.bOverride_ColorGammaShadows = true; S.ColorGammaShadows = FVector4(.94f, 1.02f, 1.02f, 1.f);
    S.bOverride_ColorContrast = true; S.ColorContrast = FVector4(1.1f, 1.1f, 1.1f, 1.f);
    S.bOverride_ColorSaturation = true; S.ColorSaturation = FVector4(.9f, .9f, .9f, 1.f);
    S.bOverride_FilmGrainIntensity = true; S.FilmGrainIntensity = .06f;
}
}

void AArenaGameMode::BuildSwampScenery()
{
    using namespace SwampRoom;
    UWorld* World = GetWorld();
    const FPlan Plan = Make(SwampSeed, SwampRoomNumber, SwampRooms, bSwampKingRoom);
    FRandomStream Dice(SwampSeed * 29 + SwampRoomNumber * 11);

    ForestArt::Ground(World, Half + 2500.f, 420.f, SwampArt::Material(TEXT("MI_SwampGround")), [](FVector2D P) { return SwampArt::GroundHeight(P); });
    SwampArt::Water(World, Plan.WaterStart, Plan.WaterEnd, SwampArt::Material(TEXT("M_SwampWater")), SwampSeed % 97);

    // The tree ring: several rows of dead trees all the way round, closing the corridor on every side.
    UStaticMesh* TreeMeshes[] = {SwampArt::Kit(TEXT("SM_SwampTreeA")), SwampArt::Kit(TEXT("SM_SwampTreeB")), SwampArt::Kit(TEXT("SM_SwampTreeC")), ForestArt::Kit(TEXT("SM_DeadTree"))};
    UHierarchicalInstancedStaticMeshComponent* Trees[4];
    for (int32 I = 0; I < 4; ++I) Trees[I] = GraveArt::Batch(World, TreeMeshes[I], false, 2.5f);
    auto Plant = [&](FVector2D P, float Scale)
    {
        Trees[Dice.RandRange(0, 3)]->AddInstance(FTransform(FRotator(0, Dice.FRandRange(0.f, 360.f), 0), FVector(P.X, P.Y, SwampArt::GroundHeight(P) - 15.f), FVector(Scale)));
    };
    for (int32 Row = 0; Row < 4; ++Row)
    {
        const float Out = 180.f + Row * 420.f;
        for (float X = -Half - Out; X <= Half + Out; X += Dice.FRandRange(260.f, 380.f))
            for (float Side : {-1.f, 1.f})
                Plant(FVector2D(X + Dice.FRandRange(-80.f, 80.f), Side * (HalfWidth + Out + Dice.FRandRange(-90.f, 90.f))), Dice.FRandRange(.9f, 1.45f) + Row * .1f);
        for (float Y = -HalfWidth - Out; Y <= HalfWidth + Out; Y += Dice.FRandRange(260.f, 380.f))
            for (float Side : {-1.f, 1.f})
                Plant(FVector2D(Side * (Half + Out + Dice.FRandRange(-90.f, 90.f)), Y + Dice.FRandRange(-80.f, 80.f)), Dice.FRandRange(.9f, 1.45f) + Row * .1f);
    }
    for (auto* Batch : Trees) Batch->BuildTreeIfOutdated(true, true);
    // Invisible walls just inside the tree ring.
    for (int32 Side = 0; Side < 4; ++Side)
    {
        const bool Long = Side < 2;
        const float S = Side % 2 ? 1.f : -1.f;
        auto* Edge = Prop(Long ? FVector(0.f, S * (HalfWidth + 60.f), 2900.f) : FVector(S * (Half + 60.f), 0.f, 2900.f),
            Long ? FVector(Half * 2.2f / 100.f, .8f, 60.f) : FVector(.8f, HalfWidth * 2.4f / 100.f, 60.f), FLinearColor::Black);
        Edge->SetActorHiddenInGame(true);
        Edge->Tags.Add(TEXT("SwampBoundary"));
    }

    // The water lights its surroundings from below: soft blue glows just over it along the corridor.
    int32 Alternate = 0;
    for (float X = Plan.WaterStart + 400.f; X < Plan.WaterEnd - 200.f; X += 950.f)
        ForestArt::PointGlow(World, FVector(X, (Alternate++ % 2 ? 1.f : -1.f) * 650.f, 70.f), FLinearColor(.35f, .7f, 1.f), 4200.f, 1400.f);

    // The light at the end: a big lantern on a crooked post holding a warm, glowing orb, bright enough to be seen
    // down the whole corridor.
    ForestArt::Solid(World, SwampArt::Kit(TEXT("SM_ExitLantern")), FTransform(FRotator(0.f, 180.f, 0.f), FVector(Exit.X + 160.f, -80.f, 0.f), FVector(1.5f)), false);
    const FVector Flame(Exit.X + 160.f - 117.f, -80.f, 330.f);
    if (auto* Orb = ForestArt::Solid(World, LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")), FTransform(FRotator::ZeroRotator, Flame, FVector(.45f)), false))
        if (auto* Warm = UMaterialInstanceDynamic::Create(LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Environment/Materials/MI_ForestKitGlow.MI_ForestKitGlow")), Orb))
        {
            Warm->SetVectorParameterValue(TEXT("Tint"), FLinearColor(1.f, .75f, .45f));
            Warm->SetScalarParameterValue(TEXT("Glow"), 14.f);
            Orb->GetStaticMeshComponent()->SetMaterial(0, Warm);
            Orb->GetStaticMeshComponent()->SetCastShadow(false);
        }
    ForestArt::PointGlow(World, Flame, FLinearColor(1.f, .78f, .5f), 32000.f, 2200.f);

    for (const FVector& Swarm : Plan.Fireflies) ForestArt::Fireflies(World, Swarm, 1.5f);
    GraveArt::MistSheet(World, 45.f, Half + 2500.f, 1600.f, .45f, .8f, FLinearColor(.1f, .15f, .15f));
    GraveArt::MistSheet(World, 120.f, Half + 2500.f, 2300.f, .22f, .6f, FLinearColor(.09f, .13f, .13f));
    SwampArt::Night(World);
}
