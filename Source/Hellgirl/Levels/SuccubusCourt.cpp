#include "Levels/ArenaGameMode.h"
#include "Levels/MapPieces.h"
#include "Fighter/ArenaFighter.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/PointLight.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/SkyLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

// World III, phase 1 arena: a round gothic court open to a red sky. The Succubus Queen will float over
// the central pool; waves will enter through the four arches (sealed for now); pillars block Siphon beams.
// Map only: no encounters yet. +X points from the entrance (south arch) toward the far (north) arch.
using namespace MapPieces;

namespace
{
namespace Court
{
constexpr float Radius = 3500.f;        // Floor edge and inner face of the outer wall.
constexpr float WallHeight = 750.f;
constexpr float PoolRadius = 1000.f;
constexpr float PillarRadius = 2000.f;
constexpr float ArchHalfAngle = 3.75f;  // Degrees: each arch replaces one 7.5 degree wall segment.
const FVector PlayerStart(-2600.f, 0.f, 115.f);
const FLinearColor Stone(.16f, .12f, .14f), DarkStone(.09f, .07f, .08f), Rose(1.f, .16f, .45f), Violet(.55f, .1f, .95f);
}

AStaticMeshActor* Shape(UWorld* World, const TCHAR* Mesh, FVector Position, FVector Scale, FLinearColor Color,
    bool Collision = true, bool Glow = false, FRotator Rotation = FRotator::ZeroRotator, float Emissive = 2.f)
{
    auto* Actor = World->SpawnActor<AStaticMeshActor>(Position, Rotation);
    if (!Actor) return nullptr;
    Actor->SetMobility(EComponentMobility::Movable);
    auto* Component = Actor->GetStaticMeshComponent();
    Component->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, Mesh));
    Component->SetCollisionProfileName(Collision ? TEXT("BlockAll") : TEXT("NoCollision"));
    Actor->SetActorScale3D(Scale);
    if (auto* Material = Surface(Actor, Color, false, Glow))
    {
        if (Glow) Material->SetScalarParameterValue(TEXT("EmissiveStrength"), Emissive);
        Component->SetMaterial(0, Material);
    }
    return Actor;
}

const TCHAR* const Cube = TEXT("/Engine/BasicShapes/Cube.Cube");
const TCHAR* const Cylinder = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");
const TCHAR* const Cone = TEXT("/Engine/BasicShapes/Cone.Cone");
const TCHAR* const Sphere = TEXT("/Engine/BasicShapes/Sphere.Sphere");

FVector Polar(float Degrees, float Distance, float Z = 0.f)
{
    const float A = FMath::DegreesToRadians(Degrees);
    return FVector(FMath::Cos(A) * Distance, FMath::Sin(A) * Distance, Z);
}

APointLight* Glow(UWorld* World, FVector Position, FLinearColor Color, float Intensity, float Radius)
{
    auto* Actor = World->SpawnActor<APointLight>(Position, FRotator::ZeroRotator);
    if (!Actor) return nullptr;
    auto* Light = CastChecked<UPointLightComponent>(Actor->GetLightComponent());
    Light->SetMobility(EComponentMobility::Movable);
    Light->SetLightColor(Color);
    Light->SetIntensity(Intensity);
    Light->SetAttenuationRadius(Radius);
    Light->SetCastShadows(false);
    return Actor;
}
}

void AArenaGameMode::TravelToSuccubusCourt()
{
    UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this)), true, TEXT("SuccubusCourt=1"));
}

