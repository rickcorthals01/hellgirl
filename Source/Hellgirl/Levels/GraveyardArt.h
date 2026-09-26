#pragma once
#include "CoreMinimal.h"
#include "Rules/GraveyardRules.h"

class UWorld;
class UStaticMesh;
class UMaterialInterface;
class UHierarchicalInstancedStaticMeshComponent;

// Building blocks for the graveyard (defined in GraveyardScenery.cpp). Assets live in /Game/Environment/Graveyard
// (Tools/Environment/build_graveyard_kit.ps1).
namespace GraveArt
{
// A mesh from the generated graveyard kit, e.g. Kit(TEXT("SM_Obelisk")).
UStaticMesh* Kit(const TCHAR* Name);
// The mesh for a planned piece (dead trees come from the forest kit).
UStaticMesh* Mesh(GraveRoom::EPiece Piece);
// A material from the graveyard folder, e.g. Material(TEXT("MI_GravePath")).
UMaterialInterface* Material(const TCHAR* Name);
// Instanced scenery like ForestArt::Batch, optionally blocking movement.
UHierarchicalInstancedStaticMeshComponent* Batch(UWorld* World, UStaticMesh* Mesh, bool Collide, float Sway, float CullDistance = 0.f);
// Height of the ground: flat inside the wall, rising into wooded hills outside it.
float GroundHeight(FVector2D P);
// A flat strip of a tiling material from From to To with ragged edges (world-space UVs).
void Strip(UWorld* World, FVector2D From, FVector2D To, float HalfWidth, float TileSize, UMaterialInterface* Material, float Z, int32 Seed);
// A flat sheet of drifting mist at height Z (M_GraveMist).
void MistSheet(UWorld* World, float Z, float Extent, float TileSize, float Opacity, float Speed, FLinearColor Color = FLinearColor(.16f, .19f, .24f));
}
