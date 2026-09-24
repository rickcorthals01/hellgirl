// The forest run's scenery: ground, trees, plants, dressing around the rolled cover and thorns, sky and light.
// Meshes come from the generated forest kit (Tools/Environment), Rock_Collection_04, Pack_Bonus (ground),
// Realistic_Starter_VFX_Pack_Vol2 (fireflies) and Stylish_Fire_VFX. Gameplay pieces are built in ForestRun.cpp.
#include "Levels/ArenaGameMode.h"
#include "Levels/ForestArt.h"
#include "Rules/ForestRoomRules.h"
#include "ProceduralMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/PointLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Particles/ParticleSystem.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Misc/Paths.h"

namespace ForestArt
{
UStaticMesh* Kit(const TCHAR* Name)
{
    return LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/Environment/ForestKit/%s.%s"), Name, Name));
}

UStaticMesh* Boulder(int32 Index)
{
    const int32 N = 1 + FMath::Abs(Index) % 7;
    return LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/Rock_Collection_04/Meshes/Rock_0%d/StaticMeshes/SM_Rock_0%d.SM_Rock_0%d"), N, N, N));
}

UHierarchicalInstancedStaticMeshComponent* Batch(UWorld* World, UStaticMesh* Mesh, bool Shadows, float Sway, float CullDistance)
{
    auto* Actor = World->SpawnActor<AActor>();
    auto* Instances = NewObject<UHierarchicalInstancedStaticMeshComponent>(Actor);
    Actor->SetRootComponent(Instances);
    Actor->AddInstanceComponent(Instances);
    Instances->SetStaticMesh(Mesh);
    Instances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Instances->SetGenerateOverlapEvents(false);
    Instances->SetCastShadow(Shadows);
    Instances->bAutoRebuildTreeOnInstanceChanges = false;
    if (CullDistance > 0.f) Instances->SetCullDistances(CullDistance * .8f, CullDistance);
    if (Sway >= 0.f && Mesh)
        if (auto* Material = UMaterialInstanceDynamic::Create(Mesh->GetMaterial(0), Instances))
        {
            Material->SetScalarParameterValue(TEXT("SwayAmount"), Sway);
            Instances->SetMaterial(0, Material);
        }
    Instances->RegisterComponent();
    return Instances;
}

AStaticMeshActor* Solid(UWorld* World, UStaticMesh* Mesh, const FTransform& Where, bool Collide)
{
    auto* Actor = World->SpawnActor<AStaticMeshActor>(Where.GetLocation(), Where.Rotator());
    if (!Actor) return nullptr;
    Actor->SetMobility(EComponentMobility::Movable);
    Actor->GetStaticMeshComponent()->SetStaticMesh(Mesh);
    Actor->GetStaticMeshComponent()->SetCollisionProfileName(Collide ? TEXT("BlockAll") : TEXT("NoCollision"));
    Actor->SetActorScale3D(Where.GetScale3D());
    return Actor;
}

AStaticMeshActor* Stand(UWorld* World, UStaticMesh* Mesh, FVector2D Where, float GroundZ, float Yaw, float Scale, bool Collide, float Sink)
{
    if (!Mesh) return nullptr;
    const FBoxSphereBounds B = Mesh->GetBounds();
    const float Bottom = static_cast<float>(B.Origin.Z - B.BoxExtent.Z);
    return Solid(World, Mesh, FTransform(FRotator(0, Yaw, 0), FVector(Where.X, Where.Y, GroundZ - Bottom * Scale - Sink), FVector(Scale)), Collide);
}

float GroundHeight(FVector2D P)
{
    // Flat where anyone can walk; the forest floor rises gently behind the tree line.
    const float Out = FMath::Max(0.f, static_cast<float>(P.Size()) - ForestRoom::Radius - 150.f);
    return Out * .09f * (.7f + .3f * FMath::PerlinNoise2D(P * .0011f));
}

void Ground(UWorld* World, float Extent, float TileSize, UMaterialInterface* Material)
{
    auto* Actor = World->SpawnActor<AActor>();
    auto* Mesh = NewObject<UProceduralMeshComponent>(Actor);
    Actor->SetRootComponent(Mesh);
    Actor->AddInstanceComponent(Mesh);
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Mesh->RegisterComponent();
    constexpr int32 Cells = 110;
    TArray<FVector> Vertices, Normals;
    TArray<FVector2D> UVs;
    TArray<int32> Triangles;
    const float Step = Extent * 2.f / Cells;
    for (int32 Y = 0; Y <= Cells; ++Y)
        for (int32 X = 0; X <= Cells; ++X)
        {
            const FVector2D P(-Extent + X * Step, -Extent + Y * Step);
            Vertices.Add(FVector(P.X, P.Y, GroundHeight(P)));
            const float DX = GroundHeight(P + FVector2D(20.f, 0.f)) - GroundHeight(P - FVector2D(20.f, 0.f));
            const float DY = GroundHeight(P + FVector2D(0.f, 20.f)) - GroundHeight(P - FVector2D(0.f, 20.f));
            Normals.Add(FVector(-DX, -DY, 40.f).GetSafeNormal());
            UVs.Add(P / TileSize);
        }
    for (int32 Y = 0; Y < Cells; ++Y)
        for (int32 X = 0; X < Cells; ++X)
        {
            const int32 A = Y * (Cells + 1) + X, B = A + 1, C = A + Cells + 1, D = C + 1;
            Triangles.Append({A, C, B, B, C, D});
        }
    Mesh->CreateMeshSection(0, Vertices, Triangles, Normals, UVs, {}, {}, false);
    Mesh->SetMaterial(0, Material);
}

void NightSky(UWorld* World, FVector MoonDirection)
{
    auto* Dome = Solid(World, LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")), FTransform(FRotator::ZeroRotator, FVector::ZeroVector, FVector(800.f)), false);
    if (!Dome) return;
    Dome->GetStaticMeshComponent()->SetCastShadow(false);
    if (auto* Sky = UMaterialInstanceDynamic::Create(LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Environment/Materials/M_ForestSky.M_ForestSky")), Dome))
    {
        Sky->SetVectorParameterValue(TEXT("MoonDir"), FLinearColor(MoonDirection));
        Dome->GetStaticMeshComponent()->SetMaterial(0, Sky);
    }
}

void Moonlight(UWorld* World, FRotator MoonLight, float Fog)
{
    // Low moon ahead of the default camera: long shadows toward the player, shafts in the fog.
    auto* Moon = World->SpawnActor<ADirectionalLight>(FVector(0, 0, 2000), MoonLight);
    auto* MoonComponent = CastChecked<UDirectionalLightComponent>(Moon->GetLightComponent());
    MoonComponent->SetMobility(EComponentMobility::Movable);
    MoonComponent->SetIntensity(2.6f);
    MoonComponent->SetLightColor(FLinearColor(.55f, .66f, .95f));
    MoonComponent->SetVolumetricScatteringIntensity(2.5f);
    MoonComponent->SetDynamicShadowDistanceMovableLight(9000.f);
    // A soft, shadowless fill from behind the camera keeps faces readable against the moon.
    auto* Fill = World->SpawnActor<ADirectionalLight>(FVector(0, 0, 1500), FRotator(-40.f, MoonLight.Yaw + 180.f, 0.f));
    auto* FillComponent = CastChecked<UDirectionalLightComponent>(Fill->GetLightComponent());
    FillComponent->SetMobility(EComponentMobility::Movable);
    FillComponent->SetIntensity(.7f);
    FillComponent->SetLightColor(FLinearColor(.45f, .55f, .8f));
    FillComponent->SetCastShadows(false);
    FillComponent->SetVolumetricScatteringIntensity(0.f);
    auto* Sky = World->SpawnActor<ASkyLight>();
    Sky->GetLightComponent()->SetMobility(EComponentMobility::Movable);
    Sky->GetLightComponent()->SetIntensity(.55f);
    Sky->GetLightComponent()->SetLightColor(FLinearColor(.5f, .62f, .85f));
    Sky->GetLightComponent()->SetRealTimeCaptureEnabled(false);
    auto* Mist = World->SpawnActor<AExponentialHeightFog>();
    auto* FogComponent = Mist->GetComponent();
    FogComponent->SetFogDensity(Fog);
    FogComponent->SetFogHeightFalloff(.35f);
    FogComponent->SetFogInscatteringColor(FLinearColor(.028f, .06f, .066f));
    FogComponent->SetStartDistance(900.f);
    FogComponent->SetFogCutoffDistance(30000.f); // the sky dome paints its own horizon
    FogComponent->SetVolumetricFog(true);
    FogComponent->SetVolumetricFogScatteringDistribution(.75f);
    FogComponent->SetVolumetricFogAlbedo(FColor(170, 190, 210));
    FogComponent->SetVolumetricFogExtinctionScale(.8f);
    auto* Post = World->SpawnActor<APostProcessVolume>();
    Post->bUnbound = true;
    auto& S = Post->Settings;
    S.bOverride_AutoExposureMinBrightness = S.bOverride_AutoExposureMaxBrightness = true;
    S.AutoExposureMinBrightness = S.AutoExposureMaxBrightness = 1.f;
    S.bOverride_BloomIntensity = true; S.BloomIntensity = .8f;
    S.bOverride_VignetteIntensity = true; S.VignetteIntensity = .55f;
    S.bOverride_AmbientOcclusionIntensity = true; S.AmbientOcclusionIntensity = .75f;
    S.bOverride_AmbientOcclusionRadius = true; S.AmbientOcclusionRadius = 140.f;
    // Cool shadows, faintly warm highlights, a touch more contrast.
    S.bOverride_ColorGammaShadows = true; S.ColorGammaShadows = FVector4(.96f, 1.f, 1.06f, 1.f);
    S.bOverride_ColorGainHighlights = true; S.ColorGainHighlights = FVector4(1.04f, 1.f, .95f, 1.f);
    S.bOverride_ColorContrast = true; S.ColorContrast = FVector4(1.08f, 1.08f, 1.08f, 1.f);
    S.bOverride_ColorSaturation = true; S.ColorSaturation = FVector4(.92f, .92f, .92f, 1.f);
    S.bOverride_FilmGrainIntensity = true; S.FilmGrainIntensity = .06f;
}

void Fireflies(UWorld* World, FVector Where, float Scale)
{
    if (auto* Flies = LoadObject<UParticleSystem>(nullptr, TEXT("/Game/Realistic_Starter_VFX_Pack_Vol2/Particles/Environment/P_Fireflies.P_Fireflies")))
        UGameplayStatics::SpawnEmitterAtLocation(World, Flies, Where, FRotator::ZeroRotator, FVector(Scale), false);
}

UStaticMesh* Inferno(const TCHAR* Path)
{
    const FString Name = FPaths::GetBaseFilename(Path);
    return LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/Inferno_World_Free/Meshes/%s.%s"), Path, *Name));
}

void Fire(UWorld* World, FVector Where, float Scale, const TCHAR* System)
{
    if (auto* Flames = LoadObject<UNiagaraSystem>(nullptr, *FString::Printf(TEXT("/Game/Stylish_Fire_VFX/Niagara/%s.%s"), System, System)))
        UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, Flames, Where, FRotator::ZeroRotator, FVector(Scale), false);
}

void Embers(UWorld* World, FVector Where, float Scale)
{
    if (auto* Sparks = LoadObject<UParticleSystem>(nullptr, TEXT("/Game/Realistic_Starter_VFX_Pack_Vol2/Particles/Sparks/P_Embers_A.P_Embers_A")))
        UGameplayStatics::SpawnEmitterAtLocation(World, Sparks, Where, FRotator::ZeroRotator, FVector(Scale), false);
}

void Disc(UWorld* World, FVector Center, float Radius, float TileSize, UMaterialInterface* Material)
{
    auto* Actor = World->SpawnActor<AActor>();
    auto* Mesh = NewObject<UProceduralMeshComponent>(Actor);
    Actor->SetRootComponent(Mesh);
    Actor->AddInstanceComponent(Mesh);
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Mesh->RegisterComponent();
    Actor->SetActorLocation(Center);
    TArray<FVector> Vertices = {FVector::ZeroVector}, Normals = {FVector::UpVector};
    TArray<FVector2D> UVs = {FVector2D(Center.X, Center.Y) / TileSize};
    TArray<int32> Triangles;
    constexpr int32 Sides = 48;
    for (int32 I = 0; I < Sides; ++I)
    {
        const float A = 2.f * PI * I / Sides;
        // A slightly uneven rim reads as worn stone rather than a drawn circle.
        const float R = Radius * (1.f + .06f * FMath::Sin(A * 5.f) + .04f * FMath::Sin(A * 11.f + 1.f));
        const FVector P(FMath::Cos(A) * R, FMath::Sin(A) * R, 0.f);
        Vertices.Add(P);
        Normals.Add(FVector::UpVector);
        UVs.Add(FVector2D(Center.X + P.X, Center.Y + P.Y) / TileSize);
        Triangles.Append({0, 1 + (I + 1) % Sides, 1 + I});
    }
    Mesh->CreateMeshSection(0, Vertices, Triangles, Normals, UVs, {}, {}, false);
    Mesh->SetMaterial(0, Material);
}

APointLight* PointGlow(UWorld* World, FVector Where, FLinearColor Color, float Intensity, float Radius)
{
    auto* Light = World->SpawnActor<APointLight>(Where, FRotator::ZeroRotator);
    Light->PointLightComponent->SetMobility(EComponentMobility::Movable);
    Light->PointLightComponent->SetLightColor(Color);
    Light->PointLightComponent->SetIntensity(Intensity);
    Light->PointLightComponent->SetAttenuationRadius(Radius);
    Light->PointLightComponent->SetCastShadows(false);
    return Light;
}
}

using namespace ForestArt;

void AArenaGameMode::BuildForestRunScenery()
{
    UWorld* World = GetWorld();
    const ForestRoom::FPlan Plan = ForestRoom::Make(ForestSeed, ForestRoomNumber);
    FRandomStream Dice(ForestSeed * 17 + ForestRoomNumber * 3);
    // Things the dressing must keep clear of: cover, thorns, the trail and the centre where the fights are.
    auto Crowded = [&](FVector2D P, float Keep)
    {
        for (const auto& C : Plan.Cover) if (FVector2D::Distance(P, C.Position) < ForestRoom::CoverReach(C) + Keep) return true;
        for (const auto& T : Plan.Thorns) if (FVector2D::Distance(P, T.Position) < T.Radius + Keep) return true;
        return FMath::Abs(P.Y) < 170.f + Keep * .5f;
    };
    auto OnGround = [](FVector2D P, float Sink = 0.f) { return FVector(P.X, P.Y, GroundHeight(P) - Sink); };

    // Ground: tiled forest grass on a gently rising floor; the invisible collision floor is built in ForestRun.cpp.
    Ground(World, ForestRoom::Radius + 2600.f, 320.f, LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Pack_Bonus/Materials/M_Pack_Bonus_Grass_1.M_Pack_Bonus_Grass_1")));
    // The worn trail: flat stepping stones from the entrance to the exit.
    auto* Steps = Batch(World, Boulder(5), false, -1.f);
    for (float X = -ForestRoom::Radius - 900.f; X < ForestRoom::Radius + 900.f; X += Dice.FRandRange(150.f, 210.f))
    {
        const FVector2D P(X, Dice.FRandRange(-60.f, 60.f));
        Steps->AddInstance(FTransform(FRotator(0, Dice.FRandRange(0.f, 360.f), 0), OnGround(P, 2.f), FVector(Dice.FRandRange(.22f, .32f), Dice.FRandRange(.2f, .3f), .05f)));
    }

    // The tree wall. The ring layout is fixed (it is the room's shape); which tree stands where is too.
    FRandomStream Trees(7717);
    UStaticMesh* Pines[] = {Kit(TEXT("SM_PineA")), Kit(TEXT("SM_PineB")), Kit(TEXT("SM_PineC"))};
    UHierarchicalInstancedStaticMeshComponent* PineBatches[] = {Batch(World, Pines[0], true, 7.f), Batch(World, Pines[1], true, 8.f), Batch(World, Pines[2], true, 6.f)};
    auto* Dead = Batch(World, Kit(TEXT("SM_DeadTree")), true, 3.f);
    auto* TreeRocks = Batch(World, Boulder(2), true, -1.f, 9000.f);
    for (int32 I = 0; I < 260; ++I)
    {
        const float A = I * 2.399963f + Trees.FRandRange(-.1f, .1f);
        const float R = ForestRoom::Radius + 180.f + FMath::Pow(Trees.FRand(), .8f) * 2100.f;
        const FVector2D P(FMath::Cos(A) * R, FMath::Sin(A) * R);
        if (FMath::Abs(P.Y) < 300.f + (R - ForestRoom::Radius) * .12f) continue; // the trail's openings
        const FRotator Turn(0, Trees.FRandRange(0.f, 360.f), 0);
        const float S = Trees.FRandRange(.85f, 1.35f) * (R > ForestRoom::Radius + 1200.f ? 1.2f : 1.f);
        if (Trees.FRand() < .12f) Dead->AddInstance(FTransform(Turn, OnGround(P, 10.f), FVector(S)));
        else PineBatches[Trees.RandRange(0, 2)]->AddInstance(FTransform(Turn, OnGround(P, 10.f), FVector(S * Trees.FRandRange(.9f, 1.1f), S * Trees.FRandRange(.9f, 1.1f), S)));
        if (I % 9 == 0)
            TreeRocks->AddInstance(FTransform(Turn, OnGround(P + FVector2D(Trees.FRandRange(-250.f, 250.f), Trees.FRandRange(-250.f, 250.f)), 20.f), FVector(Trees.FRandRange(.5f, 1.1f))));
    }

    // Undergrowth: ferns and tall grass thicken toward the trees; the open centre stays low.
    auto* Ferns = Batch(World, Kit(TEXT("SM_Fern")), true, 4.f, 7000.f);
    auto* Grass = Batch(World, Kit(TEXT("SM_GrassTuft")), false, 5.f, 5000.f);
    auto* DryGrass = Batch(World, Kit(TEXT("SM_GrassTuftDry")), false, 5.f, 5000.f);
    for (int32 I = 0; I < 2600; ++I)
    {
        const float A = Dice.FRandRange(0.f, 2.f * PI);
        const float R = FMath::Sqrt(Dice.FRand()) * (ForestRoom::Radius + 1400.f);
        const FVector2D P(FMath::Cos(A) * R, FMath::Sin(A) * R);
        // Density: sparse in the middle, thick at the edge.
        const float Edge = FMath::Clamp((R - ForestRoom::CentreClear) / (ForestRoom::Radius - ForestRoom::CentreClear), 0.f, 1.3f);
        if (Dice.FRand() > .15f + .75f * Edge || Crowded(P, 40.f)) continue;
        const FTransform T(FRotator(0, Dice.FRandRange(0.f, 360.f), 0), OnGround(P, 2.f), FVector(Dice.FRandRange(.9f, 1.6f) * (.8f + .5f * FMath::Min(Edge, 1.f))));
        if (R > ForestRoom::Radius - 300.f && Dice.FRand() < .35f) Ferns->AddInstance(FTransform(T.Rotator(), T.GetLocation(), T.GetScale3D() * 1.3f));
        else (Dice.FRand() < .25f ? DryGrass : Grass)->AddInstance(T);
    }

    // Mushrooms at tree roots and against the cover, some glowing.
    auto* Shrooms = Batch(World, Kit(TEXT("SM_Mushrooms")), true, -1.f, 5000.f);
    auto* GlowShrooms = Batch(World, Kit(TEXT("SM_GlowShrooms")), false, -1.f, 7000.f);
    int32 Lights = 0;
    for (int32 I = 0; I < 46; ++I)
    {
        FVector2D P;
        if (I < Plan.Cover.Num() * 2)
        {
            const auto& C = Plan.Cover[I % Plan.Cover.Num()];
            const float A = Dice.FRandRange(0.f, 2.f * PI);
            P = C.Position + FVector2D(FMath::Cos(A), FMath::Sin(A)) * (ForestRoom::CoverReach(C) * .9f + 40.f);
        }
        else
        {
            const float A = Dice.FRandRange(0.f, 2.f * PI), R = ForestRoom::Radius + Dice.FRandRange(-250.f, 500.f);
            P = FVector2D(FMath::Cos(A) * R, FMath::Sin(A) * R);
        }
        if (FMath::Abs(P.Y) < 200.f) continue;
        const bool Glowing = Dice.FRand() < .35f;
        (Glowing ? GlowShrooms : Shrooms)->AddInstance(FTransform(FRotator(0, Dice.FRandRange(0.f, 360.f), 0), OnGround(P), FVector(Dice.FRandRange(1.f, 1.8f))));
        if (Glowing && Lights < 6) { PointGlow(World, OnGround(P) + FVector(0, 0, 40), FLinearColor(.25f, 1.f, .8f), 900.f, 380.f); ++Lights; }
    }

    // Dressing around the rolled cover: grass and ferns at the base of every piece.
    for (const auto& C : Plan.Cover)
        for (int32 K = 0; K < 7; ++K)
        {
            const float A = Dice.FRandRange(0.f, 2.f * PI);
            const FVector2D P = C.Position + FVector2D(FMath::Cos(A), FMath::Sin(A)) * (ForestRoom::CoverReach(C) * Dice.FRandRange(.75f, 1.05f));
            (K % 3 == 0 ? Ferns : Grass)->AddInstance(FTransform(FRotator(0, Dice.FRandRange(0.f, 360.f), 0), OnGround(P), FVector(Dice.FRandRange(.8f, 1.3f))));
        }

    for (auto* B : {Steps, PineBatches[0], PineBatches[1], PineBatches[2], Dead, TreeRocks, Ferns, Grass, DryGrass, Shrooms, GlowShrooms})
        B->BuildTreeIfOutdated(true, true);

    // Fireflies drifting along the tree line, and the night itself.
    for (int32 I = 0; I < 7; ++I)
    {
        const float A = 2.f * PI * I / 7.f + .3f;
        Fireflies(World, FVector(FMath::Cos(A) * (ForestRoom::Radius - 200.f), FMath::Sin(A) * (ForestRoom::Radius - 200.f), 150.f), 1.5f);
    }
    const FRotator MoonLight(-24.f, 196.f, 0.f);
    NightSky(World, -MoonLight.Vector());
    Moonlight(World, MoonLight, .02f);

    // Invisible wall along the tree line; the trail openings stay closed too (the exit is inside the clearing).
    constexpr int32 Sections = 56;
    for (int32 I = 0; I < Sections; ++I)
    {
        const float A = 2.f * PI * I / Sections, B = 2.f * PI * (I + 1) / Sections;
        const FVector From(ForestRoom::Radius * FMath::Cos(A), ForestRoom::Radius * FMath::Sin(A), 0), To(ForestRoom::Radius * FMath::Cos(B), ForestRoom::Radius * FMath::Sin(B), 0);
        const FVector Delta = To - From;
        auto* Wall = Prop((From + To) * .5f + FVector(0, 0, 2900.f), FVector((Delta.Size() + 24.f) / 100.f, .8f, 60.f), FLinearColor::Black);
        Wall->SetActorRotation(FRotator(0, FMath::RadiansToDegrees(FMath::Atan2(Delta.Y, Delta.X)), 0));
        Wall->Tags.Add(TEXT("ForestBoundary"));
        Wall->SetActorHiddenInGame(true);
    }
}

void AArenaGameMode::BuildForestHubDetails()
{
    UWorld* World = GetWorld();
    FRandomStream Random(4291);
    // Ground: forest grass, a ring of old mossy flagstones around the fire and stepping stones to the road.
    Ground(World, 3600.f, 320.f, LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Pack_Bonus/Materials/M_Pack_Bonus_Grass_1.M_Pack_Bonus_Grass_1")));
    Disc(World, FVector(0, 0, .6f), 470.f, 260.f, LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Pack_Bonus/Materials/M_Pack_Bonus_Stone_3.M_Pack_Bonus_Stone_3")));
    auto* Steps = Batch(World, Boulder(5), false, -1.f);
    for (float X = 520.f; X < 1500.f; X += Random.FRandRange(140.f, 190.f))
        Steps->AddInstance(FTransform(FRotator(0, Random.FRandRange(0.f, 360.f), 0), FVector(X, Random.FRandRange(-50.f, 50.f), -2.f),
            FVector(Random.FRandRange(.22f, .3f), Random.FRandRange(.2f, .28f), .05f)));
    // The tree wall around the clearing, open toward the road in the east.
    UHierarchicalInstancedStaticMeshComponent* Pines[] = {Batch(World, Kit(TEXT("SM_PineA")), true, 7.f), Batch(World, Kit(TEXT("SM_PineB")), true, 8.f), Batch(World, Kit(TEXT("SM_PineC")), true, 6.f)};
    auto* Dead = Batch(World, Kit(TEXT("SM_DeadTree")), true, 3.f);
    auto* Rocks = Batch(World, Boulder(3), true, -1.f, 9000.f);
    for (int32 I = 0; I < 150; ++I)
    {
        const float A = I * 2.399963f + Random.FRandRange(-.13f, .13f);
        const float R = 1200.f + FMath::Pow(Random.FRand(), .8f) * 1700.f;
        const FVector P(FMath::Cos(A) * R, FMath::Sin(A) * R, -10.f);
        if (P.X > 380.f && FMath::Abs(P.Y) < 430.f + (P.X - 1000.f) * .1f) continue;
        const FRotator Turn(0, Random.FRandRange(0.f, 360.f), 0);
        const float S = Random.FRandRange(.8f, 1.3f);
        if (Random.FRand() < .1f) Dead->AddInstance(FTransform(Turn, P, FVector(S)));
        else Pines[Random.RandRange(0, 2)]->AddInstance(FTransform(Turn, P, FVector(S)));
        if (I % 8 == 0)
            Rocks->AddInstance(FTransform(Turn, P + FVector(Random.FRandRange(-220.f, 220.f), Random.FRandRange(-220.f, 220.f), -10.f), FVector(Random.FRandRange(.5f, 1.f))));
    }
    // Ferns and grass at the edge of the clearing, mushrooms among the roots.
    auto* Ferns = Batch(World, Kit(TEXT("SM_Fern")), true, 4.f, 6000.f);
    auto* Grass = Batch(World, Kit(TEXT("SM_GrassTuft")), false, 5.f, 5000.f);
    auto* DryGrass = Batch(World, Kit(TEXT("SM_GrassTuftDry")), false, 5.f, 5000.f);
    for (int32 I = 0; I < 1300; ++I)
    {
        const float A = Random.FRandRange(0.f, 2.f * PI), R = Random.FRandRange(480.f, 2400.f);
        const FVector P(FMath::Cos(A) * R, FMath::Sin(A) * R, -2.f);
        // Keep the road, the merchant's stall and the bedroll clear.
        if ((P.X > 250.f && FMath::Abs(P.Y) < 300.f) || FVector::Dist2D(P, FVector(80, 820, 0)) < 330.f || FVector::Dist2D(P, FVector(200, -560, 0)) < 230.f) continue;
        if (Random.FRand() > FMath::Clamp((R - 450.f) / 700.f, .15f, 1.f)) continue;
        const FTransform T(FRotator(0, Random.FRandRange(0.f, 360.f), 0), P, FVector(Random.FRandRange(.9f, 1.5f)));
        if (R > 950.f && Random.FRand() < .35f) Ferns->AddInstance(FTransform(T.Rotator(), T.GetLocation(), T.GetScale3D() * 1.3f));
        else (Random.FRand() < .25f ? DryGrass : Grass)->AddInstance(T);
    }
    auto* Shrooms = Batch(World, Kit(TEXT("SM_Mushrooms")), true, -1.f, 5000.f);
    auto* GlowShrooms = Batch(World, Kit(TEXT("SM_GlowShrooms")), false, -1.f, 6000.f);
    for (int32 I = 0; I < 30; ++I)
    {
        const float A = Random.FRandRange(0.f, 2.f * PI), R = Random.FRandRange(900.f, 1400.f);
        const FVector P(FMath::Cos(A) * R, FMath::Sin(A) * R, 0.f);
        if (P.X > 380.f && FMath::Abs(P.Y) < 450.f) continue;
        const bool Glowing = I % 3 == 0;
        (Glowing ? GlowShrooms : Shrooms)->AddInstance(FTransform(FRotator(0, Random.FRandRange(0.f, 360.f), 0), P, FVector(Random.FRandRange(1.f, 1.7f))));
        if (Glowing && I < 15) PointGlow(World, P + FVector(0, 0, 40), FLinearColor(.25f, 1.f, .8f), 800.f, 360.f);
    }
    for (auto* B : {Steps, Pines[0], Pines[1], Pines[2], Dead, Rocks, Ferns, Grass, DryGrass, Shrooms, GlowShrooms}) B->BuildTreeIfOutdated(true, true);
    for (int32 I = 0; I < 5; ++I)
    {
        const float A = 2.f * PI * I / 5.f + 1.f;
        Fireflies(World, FVector(FMath::Cos(A) * 1050.f, FMath::Sin(A) * 1050.f, 160.f), 1.3f);
    }
}