void AArenaGameMode::BuildSuccubusCourt()
{
    using namespace Court;
    UWorld* World = GetWorld();
    MapTitle = TEXT("WORLD III / THE SUCCUBUS COURT");
    MapPlatforms.Add(FVector4(0.f, 0.f, Radius * 2.f, Radius * 2.f));
    ExitPosition = FVector(-Radius, 0.f, 0.f);

    // Mountain-top plateau under the court, falling away beyond the outer wall.
    Rock(World, FVector(0, 0, -8), FVector(Radius * 2.6f, Radius * 2.6f, 2600.f), DarkStone, 3301, false);
    // Walkable floor.
    if (auto* Floor = Shape(World, Cylinder, FVector(0, 0, -50), FVector(Radius / 50.f, Radius / 50.f, 1.f), Stone))
        Floor->Tags.Add(TEXT("CourtFloor"));

    // Inlaid glowing rings and four radial lines that lead the eye from the pool to the arches.
    auto* Inlay = DecorationBatch(World, Cube, Rose * .35f, true);
    Inlay->bAutoRebuildTreeOnInstanceChanges = false;
    for (const float Ring : {PoolRadius + 280.f, PillarRadius + 650.f})
        for (int32 I = 0; I < 96; ++I)
        {
            const float A = 360.f * I / 96.f;
            Inlay->AddInstance(FTransform(FRotator(0, A + 90.f, 0), Polar(A, Ring, 1.f), FVector(Ring * 2.f * PI / 96.f / 100.f * .8f, .08f, .02f)));
        }
    for (int32 Arch = 0; Arch < 4; ++Arch)
        for (float R = PoolRadius + 400.f; R < Radius - 250.f; R += 120.f)
            Inlay->AddInstance(FTransform(FRotator(0, Arch * 90.f, 0), Polar(Arch * 90.f, R, 1.f), FVector(.9f, .06f, .02f)));
    Inlay->BuildTreeIfOutdated(true, true);

    // Central pool: a low stone rim around a glowing surface. The Queen will float above it.
    for (int32 I = 0; I < 32; ++I)
    {
        const float A = 360.f * I / 32.f;
        Shape(World, Cube, Polar(A, PoolRadius, 28.f), FVector(PoolRadius * 2.f * PI / 32.f / 100.f + .1f, .7f, .56f), Stone * 1.4f, true, false, FRotator(0, A + 90.f, 0));
    }
    if (auto* Pool = Shape(World, Cylinder, FVector(0, 0, 14), FVector((PoolRadius - 40.f) / 50.f, (PoolRadius - 40.f) / 50.f, .08f), FLinearColor(.5f, .03f, .32f), false, true, FRotator::ZeroRotator, 1.1f))
        Pool->Tags.Add(TEXT("CourtPool"));
    // Floating throne slab and halo over the pool (out of reach).
    Rock(World, FVector(0, 0, 640), FVector(420, 420, 160), DarkStone * 1.3f, 3307, false);
    Shape(World, Cube, FVector(80, 0, 760), FVector(.5f, 1.9f, 2.3f), Stone, false);
    auto* Halo = DecorationBatch(World, Sphere, Violet, true);
    for (int32 I = 0; I < 28; ++I)
        Halo->AddInstance(FTransform(FRotator::ZeroRotator, Polar(360.f * I / 28.f, 300.f, 1050.f), FVector(.22f)));
    Glow(World, FVector(0, 0, 220), FLinearColor(1.f, .2f, .6f), 26000.f, 2400.f);
    Glow(World, FVector(0, 0, 1000), Violet, 18000.f, 1800.f);

    // Eight pillars between the arches; three are broken. They are the cover against Siphon beams.
    int32 Pillars = 0;
    for (int32 I = 0; I < 8; ++I)
    {
        const float A = 22.5f + 45.f * I;
        const FVector Base = Polar(A, PillarRadius);
        const bool Broken = I == 1 || I == 4 || I == 6;
        const float Height = Broken ? 380.f + 70.f * I : 900.f;
        Shape(World, Cube, Base + FVector(0, 0, 30), FVector(1.7f, 1.7f, .6f), Stone * 1.3f, true, false, FRotator(0, A, 0));
        if (auto* Shaft = Shape(World, Cylinder, Base + FVector(0, 0, 60 + Height * .5f), FVector(1.15f, 1.15f, Height / 100.f), Stone * 1.15f))
        { Shaft->Tags.Add(TEXT("CourtPillar")); ++Pillars; }
        if (!Broken)
        {
            Shape(World, Cube, Base + FVector(0, 0, 60 + Height + 25), FVector(1.6f, 1.6f, .5f), Stone * 1.3f, true, false, FRotator(0, A, 0));
            Shape(World, Cone, Base + FVector(0, 0, 60 + Height + 110), FVector(.9f, .9f, 1.2f), DarkStone, false);
        }
        else
            for (int32 Chunk = 0; Chunk < 3; ++Chunk)
                Rock(World, Base + Polar(A + 60.f * Chunk, 150.f + 40.f * Chunk, 30.f), FVector(110, 90, 70), Stone, 3400 + I * 7 + Chunk, false, false);
        // A brazier on the inner side of each pillar.
        const FVector Brazier = Polar(A, PillarRadius - 230.f);
        Shape(World, Cylinder, Brazier + FVector(0, 0, 55), FVector(.18f, .18f, 1.1f), DarkStone);
        Shape(World, Cylinder, Brazier + FVector(0, 0, 118), FVector(.75f, .75f, .22f), DarkStone * 1.5f, false);
        Shape(World, Cone, Brazier + FVector(0, 0, 165), FVector(.42f, .42f, .75f), Rose * .8f, false, true, FRotator::ZeroRotator, 2.2f);
        Glow(World, Brazier + FVector(0, 0, 260), FLinearColor(1.f, .25f, .55f), 4500.f, 1100.f);
    }
    UE_LOG(LogTemp, Display, TEXT("SUCCUBUS COURT BUILD: %d pillars"), Pillars);

    // Outer wall: 48 segments with one left out at each arch; buttresses and spires every fourth segment.
    for (int32 I = 0; I < 48; ++I)
    {
        const float A = 7.5f * I;
        const bool Arch = I % 12 == 0;
        if (Arch) continue;
        const FVector Mid = Polar(A, Radius + 45.f, WallHeight * .5f);
        Shape(World, Cube, Mid, FVector(Radius * 2.f * PI / 48.f / 100.f + .08f, .9f, WallHeight / 100.f), Stone, true, false, FRotator(0, A + 90.f, 0))
            ->Tags.Add(TEXT("CourtWall"));
        // Crenellations.
        if (I % 2 == 0) Shape(World, Cube, Polar(A, Radius + 45.f, WallHeight + 45.f), FVector(1.4f, 1.f, .9f), Stone, false, false, FRotator(0, A + 90.f, 0));
        if (I % 4 == 2)
        {
            Shape(World, Cube, Polar(A, Radius + 190.f, WallHeight * .5f + 60.f), FVector(1.1f, 2.2f, (WallHeight + 120.f) / 100.f), DarkStone, false, false, FRotator(0, A, 0));
            Shape(World, Cone, Polar(A, Radius + 190.f, WallHeight + 300.f), FVector(1.1f, 1.1f, 3.4f), DarkStone, false);
        }
        // Crimson banners between the arches, on the inner face.
        if (I % 12 == 6)
            Shape(World, Cube, Polar(A, Radius - 12.f, WallHeight - 260.f), FVector(1.5f, .05f, 4.2f), FLinearColor(.36f, .02f, .06f), false, false, FRotator(0, A + 90.f, 0));
    }

    // Four arches. Each opens onto a short passage sealed by a violet ward (enemy entrances later).
    for (int32 Arch = 0; Arch < 4; ++Arch)
    {
        const float A = 90.f * Arch;
        const float Half = Radius * FMath::Tan(FMath::DegreesToRadians(ArchHalfAngle)) + 40.f;
        const FVector Right = Polar(A + 90.f, 1.f);
        for (const float Side : {-1.f, 1.f})
        {
            Shape(World, Cube, Polar(A, Radius + 45.f, 500.f) + Right * Side * (Half + 30.f), FVector(1.4f, 1.4f, 10.f), Stone * 1.25f, true, false, FRotator(0, A, 0));
            // Pointed gothic top: two slanted beams meeting above the opening.
            Shape(World, Cube, Polar(A, Radius + 45.f, 960.f) + Right * Side * (Half * .5f), FVector(1.f, Half * 1.18f / 100.f, .7f), Stone * 1.25f, false, false,
                FRotator(0, A, Side * 24.f));
            // Passage walls.
            Shape(World, Cube, Polar(A, Radius + 600.f, 350.f) + Right * Side * (Half + 70.f), FVector(11.f, .8f, 7.f), DarkStone, true, false, FRotator(0, A, 0));
        }
        Shape(World, Cube, Polar(A, Radius + 600.f, -25.f), FVector(11.f, Half * 2.f / 100.f + .6f, .5f), Stone, true, false, FRotator(0, A, 0));
        if (auto* Ward = Shape(World, Cube, Polar(A, Radius + 1100.f, 380.f), FVector(.2f, Half * 2.f / 100.f + .6f, 7.6f), Violet * .6f, true, true, FRotator(0, A, 0), .7f))
            Ward->Tags.Add(TEXT("CourtWard"));
        Glow(World, Polar(A, Radius + 900.f, 300.f), Violet, 9000.f, 1500.f);
    }

    // Beyond the walls: distant gothic spires, floating rocks and a huge red moon.
    FRandomStream Random(3339);
    for (int32 I = 0; I < 22; ++I)
    {
        const float A = Random.FRandRange(0.f, 360.f), R = Random.FRandRange(7500.f, 12500.f), H = Random.FRandRange(2500.f, 6500.f);
        const FVector P = Polar(A, R, -600.f);
        Shape(World, Cylinder, P + FVector(0, 0, H * .5f), FVector(4.f, 4.f, H / 100.f), DarkStone * .8f, false);
        Shape(World, Cone, P + FVector(0, 0, H + 600.f), FVector(4.6f, 4.6f, 12.f), DarkStone * .7f, false);
    }
    for (int32 I = 0; I < 14; ++I)
        Rock(World, Polar(Random.FRandRange(0.f, 360.f), Random.FRandRange(5200.f, 9500.f), Random.FRandRange(1500.f, 4200.f)),
            FVector(Random.FRandRange(400.f, 1100.f), Random.FRandRange(400.f, 1000.f), Random.FRandRange(300.f, 700.f)), DarkStone, 3500 + I, false, false);
    Shape(World, Sphere, FVector(24000.f, 9000.f, 9000.f), FVector(55.f), FLinearColor(1.f, .12f, .08f), false, true, FRotator::ZeroRotator, 4.f);

    // Red sky, low crimson sun, rose fog.
    if (auto* Sky = World->SpawnActor<AActor>())
    {
        auto* Atmosphere = NewObject<USkyAtmosphereComponent>(Sky);
        Sky->SetRootComponent(Atmosphere);
        Atmosphere->RegisterComponent();
        Atmosphere->SetRayleighScattering(FLinearColor(.022f, .003f, .007f));
        Atmosphere->SetMieScattering(FLinearColor(.035f, .006f, .01f));
    }
    if (auto* Sun = World->SpawnActor<ADirectionalLight>(FVector(0, 0, 4000), FRotator(-22.f, -150.f, 0.f)))
    {
        auto* Light = CastChecked<UDirectionalLightComponent>(Sun->GetLightComponent());
        Light->SetMobility(EComponentMobility::Movable);
        Light->bAtmosphereSunLight = true;
        Light->SetIntensity(2.3f);
        Light->SetLightColor(FLinearColor(1.f, .32f, .22f));
    }
    if (auto* SkyLight = World->SpawnActor<ASkyLight>())
    {
        SkyLight->GetLightComponent()->SetMobility(EComponentMobility::Movable);
        SkyLight->GetLightComponent()->SetRealTimeCaptureEnabled(true);
        SkyLight->GetLightComponent()->SetIntensity(.6f);
    }
    if (auto* Fog = World->SpawnActor<AExponentialHeightFog>(FVector(0, 0, -200), FRotator::ZeroRotator))
    {
        auto* Component = Fog->GetComponent();
        Component->SetFogDensity(.012f);
        Component->SetFogHeightFalloff(.18f);
        Component->SetStartDistance(3000.f);
        Component->SetFogMaxOpacity(.8f);
        Component->SetFogInscatteringColor(FLinearColor(.12f, .018f, .04f));
    }
    if (auto* Post = World->SpawnActor<APostProcessVolume>())
    {
        Post->bUnbound = true;
        auto& Settings = Post->Settings;
        Settings.bOverride_AutoExposureMinBrightness = Settings.bOverride_AutoExposureMaxBrightness = true;
        Settings.AutoExposureMinBrightness = Settings.AutoExposureMaxBrightness = 1.f;
        Settings.bOverride_BloomIntensity = true; Settings.BloomIntensity = .6f;
        Settings.bOverride_AmbientOcclusionIntensity = true; Settings.AmbientOcclusionIntensity = .9f;
        Settings.bOverride_VignetteIntensity = true; Settings.VignetteIntensity = .3f;
    }
    UE_LOG(LogTemp, Display, TEXT("SUCCUBUS COURT BUILD: complete"));
}

