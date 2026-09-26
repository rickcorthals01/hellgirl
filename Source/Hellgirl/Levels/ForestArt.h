#pragma once
#include "CoreMinimal.h"

class UWorld;
class UStaticMesh;
class UMaterialInterface;
class UHierarchicalInstancedStaticMeshComponent;
class AStaticMeshActor;
class APointLight;

// Building blocks for the dressed forest maps (defined in ForestScenery.cpp).
namespace ForestArt
{
// A mesh from the generated forest kit, e.g. Kit(TEXT("SM_Log")).
UStaticMesh* Kit(const TCHAR* Name);
// One of the seven realistic boulders from Rock_Collection_04 (any index works; it wraps).
UStaticMesh* Boulder(int32 Index);
// Collision-free instanced scenery. Sway >= 0 sets the kit material's sway in cm; CullDistance > 0 fades it out far away.
UHierarchicalInstancedStaticMeshComponent* Batch(UWorld* World, UStaticMesh* Mesh, bool Shadows, float Sway, float CullDistance = 0.f);
// A single mesh actor, optionally blocking.
AStaticMeshActor* Solid(UWorld* World, UStaticMesh* Mesh, const FTransform& Where, bool Collide);
// Like Solid, but stands the mesh with the bottom of its bounds at GroundZ (minus Sink), whatever its pivot.
AStaticMeshActor* Stand(UWorld* World, UStaticMesh* Mesh, FVector2D Where, float GroundZ, float Yaw, float Scale, bool Collide, float Sink = 0.f);
// Height of the forest floor (flat inside the clearing, rising behind the tree line).
float GroundHeight(FVector2D P);
void Ground(UWorld* World, float Extent, float TileSize, UMaterialInterface* Material);
// The same with another map's height function.
void Ground(UWorld* World, float Extent, float TileSize, UMaterialInterface* Material, TFunctionRef<float(FVector2D)> Height);
void NightSky(UWorld* World, FVector MoonDirection);
// Moon, fill and sky light, volumetric fog and colour grading for a moonlit night.
void Moonlight(UWorld* World, FRotator MoonLight, float Fog);
void Fireflies(UWorld* World, FVector Where, float Scale);
// A prop from Inferno_World_Free, e.g. Inferno(TEXT("Props/SM_Brazier_002")).
UStaticMesh* Inferno(const TCHAR* Path);
// A looping stylised fire (Stylish_Fire_VFX: NS_Stylish_Fire_1..4) and rising embers.
void Fire(UWorld* World, FVector Where, float Scale, const TCHAR* System);
void Embers(UWorld* World, FVector Where, float Scale);
// A flat, round patch of a tiling material (UVs in world units, so it tiles instead of stretching).
void Disc(UWorld* World, FVector Center, float Radius, float TileSize, UMaterialInterface* Material);
APointLight* PointGlow(UWorld* World, FVector Where, FLinearColor Color, float Intensity, float Radius);
}
