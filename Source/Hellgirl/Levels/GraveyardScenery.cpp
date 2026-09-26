// The graveyard's scenery: moonlit sky and light, ground and gravel paths, the railed wall with its gate and crypt,
// dark grass, drifting mist, ghost lanterns and the dead woods beyond the wall. The planned pieces (graves,
// mausoleums, gardens...) are placed in Graveyard.cpp. Art: Tools/Environment/build_graveyard_kit.ps1.
#include "Levels/ArenaGameMode.h"
#include "Levels/ForestArt.h"
#include "Levels/GraveyardArt.h"
#include "Rules/GraveyardRules.h"
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

namespace GraveArt
{
using namespace GraveRoom;

UStaticMesh* Kit(const TCHAR* Name)
{
    return LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/Environment/Graveyard/Kit/%s.%s"), Name, Name));
}

UStaticMesh* Mesh(EPiece Piece)
{
    static const TCHAR* Names[] = {
        TEXT("SM_HeadstoneRound"), TEXT("SM_HeadstoneRoundSmall"), TEXT("SM_HeadstoneGothic"), TEXT("SM_HeadstoneCross"), TEXT("SM_HeadstoneCeltic"),
        TEXT("SM_HeadstoneBroken"), TEXT("SM_Obelisk"), TEXT("SM_GraveMound"), TEXT("SM_GraveLedger"), TEXT("SM_OpenGrave"), TEXT("SM_Sarcophagus"),
        TEXT("SM_MausoleumGothic"), TEXT("SM_MausoleumDome"), TEXT("SM_RoseBush"), TEXT("SM_RoseBushSmall"), TEXT("SM_RosesLaid"), TEXT("SM_RoseArch"),
        TEXT("SM_Hedge"), TEXT("SM_DeadOak"), TEXT("SM_Well"), TEXT("SM_StatueMourner"), TEXT("SM_StatueAngel"), TEXT("SM_Candles"),
        TEXT("SM_GhostLantern"), TEXT("SM_Bones"), TEXT("SM_BrokenColumn")};
    static_assert(UE_ARRAY_COUNT(Names) == static_cast<int32>(EPiece::DeadTree), "one kit mesh per piece before DeadTree");
    if (Piece == EPiece::DeadTree) return ForestArt::Kit(TEXT("SM_DeadTree"));
    return Kit(Names[FMath::Clamp(static_cast<int32>(Piece), 0, UE_ARRAY_COUNT(Names) - 1)]);
}

UMaterialInterface* Material(const TCHAR* Name)
{
    return LoadObject<UMaterialInterface>(nullptr, *FString::Printf(TEXT("/Game/Environment/Graveyard/%s.%s"), Name, Name));
}

UHierarchicalInstancedStaticMeshComponent* Batch(UWorld* World, UStaticMesh* Mesh, bool Collide, float Sway, float CullDistance)
{
    // Like ForestArt::Batch, but collision is set before the component registers: instance bodies are only created
    // for a component that already collides.
    auto* Actor = World->SpawnActor<AActor>();
    auto* Instances = NewObject<UHierarchicalInstancedStaticMeshComponent>(Actor);
    Actor->SetRootComponent(Instances);
    Actor->AddInstanceComponent(Instances);
    Instances->SetStaticMesh(Mesh);
    if (Collide) Instances->SetCollisionProfileName(TEXT("BlockAll"));
    else Instances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Instances->SetGenerateOverlapEvents(false);
    Instances->SetCastShadow(true);
    Instances->bAutoRebuildTreeOnInstanceChanges = false;
    if (CullDistance > 0.f) Instances->SetCullDistances(CullDistance * .8f, CullDistance);
    if (Sway >= 0.f && Mesh)
        if (auto* Swaying = UMaterialInstanceDynamic::Create(Mesh->GetMaterial(0), Instances))
        {
            Swaying->SetScalarParameterValue(TEXT("SwayAmount"), Sway);
            Instances->SetMaterial(0, Swaying);
        }
    Instances->RegisterComponent();
    return Instances;
}

float GroundHeight(FVector2D P)
{
    const float Out = FMath::Max(0.f, FMath::Max(FMath::Abs(P.X), FMath::Abs(P.Y)) - Half - 500.f);
    return Out * .16f * (.6f + .4f * FMath::PerlinNoise2D(P * .0009f));
}

// A flat strip of a tiling material from From to To with ragged edges (world-space UVs, so it tiles evenly).
void Strip(UWorld* World, FVector2D From, FVector2D To, float HalfWidth, float TileSize, UMaterialInterface* Material, float Z, int32 Seed)
{
    auto* Actor = World->SpawnActor<AActor>();
    auto* Mesh = NewObject<UProceduralMeshComponent>(Actor);
    Actor->SetRootComponent(Mesh);
    Actor->AddInstanceComponent(Mesh);
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Mesh->RegisterComponent();
    const FVector2D Along = (To - From).GetSafeNormal(), Across(-Along.Y, Along.X);
    const int32 Steps = FMath::Max(1, FMath::CeilToInt(FVector2D::Distance(From, To) / 120.f));
    TArray<FVector> Vertices, Normals;
    TArray<FVector2D> UVs;
    TArray<int32> Triangles;
    for (int32 I = 0; I <= Steps; ++I)
    {
        const FVector2D C = FMath::Lerp(From, To, static_cast<float>(I) / Steps);
        for (float Side : {-1.f, 1.f})
        {
            const float Ragged = 22.f * FMath::PerlinNoise1D(I * .37f + Side * 11.f + Seed) + 12.f * FMath::PerlinNoise1D(I * 1.3f + Side * 5.f + Seed);
            const FVector2D P = C + Across * Side * (HalfWidth + Ragged);
            Vertices.Add(FVector(P.X, P.Y, Z));
            Normals.Add(FVector::UpVector);
            UVs.Add(P / TileSize);
        }
        if (I < Steps)
        {
            const int32 A = I * 2;
            Triangles.Append({A, A + 2, A + 1, A + 1, A + 2, A + 3});
        }
    }
    // Make the winding face up whichever way the strip runs (Unreal treats clockwise-from-above as the front).
    if (FVector::CrossProduct(Vertices[2] - Vertices[0], Vertices[1] - Vertices[0]).Z > 0.f)
        for (int32 I = 0; I < Triangles.Num(); I += 3) Swap(Triangles[I + 1], Triangles[I + 2]);
    Mesh->CreateMeshSection(0, Vertices, Triangles, Normals, UVs, {}, {}, false);
    Mesh->SetMaterial(0, Material);
}

// A flat sheet of drifting mist at height Z (M_GraveMist, see import_graveyard.py).
void MistSheet(UWorld* World, float Z, float Extent, float TileSize, float Opacity, float Speed)
{
    auto* Actor = World->SpawnActor<AActor>();
    auto* Mesh = NewObject<UProceduralMeshComponent>(Actor);
    Actor->SetRootComponent(Mesh);
    Actor->AddInstanceComponent(Mesh);
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Mesh->SetCastShadow(false);
    Mesh->RegisterComponent();
    TArray<FVector> Vertices;
    TArray<FVector2D> UVs;
    for (const FVector2D& C : {FVector2D(-1, -1), FVector2D(1, -1), FVector2D(1, 1), FVector2D(-1, 1)})
    {
        Vertices.Add(FVector(C.X * Extent, C.Y * Extent, Z));
        UVs.Add(C * Extent / TileSize + FVector2D(Z * .013f, Z * .007f)); // each sheet drifts on its own offset
    }
    Mesh->CreateMeshSection(0, Vertices, {0, 2, 1, 0, 3, 2}, {FVector::UpVector, FVector::UpVector, FVector::UpVector, FVector::UpVector}, UVs, {}, {}, false);
    if (auto* Mist = UMaterialInstanceDynamic::Create(Material(TEXT("M_GraveMist")), Actor))
    {
        Mist->SetScalarParameterValue(TEXT("Opacity"), Opacity);
        Mist->SetScalarParameterValue(TEXT("Speed0"), Speed);
        Mist->SetScalarParameterValue(TEXT("Speed1"), Speed * 1.3f);
        Mesh->SetMaterial(0, Mist);
    }
}

// Full-moon night: a bright, cold moon low in the east (behind the crypt, ahead of the entering player) casting long
// shadows back toward the gate, a faint fill, low mist-laden fog and cool grading.
void Night(UWorld* World, float MoonYaw, float Mist)
{
    const FRotator MoonLight(-17.f, 180.f + MoonYaw, 0.f);
    auto* Dome = ForestArt::Solid(World, LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")), FTransform(FRotator::ZeroRotator, FVector::ZeroVector, FVector(800.f)), false);
    if (Dome)
    {
        Dome->GetStaticMeshComponent()->SetCastShadow(false);
        if (auto* Sky = UMaterialInstanceDynamic::Create(Material(TEXT("M_GraveSky")) ? Material(TEXT("M_GraveSky"))
                : LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Environment/Materials/M_ForestSky.M_ForestSky")), Dome))
        {
            Sky->SetVectorParameterValue(TEXT("MoonDir"), FLinearColor(-MoonLight.Vector()));
            Sky->SetVectorParameterValue(TEXT("Zenith"), FLinearColor(.003f, .005f, .014f));
            Sky->SetVectorParameterValue(TEXT("Horizon"), FLinearColor(.025f, .036f, .06f));
            Sky->SetVectorParameterValue(TEXT("MoonColor"), FLinearColor(.86f, .92f, 1.f));
            Sky->SetScalarParameterValue(TEXT("MoonBrightness"), 9.f);
            Dome->GetStaticMeshComponent()->SetMaterial(0, Sky);
        }
    }
    auto* Moon = World->SpawnActor<ADirectionalLight>(FVector(0, 0, 2000), MoonLight);
    auto* MoonComponent = CastChecked<UDirectionalLightComponent>(Moon->GetLightComponent());
    MoonComponent->SetMobility(EComponentMobility::Movable);
    MoonComponent->SetIntensity(3.2f);
    MoonComponent->SetLightColor(FLinearColor(.62f, .72f, 1.f));
    MoonComponent->SetVolumetricScatteringIntensity(2.f);
    MoonComponent->SetDynamicShadowDistanceMovableLight(9000.f);
    auto* Fill = World->SpawnActor<ADirectionalLight>(FVector(0, 0, 1500), FRotator(-45.f, MoonLight.Yaw + 180.f, 0.f));
    auto* FillComponent = CastChecked<UDirectionalLightComponent>(Fill->GetLightComponent());
    FillComponent->SetMobility(EComponentMobility::Movable);
    FillComponent->SetIntensity(.55f);
    FillComponent->SetLightColor(FLinearColor(.45f, .55f, .85f));
    FillComponent->SetCastShadows(false);
    FillComponent->SetVolumetricScatteringIntensity(0.f);
    auto* Sky = World->SpawnActor<ASkyLight>();
    Sky->GetLightComponent()->SetMobility(EComponentMobility::Movable);
    Sky->GetLightComponent()->SetIntensity(.6f);
    Sky->GetLightComponent()->SetLightColor(FLinearColor(.45f, .55f, .85f));
    Sky->GetLightComponent()->SetRealTimeCaptureEnabled(false);
    auto* Fog = World->SpawnActor<AExponentialHeightFog>();
    auto* FogComponent = Fog->GetComponent();
    FogComponent->SetFogDensity(.018f + .02f * Mist);
    FogComponent->SetFogHeightFalloff(.5f);
    FogComponent->SetFogInscatteringColor(FLinearColor(.03f, .042f, .068f));
    FogComponent->SetStartDistance(700.f);
    FogComponent->SetFogCutoffDistance(30000.f);
    FogComponent->SetVolumetricFog(true);
    FogComponent->SetVolumetricFogScatteringDistribution(.7f);
    FogComponent->SetVolumetricFogAlbedo(FColor(185, 200, 225));
    FogComponent->SetVolumetricFogExtinctionScale(.7f);
    auto* Post = World->SpawnActor<APostProcessVolume>();
    Post->bUnbound = true;
    auto& S = Post->Settings;
    S.bOverride_AutoExposureMinBrightness = S.bOverride_AutoExposureMaxBrightness = true;
    S.AutoExposureMinBrightness = S.AutoExposureMaxBrightness = 1.f;
    S.bOverride_BloomIntensity = true; S.BloomIntensity = .9f;
    S.bOverride_VignetteIntensity = true; S.VignetteIntensity = .6f;
    S.bOverride_AmbientOcclusionIntensity = true; S.AmbientOcclusionIntensity = .8f;
    S.bOverride_AmbientOcclusionRadius = true; S.AmbientOcclusionRadius = 120.f;
    // Silver-blue shadows and mids; the roses' red is kept (saturation is only slightly lowered).
    S.bOverride_ColorGammaShadows = true; S.ColorGammaShadows = FVector4(.94f, .99f, 1.08f, 1.f);
    S.bOverride_ColorGainMidtones = true; S.ColorGainMidtones = FVector4(.96f, 1.f, 1.05f, 1.f);
    S.bOverride_ColorContrast = true; S.ColorContrast = FVector4(1.1f, 1.1f, 1.1f, 1.f);
    S.bOverride_ColorSaturation = true; S.ColorSaturation = FVector4(.88f, .88f, .88f, 1.f);
    S.bOverride_FilmGrainIntensity = true; S.FilmGrainIntensity = .07f;
}
}

void AArenaGameMode::BuildGraveyardScenery()
{
    using namespace GraveRoom;
    UWorld* World = GetWorld();
    const FPlan Plan = Make(GraveSeed, GraveRoomNumber);
    FRandomStream Dice(GraveSeed * 13 + GraveRoomNumber * 7);
    auto OnGround = [](FVector2D P, float Sink = 0.f) { return FVector(P.X, P.Y, GraveArt::GroundHeight(P) - Sink); };

    // Ground and gravel: the two crossing paths, the round plaza where they meet and the track out of the gate.
    ForestArt::Ground(World, Half + 4200.f, 420.f, GraveArt::Material(TEXT("MI_GraveGrass")), [](FVector2D P) { return GraveArt::GroundHeight(P); });
    UMaterialInterface* Gravel = GraveArt::Material(TEXT("MI_GravePath"));
    GraveArt::Strip(World, FVector2D(-Half - 1800.f, 0.f), FVector2D(Half, 0.f), PathHalf, 380.f, Gravel, 1.2f, 3);
    GraveArt::Strip(World, FVector2D(0.f, -Half), FVector2D(0.f, Half), PathHalf, 380.f, Gravel, 1.4f, 9);
    ForestArt::Disc(World, FVector(0, 0, 1.8f), PlazaRadius, 380.f, Gravel);
    for (const FItem& Item : Plan.Items)
        if (Item.Piece == EPiece::OpenGrave) ForestArt::Disc(World, FVector(Item.P.X, Item.P.Y, .8f), 190.f, 300.f, GraveArt::Material(TEXT("MI_GraveDirt")));

    // The wall: railed stone sections between pillars, broken by the gate (west) and the crypt (east).
    auto* Walls = GraveArt::Batch(World, GraveArt::Kit(TEXT("SM_GraveWall")), true, -1.f);
    auto* Pillars = GraveArt::Batch(World, GraveArt::Kit(TEXT("SM_GraveWallPillar")), true, -1.f);
    auto WallRun = [&](FVector2D From, FVector2D To)
    {
        const float Length = FVector2D::Distance(From, To);
        if (Length < 40.f) return;
        const int32 Count = FMath::Max(1, FMath::RoundToInt(Length / 400.f));
        const FVector2D Along = (To - From) / Length;
        const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Along.Y, Along.X));
        for (int32 I = 0; I < Count; ++I)
        {
            const FVector2D C = From + Along * (Length * (I + .5f) / Count);
            Walls->AddInstance(FTransform(FRotator(0, Yaw, 0), FVector(C.X, C.Y, -2.f), FVector(Length / Count / 400.f, 1.f, 1.f)));
        }
        for (int32 I = 0; I <= Count; I += 2)
        {
            const FVector2D C = From + Along * (Length * I / Count);
            Pillars->AddInstance(FTransform(FRotator(0, Yaw, 0), FVector(C.X, C.Y, -2.f)));
        }
        if (Count % 2) Pillars->AddInstance(FTransform(FRotator(0, Yaw, 0), FVector(To.X, To.Y, -2.f)));
    };
    const float W = Half + 30.f;
    WallRun(FVector2D(-W, W), FVector2D(W, W));
    WallRun(FVector2D(-W, -W), FVector2D(W, -W));
    WallRun(FVector2D(-W, -W), FVector2D(-W, -245.f));
    WallRun(FVector2D(-W, 245.f), FVector2D(-W, W));
    WallRun(FVector2D(W, -W), FVector2D(W, -250.f));
    WallRun(FVector2D(W, 250.f), FVector2D(W, W));
    Walls->BuildTreeIfOutdated(true, true);
    Pillars->BuildTreeIfOutdated(true, true);
    // The gate stands open outward; the crypt's front faces into the yard.
    ForestArt::Solid(World, GraveArt::Kit(TEXT("SM_GraveGate")), FTransform(FRotator(0, FaceYaw(FVector2D(-1, 0)), 0), FVector(-W, 0, -2.f)), false);
    ForestArt::Solid(World, GraveArt::Kit(TEXT("SM_CryptExit")), FTransform(FRotator(0, FaceYaw(FVector2D(-1, 0)), 0), FVector(W + 60.f, 0, -2.f)), false);
    for (float Side : {-1.f, 1.f})
    {
        // Blockers for the gate's pillars and the crypt's side walls (the pieces themselves have no collision).
        auto* GatePost = Prop(FVector(-W, Side * 200.f, 160.f), FVector(.95f, .95f, 3.3f), FLinearColor::Black);
        GatePost->SetActorHiddenInGame(true);
        auto* CryptSide = Prop(FVector(W + 60.f, Side * 175.f, 220.f), FVector(1.7f, 1.5f, 4.4f), FLinearColor::Black);
        CryptSide->SetActorHiddenInGame(true);
    }
    // Invisible boundary so nothing leaves the yard (the gate and the crypt included).
    for (int32 Side = 0; Side < 4; ++Side)
    {
        const bool AlongX = Side < 2;
        const float S = Side % 2 ? 1.f : -1.f;
        auto* Edge = Prop(AlongX ? FVector(0.f, S * (Half + 60.f), 2900.f) : FVector(S * (Half + 60.f), 0.f, 2900.f),
            AlongX ? FVector(Half * 2.2f / 100.f, .8f, 60.f) : FVector(.8f, Half * 2.2f / 100.f, 60.f), FLinearColor::Black);
        Edge->SetActorHiddenInGame(true);
        Edge->Tags.Add(TEXT("GraveBoundary"));
    }

    // Ghost lanterns along both paths (and those the plan put in the quarters), each with a pale blue glow.
    auto* Lanterns = GraveArt::Batch(World, GraveArt::Kit(TEXT("SM_GhostLantern")), false, -1.f);
    TArray<FVector> Glows;
    int32 Alternate = 0;
    for (float T = -Half + 900.f; T < Half - 500.f; T += 1250.f)
    {
        if (FMath::Abs(T) < PlazaRadius + 200.f) continue;
        const float Side = Alternate++ % 2 ? 1.f : -1.f;
        const FVector2D OnEW(T, Side * (PathHalf + 70.f)), OnNS(Side * (PathHalf + 70.f), T);
        Lanterns->AddInstance(FTransform(FRotator(0, FaceYaw(FVector2D(0, -Side)), 0), OnGround(OnEW)));
        Lanterns->AddInstance(FTransform(FRotator(0, FaceYaw(FVector2D(-Side, 0)), 0), OnGround(OnNS)));
        Glows.Add(OnGround(OnEW) + FVector(0, 0, 215));
        Glows.Add(OnGround(OnNS) + FVector(0, 0, 215));
    }
    for (const FItem& Item : Plan.Items)
        if (Item.Piece == EPiece::GhostLantern && Glows.Num() < 22) Glows.Add(FVector(Item.P.X, Item.P.Y, 215.f));
    for (const FVector& At : Glows) ForestArt::PointGlow(World, At, FLinearColor(.5f, .78f, 1.f), 2600.f, 750.f);
    Lanterns->BuildTreeIfOutdated(true, true);

    // Dark grass everywhere but the gravel, thicker along the wall and at the foot of every stone.
    auto* Grass = GraveArt::Batch(World, GraveArt::Kit(TEXT("SM_GraveGrass")), false, 3.f, 6500.f);
    auto* Tall = GraveArt::Batch(World, GraveArt::Kit(TEXT("SM_GraveGrassTall")), false, 4.f, 6500.f);
    auto OnGravel = [](FVector2D P) { return FMath::Abs(P.X) < PathHalf + 30.f || FMath::Abs(P.Y) < PathHalf + 30.f || P.Size() < PlazaRadius + 30.f; };
    auto Blocked = [&](FVector2D P)
    {
        for (const FItem& Item : Plan.Items)
            if (Item.IsSolid() && DistanceTo(Item, P) < 5.f) return true;
        return false;
    };
    for (int32 I = 0; I < 9000; ++I)
    {
        const FVector2D P(Dice.FRandRange(-Half - 2500.f, Half + 2500.f), Dice.FRandRange(-Half - 2500.f, Half + 2500.f));
        const float Edge = FMath::Max(FMath::Abs(P.X), FMath::Abs(P.Y));
        if (Edge < Half && (OnGravel(P) || Dice.FRand() > .45f || Blocked(P))) continue;
        const bool IsTall = Edge > Half - 500.f ? Dice.FRand() < .6f : Dice.FRand() < .15f;
        (IsTall ? Tall : Grass)->AddInstance(FTransform(FRotator(0, Dice.FRandRange(0.f, 360.f), 0), OnGround(P, 2.f), FVector(Dice.FRandRange(.9f, 1.5f))));
    }
    for (const FItem& Item : Plan.Items)
    {
        if (!Item.IsSolid()) continue;
        const int32 Tufts = Item.Half().Size() > 150.f ? 10 : 3;
        for (int32 K = 0; K < Tufts; ++K)
        {
            const float A = Dice.FRandRange(0.f, 2.f * PI);
            const FVector2D P = Item.P + FVector2D(FMath::Cos(A), FMath::Sin(A)) * (Item.Half().Size() * Dice.FRandRange(.7f, 1.05f));
            if (!OnGravel(P)) (Dice.FRand() < .4f ? Tall : Grass)->AddInstance(FTransform(FRotator(0, Dice.FRandRange(0.f, 360.f), 0), OnGround(P, 2.f), FVector(Dice.FRandRange(.8f, 1.3f))));
        }
    }
    Grass->BuildTreeIfOutdated(true, true);
    Tall->BuildTreeIfOutdated(true, true);

    // Beyond the wall: dead woods and dark pines on rising ground, rocks among them; open behind the gate.
    UHierarchicalInstancedStaticMeshComponent* Pines[] = {ForestArt::Batch(World, ForestArt::Kit(TEXT("SM_PineA")), true, 6.f),
        ForestArt::Batch(World, ForestArt::Kit(TEXT("SM_PineB")), true, 7.f), ForestArt::Batch(World, ForestArt::Kit(TEXT("SM_PineC")), true, 5.f)};
    auto* DeadTrees = ForestArt::Batch(World, ForestArt::Kit(TEXT("SM_DeadTree")), true, 3.f);
    auto* Oaks = ForestArt::Batch(World, GraveArt::Kit(TEXT("SM_DeadOak")), true, 2.f);
    FRandomStream Woods(5151);
    for (int32 I = 0; I < 420; ++I)
    {
        const FVector2D P(Woods.FRandRange(-Half - 4000.f, Half + 4000.f), Woods.FRandRange(-Half - 4000.f, Half + 4000.f));
        const float Edge = FMath::Max(FMath::Abs(P.X), FMath::Abs(P.Y));
        if (Edge < Half + 450.f || (P.X < -Half && FMath::Abs(P.Y) < 450.f + (-Half - P.X) * .15f)) continue;
        const FRotator Turn(0, Woods.FRandRange(0.f, 360.f), 0);
        const float S = Woods.FRandRange(.9f, 1.4f) * (Edge > Half + 2000.f ? 1.2f : 1.f);
        const float Roll = Woods.FRand();
        if (Roll < .38f) DeadTrees->AddInstance(FTransform(Turn, OnGround(P, 10.f), FVector(S * 1.2f)));
        else if (Roll < .46f) Oaks->AddInstance(FTransform(Turn, OnGround(P, 20.f), FVector(S * .8f)));
        else Pines[Woods.RandRange(0, 2)]->AddInstance(FTransform(Turn, OnGround(P, 10.f), FVector(S)));
    }
    for (auto* B : {Pines[0], Pines[1], Pines[2], DeadTrees, Oaks}) B->BuildTreeIfOutdated(true, true);

    // Mist: three drifting sheets hugging the ground; how thick is rolled per room.
    GraveArt::MistSheet(World, 18.f, Half + 3000.f, 1700.f, .35f + .45f * Plan.Mist, 1.f);
    GraveArt::MistSheet(World, 60.f, Half + 3000.f, 1300.f, .25f + .4f * Plan.Mist, 1.4f);
    GraveArt::MistSheet(World, 125.f, Half + 3000.f, 2100.f, .1f + .25f * Plan.Mist, .8f);
    GraveArt::Night(World, Plan.MoonYaw, Plan.Mist);
}