void AArenaGameMode::TickSuccubusCourt(float Dt)
{
    RunMapVisualCheck(Dt);
    RunSuccubusCourtCheck();
    auto* Hero = Cast<AArenaFighter>(UGameplayStatics::GetPlayerPawn(this, 0));
    if (!Hero || !Hero->IsAlive()) return;
    Objective = TEXT("THE SUCCUBUS COURT / Map preview: no enemies yet");
    Prompt.Empty();
    PromptAction = 0;
    if (Hero->GetCharacterMovement()->IsMovingOnGround()) LastSafePosition = Hero->GetActorLocation() + FVector(0, 0, 10);
    // Safety net only: the walls and wards keep the player inside.
    if (Hero->GetActorLocation().Z < -600.f)
    {
        Hero->SetActorLocation(Court::PlayerStart, false, nullptr, ETeleportType::TeleportPhysics);
        Hero->ResetAfterRecovery();
    }
}

void AArenaGameMode::RunSuccubusCourtCheck()
{
#if WITH_DEV_AUTOMATION_TESTS
    if (!FParse::Param(FCommandLine::Get(), TEXT("HellgirlCourtCheck"))) return;
    static bool Done = false;
    if (Done || GetWorld()->GetTimeSeconds() < 1.f) return;
    Done = true;
    using namespace Court;
    bool Passed = bSuccubusCourt && !bForestHub && !bStoryEnabled && SpawnSites.IsEmpty();
    FString Failure;
    auto Fail = [&](const TCHAR* Why) { if (Passed) Failure = Why; Passed = false; };
    FCollisionQueryParams Query(SCENE_QUERY_STAT(CourtCheck), false);
    if (auto* Hero = UGameplayStatics::GetPlayerPawn(this, 0)) Query.AddIgnoredActor(Hero);
    // The floor is walkable everywhere a player can reach inside the wall (outside the pool rim and pillars).
    for (int32 I = 0; I < 16; ++I)
        for (const float R : {300.f, 1500.f, 2450.f, Radius - 150.f})
        {
            const FVector P = Polar(11.25f + 22.5f * I, R);
            FHitResult Floor;
            if (!GetWorld()->LineTraceSingleByChannel(Floor, P + FVector(0, 0, 400), P - FVector(0, 0, 400), ECC_Visibility, Query)
                || Floor.ImpactNormal.Z < .9f || FMath::Abs(Floor.ImpactPoint.Z) > (R < PoolRadius ? 30.f : 5.f))
                Fail(TEXT("Floor missing or not level"));
        }
    // Between the arches the wall stops the player; through an arch the ward does, beyond the wall.
    for (int32 I = 0; I < 36; ++I)
    {
        const float A = 10.f * I + 5.f;
        const bool Arch = FMath::Abs(FMath::Fmod(A + 45.f, 90.f) - 45.f) < ArchHalfAngle - .5f;
        FHitResult Block;
        const FVector From = Polar(A, PillarRadius + 400.f, 100.f);
        if (!GetWorld()->LineTraceSingleByChannel(Block, From, Polar(A, Radius + 3000.f, 100.f), ECC_Pawn, Query))
            Fail(TEXT("A direction leads out of the court"));
        else if (!Arch && FVector::Dist2D(Block.ImpactPoint, FVector::ZeroVector) > Radius + 150.f)
            Fail(TEXT("Wall gap outside an arch"));
    }
    for (int32 Arch = 0; Arch < 4; ++Arch)
    {
        FHitResult Block;
        GetWorld()->LineTraceSingleByChannel(Block, Polar(90.f * Arch, PillarRadius + 400.f, 100.f), Polar(90.f * Arch, Radius + 3000.f, 100.f), ECC_Pawn, Query);
        if (!Block.GetActor() || !Block.GetActor()->ActorHasTag(TEXT("CourtWard"))) Fail(TEXT("Arch is not open up to its ward"));
    }
    int32 PillarCount = 0;
    for (TActorIterator<AActor> It(GetWorld()); It; ++It) PillarCount += It->ActorHasTag(TEXT("CourtPillar"));
    if (PillarCount != 8) Fail(TEXT("Expected eight pillars"));
    // The player starts on the floor at the entrance.
    if (auto* Hero = Cast<AArenaFighter>(UGameplayStatics::GetPlayerPawn(this, 0)))
        if (FVector::Dist2D(Hero->GetActorLocation(), PlayerStart) > 50.f || !Hero->GetCharacterMovement()->IsMovingOnGround()) Fail(TEXT("Player not standing at the entrance"));
    if (Passed) { UE_LOG(LogTemp, Display, TEXT("COURT CHECK PASSED: level floor, closed wall, four open arches sealed by wards, eight pillars, player at the entrance")); }
    else { UE_LOG(LogTemp, Error, TEXT("COURT CHECK FAILED: %s"), *Failure); }
    FPlatformMisc::RequestExitWithStatus(false, Passed ? 0 : 1);
#endif
}
