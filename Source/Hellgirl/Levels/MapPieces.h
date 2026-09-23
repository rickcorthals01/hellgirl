#pragma once
#include "CoreMinimal.h"

class AActor;
class UWorld;
class UObject;
class UMaterialInstanceDynamic;
class UHierarchicalInstancedStaticMeshComponent;

// Scenery building blocks shared by the code-built maps (defined in MapVisuals.cpp).
namespace MapPieces
{
// Dynamic instance of the environment surface material, or the emissive glow material.
UMaterialInstanceDynamic* Surface(UObject* Owner, FLinearColor Color, bool VertexColor = false, bool Glow = false);
// Irregular rock with a level top (or a rough top when LevelTop is false).
AActor* Rock(UWorld* World, FVector Center, FVector Size, FLinearColor Color, int32 Seed, bool Collision, bool LevelTop = true);
// Instanced, collision-free decoration of one engine basic shape.
UHierarchicalInstancedStaticMeshComponent* DecorationBatch(UWorld* World, const TCHAR* Asset, FLinearColor Color, bool Glow = false);
}
