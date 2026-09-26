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
}
